# io_conditioner.py
# IO signal conditioner — evaluates logic rules from conditioner.cfg
# and writes results to virt.* signals on the IOBus.
#
# cfg syntax:
#   [IO]
#   virt.N = EXPRESSION
#
# Boolean expressions:
#   virt.0 = xkop.i.101 AND xkop.i.102
#   virt.1 = NOT xkop.i.103
#   virt.2 = virt.0 OR NOT virt.1
#   virt.3 = xkop.i.101 AND NOT xkop.i.102
#
# Function calls:
#   virt.4  = ON_DELAY(xkop.i.101, 5.0)
#   virt.5  = OFF_DELAY(xkop.i.101, 3.0)
#   virt.6  = PULSE(xkop.i.101, 1.5)
#   virt.7  = WATCHDOG(xkop.i.101, 300)
#   virt.8  = MISSING(xkop.i.101, 60)
#   virt.9  = RISING_EDGE(xkop.i.101)
#   virt.10 = FALLING_EDGE(xkop.i.101)
#   virt.11 = LATCH(set=xkop.i.101, reset=xkop.i.102)
#   virt.12 = TOGGLE(xkop.i.101)
#   virt.13 = COUNT(xkop.i.101, n=5, reset=xkop.i.102)
#   virt.14 = SPEED(det1=xkop.i.101, det2=xkop.i.102, distance_m=3.5)
#   virt.15 = COUNT_UP(xkop.i.101)
#   virt.16 = COUNT_DOWN(xkop.i.101, start=10)
#
# Bidirectional counter — COUNT_UP and COUNT_DOWN share one register via OR:
#   virt.17 = COUNT_UP(xkop.i.101) OR COUNT_DOWN(xkop.i.102)
#
# Nesting — inner function calls resolved automatically:
#   virt.18 = PULSE(FALLING_EDGE(xkop.i.101), 5.0)
#   virt.19 = COUNT_UP(RISING_EDGE(xkop.i.5)) OR COUNT_DOWN(FALLING_EDGE(xkop.i.6))
#
# Rules are evaluated in declaration order.
# virt.* outputs can be used as inputs to later rules.

import re
import time
import threading
import logging

log = logging.getLogger('COND')

TICK_INTERVAL = 0.05   # 50ms tick for timer resolution


# ── Base block ────────────────────────────────────────────────────────────────

class Block:
    """Base class for all conditioner blocks."""
    def __init__(self, output):
        self.output = output

    def tick(self, io):
        """Called every TICK_INTERVAL seconds."""
        pass

    def on_change(self, name, value, source, io):
        """Called when a subscribed signal changes."""
        pass


# ── Boolean expression block ──────────────────────────────────────────────────

class ExpressionBlock(Block):
    """
    Evaluates a boolean expression involving AND, OR, NOT.
    virt.0 = xkop.i.101 AND NOT xkop.i.102
    """

    def __init__(self, output, expression, signals):
        super().__init__(output)
        self.expression = expression
        self.signals    = signals   # list of signal names referenced

    def evaluate(self, io):
        expr = self.expression
        # Replace signal names with their values (longest first to avoid partial matches)
        for sig in sorted(self.signals, key=len, reverse=True):
            val = '1' if io.read(sig) else '0'
            expr = expr.replace(sig, val)
        # Replace NOT/AND/OR with Python equivalents
        expr = re.sub(r'\bNOT\b', 'not', expr)
        expr = re.sub(r'\bAND\b', 'and', expr)
        expr = re.sub(r'\bOR\b',  'or',  expr)
        expr = re.sub(r'\bXOR\b', '!=',  expr)
        try:
            result = int(bool(eval(expr)))
            io.write(self.output, result, source='conditioner')
        except Exception as e:
            log.warning("Expression eval error '%s': %s", self.expression, e)

    def on_change(self, name, value, source, io):
        if name in self.signals:
            self.evaluate(io)


# ── Timer blocks ──────────────────────────────────────────────────────────────

class OnDelayBlock(Block):
    """
    Output goes ON only after input has been ON for delay_s seconds.
    Output goes OFF immediately when input goes OFF.
    ON_DELAY(signal, delay_s)
    """
    def __init__(self, output, input_sig, delay_s):
        super().__init__(output)
        self.input   = input_sig
        self.delay   = delay_s
        self._on_since = None

    def on_change(self, name, value, source, io):
        if name != self.input:
            return
        if value:
            self._on_since = time.time()
        else:
            self._on_since = None
            io.write(self.output, 0, source='conditioner')

    def tick(self, io):
        if self._on_since and (time.time() - self._on_since) >= self.delay:
            io.write(self.output, 1, source='conditioner')


class OffDelayBlock(Block):
    """
    Output goes ON immediately when input goes ON.
    Output stays ON for delay_s seconds after input goes OFF.
    OFF_DELAY(signal, delay_s)
    """
    def __init__(self, output, input_sig, delay_s):
        super().__init__(output)
        self.input    = input_sig
        self.delay    = delay_s
        self._off_since = None

    def on_change(self, name, value, source, io):
        if name != self.input:
            return
        if value:
            self._off_since = None
            io.write(self.output, 1, source='conditioner')
        else:
            self._off_since = time.time()

    def tick(self, io):
        if self._off_since and (time.time() - self._off_since) >= self.delay:
            self._off_since = None
            io.write(self.output, 0, source='conditioner')


class PulseBlock(Block):
    """
    On rising edge of input, fire a fixed-length pulse of pulse_s seconds.
    PULSE(signal, pulse_s)
    """
    def __init__(self, output, input_sig, pulse_s):
        super().__init__(output)
        self.input   = input_sig
        self.pulse   = pulse_s
        self._fired_at = None
        self._last_val = 0

    def on_change(self, name, value, source, io):
        if name != self.input:
            return
        if value and not self._last_val:   # rising edge
            self._fired_at = time.time()
            io.write(self.output, 1, source='conditioner')
            log.debug("PULSE %s fired (%.1fs)", self.output, self.pulse)
        self._last_val = value

    def tick(self, io):
        if self._fired_at and (time.time() - self._fired_at) >= self.pulse:
            self._fired_at = None
            io.write(self.output, 0, source='conditioner')


class WatchdogBlock(Block):
    """
    Output goes ON if input stays ON longer than timeout_s seconds.
    (Stuck detector alarm)
    WATCHDOG(signal, timeout_s)
    """
    def __init__(self, output, input_sig, timeout_s):
        super().__init__(output)
        self.input    = input_sig
        self.timeout  = timeout_s
        self._on_since = None

    def on_change(self, name, value, source, io):
        if name != self.input:
            return
        if value:
            self._on_since = time.time()
        else:
            self._on_since = None
            io.write(self.output, 0, source='conditioner')

    def tick(self, io):
        if self._on_since and (time.time() - self._on_since) >= self.timeout:
            if io.read(self.output) == 0:
                log.warning("WATCHDOG %s triggered — %s stuck ON > %ds",
                            self.output, self.input, self.timeout)
            io.write(self.output, 1, source='conditioner')


class MissingBlock(Block):
    """
    Output goes ON if input stays OFF longer than timeout_s seconds.
    (Dead detector / cable fault alarm)
    MISSING(signal, timeout_s)
    """
    def __init__(self, output, input_sig, timeout_s):
        super().__init__(output)
        self.input    = input_sig
        self.timeout  = timeout_s
        self._off_since = time.time()   # start timer on init

    def on_change(self, name, value, source, io):
        if name != self.input:
            return
        if value:
            self._off_since = None
            io.write(self.output, 0, source='conditioner')
        else:
            self._off_since = time.time()

    def tick(self, io):
        if self._off_since and (time.time() - self._off_since) >= self.timeout:
            if io.read(self.output) == 0:
                log.warning("MISSING %s triggered — %s absent > %ds",
                            self.output, self.input, self.timeout)
            io.write(self.output, 1, source='conditioner')


# ── Edge detection blocks ─────────────────────────────────────────────────────

class RisingEdgeBlock(Block):
    """
    Fires a single 50ms pulse on 0→1 transition.
    RISING_EDGE(signal)
    """
    def __init__(self, output, input_sig):
        super().__init__(output)
        self.input     = input_sig
        self._last_val = 0
        self._fired_at = None

    def on_change(self, name, value, source, io):
        if name != self.input:
            return
        if value and not self._last_val:
            self._fired_at = time.time()
            io.write(self.output, 1, source='conditioner')
        self._last_val = value

    def tick(self, io):
        if self._fired_at and (time.time() - self._fired_at) >= TICK_INTERVAL * 2:
            self._fired_at = None
            io.write(self.output, 0, source='conditioner')


class FallingEdgeBlock(Block):
    """
    Fires a single 50ms pulse on 1→0 transition.
    FALLING_EDGE(signal)
    """
    def __init__(self, output, input_sig):
        super().__init__(output)
        self.input     = input_sig
        self._last_val = 0
        self._fired_at = None

    def on_change(self, name, value, source, io):
        if name != self.input:
            return
        if not value and self._last_val:
            self._fired_at = time.time()
            io.write(self.output, 1, source='conditioner')
        self._last_val = value

    def tick(self, io):
        if self._fired_at and (time.time() - self._fired_at) >= TICK_INTERVAL * 2:
            self._fired_at = None
            io.write(self.output, 0, source='conditioner')


# ── Latch / memory blocks ─────────────────────────────────────────────────────

class LatchBlock(Block):
    """
    SR latch. Set by set_sig, held until reset_sig fires.
    Reset dominant (reset wins if both active simultaneously).
    LATCH(set=signal, reset=signal)
    """
    def __init__(self, output, set_sig, reset_sig):
        super().__init__(output)
        self.set_sig   = set_sig
        self.reset_sig = reset_sig

    def on_change(self, name, value, source, io):
        if name == self.reset_sig and value:
            io.write(self.output, 0, source='conditioner')
        elif name == self.set_sig and value:
            if not io.read(self.reset_sig):
                io.write(self.output, 1, source='conditioner')


class ToggleBlock(Block):
    """
    Each rising edge flips the output state.
    TOGGLE(signal)
    """
    def __init__(self, output, input_sig):
        super().__init__(output)
        self.input     = input_sig
        self._last_val = 0

    def on_change(self, name, value, source, io):
        if name != self.input:
            return
        if value and not self._last_val:   # rising edge
            current = io.read(self.output)
            io.write(self.output, 0 if current else 1, source='conditioner')
        self._last_val = value


# ── Counter block ─────────────────────────────────────────────────────────────

class CountBlock(Block):
    """
    Counts rising edges on input. Output fires when count reaches n.
    Optional reset signal clears count.
    COUNT(signal, n=5, reset=signal)
    """
    def __init__(self, output, input_sig, n, reset_sig=None):
        super().__init__(output)
        self.input     = input_sig
        self.n         = n
        self.reset_sig = reset_sig
        self._count    = 0
        self._last_val = 0

    def on_change(self, name, value, source, io):
        if self.reset_sig and name == self.reset_sig and value:
            self._count = 0
            io.write(self.output, 0, source='conditioner')
            log.debug("COUNT %s reset", self.output)
            return

        if name == self.input:
            if value and not self._last_val:   # rising edge
                self._count += 1
                log.debug("COUNT %s: %d/%d", self.output, self._count, self.n)
                if self._count >= self.n:
                    io.write(self.output, 1, source='conditioner')
                    self._count = 0
                    log.debug("COUNT %s reached %d — output fired", self.output, self.n)
            self._last_val = value


# ── Speed calculation block ───────────────────────────────────────────────────

class SpeedBlock(Block):
    """
    Calculates vehicle speed from two detector rising edges and known distance.
    Output is speed in km/h (written as integer to virt signal).
    SPEED(det1=signal, det2=signal, distance_m=3.5)
    """
    def __init__(self, output, det1_sig, det2_sig, distance_m):
        super().__init__(output)
        self.det1       = det1_sig
        self.det2       = det2_sig
        self.distance   = distance_m
        self._t1        = None
        self._last_det1 = 0
        self._last_det2 = 0

    def on_change(self, name, value, source, io):
        if name == self.det1:
            if value and not self._last_det1:   # rising edge det1
                self._t1 = time.time()
            self._last_det1 = value

        elif name == self.det2:
            if value and not self._last_det2:   # rising edge det2
                if self._t1 is not None:
                    dt = time.time() - self._t1
                    if 0.01 < dt < 10.0:   # sanity: 0.01s to 10s
                        speed_ms  = self.distance / dt
                        speed_kph = int(speed_ms * 3.6)
                        io.write(self.output, speed_kph, source='conditioner')
                        log.debug("SPEED %s: %.1f m/s = %d km/h (dt=%.3fs)",
                                  self.output, speed_ms, speed_kph, dt)
                    self._t1 = None
            self._last_det2 = value


# ── Function map ──────────────────────────────────────────────────────────────


class N1CountBlock(Block):
    """
    3-loop N+1 vehicle counter.
    A and C rising edges always count.
    B rising edge counts only if A=0 AND C=0 at both rise and fall of B.

    cfg: virt.20 = N1COUNT(a=xkop.i.1, b=xkop.i.2, c=xkop.i.3)
    """
    def __init__(self, output, a_sig, b_sig, c_sig):
        super().__init__(output)
        self.a_sig      = a_sig
        self.b_sig      = b_sig
        self.c_sig      = c_sig
        self._state     = {a_sig: 0, b_sig: 0, c_sig: 0}
        self._b_eligible = False   # True if B rose with A=0 and C=0

    def on_change(self, name, value, source, io):
        if name not in self._state:
            return
        prev = self._state[name]
        self._state[name] = value

        a = self._state[self.a_sig]
        c = self._state[self.c_sig]

        if name == self.a_sig and value and not prev:
            # A rising edge — always count
            current = io.read(self.output) or 0
            io.write(self.output, current + 1, source='conditioner')
            log.debug("N1COUNT %s: A rising → %d", self.output, current + 1)

        elif name == self.c_sig and value and not prev:
            # C rising edge — always count
            current = io.read(self.output) or 0
            io.write(self.output, current + 1, source='conditioner')
            log.debug("N1COUNT %s: C rising → %d", self.output, current + 1)

        elif name == self.b_sig:
            if value and not prev:
                # B rising — eligible only if A and C both clear
                self._b_eligible = (a == 0 and c == 0)
            elif not value and prev:
                # B falling — count if was eligible and A/C still clear
                if self._b_eligible and a == 0 and c == 0:
                    current = io.read(self.output) or 0
                    io.write(self.output, current + 1, source='conditioner')
                    log.debug("N1COUNT %s: B isolated → %d", self.output, current + 1)
                self._b_eligible = False


class FactorBlock(Block):
    """
    Extract a single bit from an integer virt signal.
    Output is 0 or 1 depending on whether bit N of the source is set.

    cfg: virt.21 = FACTOR(virt.20, bit=0)   # 1s
         virt.22 = FACTOR(virt.20, bit=1)   # 2s
         virt.23 = FACTOR(virt.20, bit=2)   # 4s
         virt.24 = FACTOR(virt.20, bit=3)   # 8s
    """
    def __init__(self, output, source_sig, bit):
        super().__init__(output)
        self.source_sig = source_sig
        self.bit        = int(bit)

    def on_change(self, name, value, source, io):
        if name != self.source_sig:
            return
        result = 1 if (int(value or 0) >> self.bit) & 1 else 0
        io.write(self.output, result, source='conditioner')
        log.debug("FACTOR %s: bit%d of %s(%d) → %d",
                  self.output, self.bit, self.source_sig, int(value or 0), result)


class CountUpBlock(Block):
    """
    Increments an integer counter on every rising edge of input.
    Output is the running total — use FACTOR to extract individual bits.
    COUNT_UP(signal)
    COUNT_UP(signal, reset=signal)
    """
    def __init__(self, output, input_sig, reset_sig=None):
        super().__init__(output)
        self.input_sig  = input_sig
        self.reset_sig  = reset_sig
        self._last_val  = 0

    def on_change(self, name, value, source, io):
        if self.reset_sig and name == self.reset_sig and value:
            io.write(self.output, 0, source='conditioner')
            log.debug("COUNT_UP %s reset", self.output)
            return
        if name == self.input_sig:
            if value and not self._last_val:   # rising edge
                current = io.read(self.output) or 0
                io.write(self.output, current + 1, source='conditioner')
                log.debug("COUNT_UP %s → %d", self.output, current + 1)
            self._last_val = value


class CountDownBlock(Block):
    """
    Decrements an integer counter on every rising edge of input. Floors at 0.
    COUNT_DOWN(signal, start=N)
    COUNT_DOWN(signal, start=N, reset=signal)
    """
    def __init__(self, output, input_sig, start=0, reset_sig=None):
        super().__init__(output)
        self.input_sig  = input_sig
        self.start      = int(start)
        self.reset_sig  = reset_sig
        self._last_val  = 0

    def on_change(self, name, value, source, io):
        if self.reset_sig and name == self.reset_sig and value:
            io.write(self.output, self.start, source='conditioner')
            log.debug("COUNT_DOWN %s reset to %d", self.output, self.start)
            return
        if name == self.input_sig:
            if value and not self._last_val:   # rising edge
                current = io.read(self.output) or 0
                new_val = max(0, current - 1)
                io.write(self.output, new_val, source='conditioner')
                log.debug("COUNT_DOWN %s → %d", self.output, new_val)
            self._last_val = value


class BiDirCounterBlock(Block):
    """
    Bidirectional counter — any number of COUNT_UP and COUNT_DOWN inputs share one register.
    All inputs trigger on rising edge only. Counter floors at 0.

    virt.5 = COUNT_UP(xkop.i.5) OR COUNT_DOWN(xkop.i.6)
    virt.5 = COUNT_UP(RISING_EDGE(xkop.i.5)) OR COUNT_DOWN(FALLING_EDGE(xkop.i.6))
    """
    def __init__(self, output, up_sigs, down_sigs):
        super().__init__(output)
        self.up_sigs   = list(up_sigs)
        self.down_sigs = list(down_sigs)
        self._last     = {}

    def on_change(self, name, value, source, io):
        if name not in self.up_sigs and name not in self.down_sigs:
            return
        prev = self._last.get(name, 0)
        self._last[name] = value
        if not (value and not prev):   # only rising edge
            return
        current = io.read(self.output) or 0
        if name in self.up_sigs:
            io.write(self.output, current + 1, source='conditioner')
            log.debug("COUNTER %s UP → %d", self.output, current + 1)
        else:
            new_val = max(0, current - 1)
            io.write(self.output, new_val, source='conditioner')
            log.debug("COUNTER %s DOWN → %d", self.output, new_val)

FUNCTION_MAP = {
    'ON_DELAY':    OnDelayBlock,
    'OFF_DELAY':   OffDelayBlock,
    'PULSE':       PulseBlock,
    'ONE_SHOT':    PulseBlock,       # alias
    'WATCHDOG':    WatchdogBlock,
    'MISSING':     MissingBlock,
    'RISING_EDGE': RisingEdgeBlock,
    'FALLING_EDGE':FallingEdgeBlock,
    'LATCH':       LatchBlock,
    'TOGGLE':      ToggleBlock,
    'COUNT':       CountBlock,
    'SPEED':       SpeedBlock,
    'N1COUNT':     N1CountBlock,
    'COUNT_UP':    CountUpBlock,
    'ACCUMULATE':  CountUpBlock,     # alias
    'COUNT_DOWN':  CountDownBlock,
    'FACTOR':      FactorBlock,
}


# ── Rule parser ───────────────────────────────────────────────────────────────

SIGNAL_RE      = re.compile(r'[a-z_]+\.[io]\.\d+|virt\.\d+|_cond\.\d+')
FUNC_RE        = re.compile(r'^([A-Z_]+)\((.+)\)$', re.DOTALL)
COUNTER_KW_RE  = re.compile(r'^\s*(COUNT_UP|COUNT_DOWN)\s*\(')

_inner_idx = 0   # auto-increment for nested inner signal names


def _split_args(s):
    """Split args string by commas at parenthesis depth 0."""
    parts, depth, buf = [], 0, []
    for ch in s:
        if ch == '(':
            depth += 1; buf.append(ch)
        elif ch == ')':
            depth -= 1; buf.append(ch)
        elif ch == ',' and depth == 0:
            parts.append(''.join(buf).strip()); buf = []
        else:
            buf.append(ch)
    if buf:
        parts.append(''.join(buf).strip())
    return [p for p in parts if p]


def _split_by_or(s):
    """Split expression by top-level OR tokens (respects nested parentheses)."""
    parts, depth, buf = [], 0, []
    i = 0
    while i < len(s):
        if s[i] == '(':
            depth += 1; buf.append(s[i]); i += 1
        elif s[i] == ')':
            depth -= 1; buf.append(s[i]); i += 1
        elif depth == 0 and s[i:i+2] == 'OR' and \
             (i == 0 or not (s[i-1].isalnum() or s[i-1] == '_')) and \
             (i+2 >= len(s) or not (s[i+2].isalnum() or s[i+2] == '_')):
            parts.append(''.join(buf).strip()); buf = []; i += 2
        else:
            buf.append(s[i]); i += 1
    if buf:
        parts.append(''.join(buf).strip())
    return [p for p in parts if p]


def _is_counter_expr(expr):
    """True if expr consists of COUNT_UP/COUNT_DOWN terms connected by top-level OR."""
    parts = _split_by_or(expr)
    return len(parts) > 1 and all(COUNTER_KW_RE.match(p) for p in parts)


def _resolve_arg(arg, pre_blocks, io):
    """
    If arg is a nested function call, allocate an internal _cond.N signal,
    create the inner block (marked hidden), append to pre_blocks, and return
    the inner signal name. Otherwise return arg unchanged.
    """
    global _inner_idx
    arg = arg.strip()
    m = FUNC_RE.match(arg)
    if not m:
        return arg
    inner_out = f'_cond.{_inner_idx}'
    _inner_idx += 1
    try:
        io.register(inner_out, 'conditioner')
    except ValueError:
        pass
    inner_block = _parse_function(m.group(1), m.group(2), inner_out, pre_blocks, io)
    if inner_block:
        inner_block.hidden = True
        pre_blocks.append(inner_block)
    return inner_out


def _parse_counter_expr(expr, output, all_blocks, io):
    """Parse COUNT_UP(...) OR COUNT_DOWN(...) [OR ...] into a BiDirCounterBlock."""
    up_sigs, down_sigs, pre = [], [], []
    for part in _split_by_or(expr):
        m = re.match(r'^\s*(COUNT_UP|COUNT_DOWN)\s*\((.+)\)$', part.strip(), re.DOTALL)
        if not m:
            log.error("Counter expr parse error: '%s'", part)
            return None
        direction = m.group(1)
        sig = _resolve_arg(m.group(2).strip(), pre, io)
        (up_sigs if direction == 'COUNT_UP' else down_sigs).append(sig)
    all_blocks.extend(pre)
    return BiDirCounterBlock(output, up_sigs, down_sigs)


def parse_rules(rules, io):
    """
    Parse list of {writer, expression} dicts into Block instances.
    Registers output signals with IOBus.
    Returns list of Block instances in declaration order.
    """
    blocks = []

    for rule in rules:
        writer = rule['writer']
        expr   = rule['expression'].strip()

        try:
            io.register(writer, 'conditioner')
        except ValueError as e:
            log.error("Rule conflict: %s", e)
            continue

        # Bidirectional counter: COUNT_UP(...) OR COUNT_DOWN(...) [OR ...]
        if _is_counter_expr(expr):
            block = _parse_counter_expr(expr, writer, blocks, io)
            if block:
                block._orig_expr = expr
                blocks.append(block)
                log.info("Rule: %s = %s", writer, expr)
            continue

        # Single function call (possibly with nested args)
        m = FUNC_RE.match(expr)
        if m:
            func_name = m.group(1)
            args_str  = m.group(2)
            block = _parse_function(func_name, args_str, writer, blocks, io)
            if block:
                block._orig_expr = expr
                blocks.append(block)
                log.info("Rule: %s = %s(...)", writer, func_name)
            continue

        # Boolean expression
        signals = SIGNAL_RE.findall(expr)
        block   = ExpressionBlock(writer, expr, signals)
        block._orig_expr = expr
        blocks.append(block)
        log.info("Rule: %s = %s", writer, expr)

    return blocks


def _parse_function(func_name, args_str, output, _blocks=None, _io=None):
    """Parse function arguments and instantiate the correct Block."""
    cls = FUNCTION_MAP.get(func_name)
    if cls is None:
        log.error("Unknown function: %s — skipped", func_name)
        return None

    pre = []   # inner blocks created by nested arg resolution
    positional, kwargs = [], {}

    for part in _split_args(args_str):
        # Keyword arg? key must be lowercase/underscore
        eq = part.find('=')
        if eq > 0:
            key = part[:eq].strip()
            val = part[eq+1:].strip()
            if re.match(r'^[a-z_]+$', key):
                if _io and FUNC_RE.match(val):
                    kwargs[key] = _resolve_arg(val, pre, _io)
                else:
                    try:    kwargs[key] = float(val) if '.' in val else int(val)
                    except: kwargs[key] = val
                continue
        # Positional — may be nested function, signal, or literal
        if _io and FUNC_RE.match(part):
            positional.append(_resolve_arg(part, pre, _io))
        else:
            try:    positional.append(float(part) if '.' in part else int(part))
            except: positional.append(part)

    # Prepend resolved inner blocks so they appear before the outer block
    if _blocks is not None:
        for b in pre:
            _blocks.append(b)

    try:
        if func_name in ('ON_DELAY', 'OFF_DELAY', 'PULSE', 'ONE_SHOT',
                          'WATCHDOG', 'MISSING'):
            return cls(output, positional[0], positional[1])

        elif func_name in ('RISING_EDGE', 'FALLING_EDGE', 'TOGGLE'):
            return cls(output, positional[0])

        elif func_name == 'LATCH':
            return cls(output,
                       set_sig   = kwargs.get('set',   positional[0] if positional else None),
                       reset_sig = kwargs.get('reset', positional[1] if len(positional)>1 else None))

        elif func_name == 'COUNT':
            return cls(output,
                       input_sig = positional[0],
                       n         = int(kwargs.get('n', positional[1] if len(positional)>1 else 1)),
                       reset_sig = kwargs.get('reset'))

        elif func_name in ('COUNT_UP', 'ACCUMULATE'):
            return cls(output,
                       input_sig = positional[0] if positional else kwargs.get('input'),
                       reset_sig = kwargs.get('reset'))

        elif func_name == 'COUNT_DOWN':
            return cls(output,
                       input_sig = positional[0] if positional else kwargs.get('input'),
                       start     = int(kwargs.get('start', 0)),
                       reset_sig = kwargs.get('reset'))

        elif func_name == 'N1COUNT':
            return cls(output,
                       a_sig = kwargs.get('a', positional[0] if positional else None),
                       b_sig = kwargs.get('b', positional[1] if len(positional)>1 else None),
                       c_sig = kwargs.get('c', positional[2] if len(positional)>2 else None))

        elif func_name == 'FACTOR':
            return cls(output,
                       source_sig = positional[0],
                       bit        = int(kwargs.get('bit', positional[1] if len(positional)>1 else 0)))

        elif func_name == 'SPEED':
            return cls(output,
                       det1_sig   = kwargs.get('det1', positional[0] if positional else None),
                       det2_sig   = kwargs.get('det2', positional[1] if len(positional)>1 else None),
                       distance_m = float(kwargs.get('distance_m',
                                          positional[2] if len(positional)>2 else 1.0)))

    except Exception as e:
        log.error("Failed to create %s block: %s", func_name, e)
        return None


# ── IOConditioner service ─────────────────────────────────────────────────────

class IOConditioner:
    """
    Runs all conditioner blocks.
    Subscribes to IOBus and dispatches changes to relevant blocks.
    Runs a tick loop for timer-based blocks.

    Parameters
    ----------
    io    : IOBus
    rules : list of {writer, expression} from config
    """

    def __init__(self, io, rules):
        self.io     = io
        self.blocks = parse_rules(rules, io)
        self._lock  = threading.RLock()

        log.info("Conditioner: %d rules loaded", len(self.blocks))

        io.subscribe(self._on_bus_change)

    def _on_bus_change(self, name, value, source):
        with self._lock:
            for block in self.blocks:
                if source == 'conditioner' and getattr(block, 'output', None) == name:
                    continue
                block.on_change(name, value, source, self.io)

    def _tick_loop(self):
        while True:
            time.sleep(TICK_INTERVAL)
            with self._lock:
                for block in self.blocks:
                    block.tick(self.io)

    def reload(self, rules):
        """Hot-reload conditioner rules without restarting.
        Cancels active timers, replaces blocks, re-evaluates immediately.
        """
        log.info("Conditioner reloading %d rules...", len(rules))
        for block in self.blocks:
            if hasattr(block, '_timer') and block._timer:
                try: block._timer.cancel()
                except: pass

        old_sigs = {name for name, owner in self.io.registered_signals()
                    if owner == 'conditioner'}

        self.blocks = parse_rules(rules, self.io)

        new_sigs = {b.output for b in self.blocks}
        for sig in old_sigs - new_sigs:
            try:
                self.io.write(sig, 0, source='conditioner')
            except Exception:
                pass
            self.io.unregister(sig, 'conditioner')
            log.debug("Conditioner: unregistered orphan %s", sig)

        self._evaluate_all()
        log.info("Conditioner reloaded — %d blocks active", len(self.blocks))

    def _evaluate_all(self):
        """Force-evaluate all blocks against current bus state.
        Called at startup and after reload so outputs reflect current inputs immediately.
        """
        for block in self.blocks:
            # For expression blocks, evaluate directly
            if hasattr(block, 'evaluate'):
                block.evaluate(self.io)
            # For all blocks, call on_change for each registered input signal
            # by reading current values and firing synthetic change events
            for name, owner in self.io.registered_signals():
                val = self.io.read(name)
                try:
                    block.on_change(name, val, 'init', self.io)
                except Exception:
                    pass
            # Also run tick to handle timer-based initial state
            block.tick(self.io)

    def start(self):
        threading.Thread(target=self._tick_loop, daemon=True).start()
        log.info("Conditioner started (%d blocks, %.0fms tick)",
                 len(self.blocks), TICK_INTERVAL * 1000)


# ── Standalone test ───────────────────────────────────────────────────────────

if __name__ == '__main__':
    import sys, os
    sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'core'))

    logging.basicConfig(
        level=logging.DEBUG,
        format='%(asctime)s [%(name)-8s] %(levelname)-5s %(message)s',
        datefmt='%H:%M:%S'
    )

    from io_bus import IOBus
    from config import load_conditioner

    cfg_path = os.path.join(os.path.dirname(__file__), 'conditioner.cfg')
    cfg = load_conditioner(cfg_path)

    io = IOBus()
    for name, owner in cfg['signals'].items():
        io.register(name, owner)

    def on_change(name, value, source):
        if name.startswith('virt.'):
            print(f"  → {name} = {value}  (src={source})")

    io.subscribe(on_change)

    cond = IOConditioner(io, cfg['rules'])
    cond.start()

    print(f"Conditioner running — {len(cfg['rules'])} rules")
    print("Edit conditioner.cfg rules and restart to test")

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("Stopped")
