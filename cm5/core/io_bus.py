# io_bus.py
# Typed address space IO bus.
#
# Signal naming:  driver.direction.index  or  virt.index
# Examples:       xkop.i.101  xkop.o.101  gp.i.0  virt.0
#
# Address map:
#   xkop.i.0-255         slots 0-255
#   xkop.o.0-255         slots 256-511
#   gp.i.0-127           slots 512-639
#   gp.o.0-127           slots 640-767
#   virt.0-511           slots 768-1279
#   cm5.i.0-127          slots 1280-1407
#   cm5.o.0-127          slots 1408-1535
#   modb.i.0-255         slots 1536-1791
#   modb.o.0-255         slots 1792-2047
#   rpdb.i.xsg.0-63      slots 2048-2111
#   rpdb.i.xdet.0-255    slots 2112-2367
#   rpdb.i.xin.0-255     slots 2368-2623
#   rpdb.i.xout.0-255    slots 2624-2879
#   rpdb.i.stg.0-63      slots 2880-2943
#   rpdb.i.mode.0-31     slots 2944-2975
#   rpdb.o.xout.0-255    slots 3072-3327
#
# Rules:
#   Each signal has exactly ONE writer (enforced at startup)
#   Any signal can have many readers
#   Undeclared signals do not exist
#   Conflict at startup = hard error, will not run

import threading

# ── Address space definition ──────────────────────────────────────────────────

NAMESPACES = {
    'xkop': {
        'i': (0,    255),
        'o': (256,  511),
    },
    'gp': {
        'i': (512,  639),
        'o': (640,  767),
    },
    'virt': {
        None: (768, 1279),   # no direction
    },
    'cm5': {
        'i': (1280, 1407),
        'o': (1408, 1535),
    },
    'modb': {
        'i': (1536, 1791),
        'o': (1792, 2047),
    },
    'rpdb': {
        'i': (2048, 2975),
        'o': (3072, 3327),
        'subs_i': {
            'xsg':  2048,
            'xdet': 2112,
            'xin':  2368,
            'xout': 2624,
            'stg':  2880,
            'mode': 2944,
        },
        'subs_o': {
            'xout': 3072,
        },
    },
    'mova': {
        'i': (3584, 3839),   # 256 slots — 32 per stream × 8 streams
        'o': (3840, 4863),   # 1024 slots — 128 per stream × 8 streams
        'stride_i': 32,      # slots per stream (inputs)
        'stride_o': 128,     # slots per stream (outputs)
        'subs_i': {          # offset within stream block
            'force':  0,     # 0-9   (10 force bits)
            'to':     10,    # 1     (take over)
            'spo':    11,    # 11-18 (8 special outputs)
            'fault':  19,    # 1     (fault bit)
            'status': 20,    # status code
            'stage':  21,    # current stage
            'on':     22,    # on control flag
        },
        'subs_o': {          # offset within stream block
            'det':    0,     # 0-63  (64 detectors)
            'conf':   64,    # 64-95 (32 confirms)
            'crb':    96,    # 1     (controller ready)
        },
    },
}
BUS_SIZE = 3328


def resolve(name):
    """
    Resolve a signal name to an internal slot index.
    'xkop.i.101' → 101
    'xkop.o.101' → 357
    'gp.i.0'     → 512
    'virt.0'     → 768
    """
    parts = name.strip().split('.')
    if len(parts) == 2:
        driver, idx = parts
        direction = None
        sub = None
    elif len(parts) == 3:
        driver, direction, idx = parts
        sub = None
    elif len(parts) == 4:
        driver, direction, sub, idx = parts
    elif len(parts) == 5:
        # mova.i.force.S.N — driver.direction.sub.stream.element
        driver, direction, sub, idx = parts[0], parts[1], parts[2], parts[3]
    else:
        raise ValueError(f"Invalid signal name: '{name}'")

    if driver not in NAMESPACES:
        raise ValueError(f"Unknown driver namespace: '{driver}'")

    ns = NAMESPACES[driver]

    if sub is not None:
        subs_key = f'subs_{direction}'
        if subs_key not in ns:
            raise ValueError(f"Driver '{driver}' has no sub-namespaces for '{direction}'")
        subs = ns[subs_key]
        if sub not in subs:
            raise ValueError(f"Unknown sub-namespace '{sub}' for '{driver}.{direction}'")
        base_offset = subs[sub]

        # MOVA: 5-part signal mova.i.force.S.N or mova.o.det.S.N
        # sub=force/det etc, idx=stream, extra idx in name
        if driver == 'mova':
            stream_id   = int(idx)
            element_idx = int(parts[4]) if len(parts) > 4 else 0
            stride_key  = f'stride_{direction}'
            stride      = ns.get(stride_key, 32)
            lo, hi      = ns[direction]
            slot        = lo + (stream_id * stride) + base_offset + element_idx
            if not (lo <= slot <= hi):
                raise ValueError(f"Signal '{name}' index out of range for {driver}.{direction}")
            return slot
        else:
            # rpdb sub offsets are absolute slot numbers
            slot = base_offset + int(idx)
    else:
        if direction not in ns:
            raise ValueError(f"Unknown direction '{direction}' for driver '{driver}'")
        lo, hi = ns[direction]
        slot = lo + int(idx)

    lo, hi = ns[direction]
    if not (lo <= slot <= hi):
        raise ValueError(
            f"Signal '{name}' index out of range for {driver}.{direction}")

    return slot


def slot_to_name(slot):
    """Reverse lookup — slot index to signal name (for debug/logging)."""
    for driver, directions in NAMESPACES.items():
        for direction, (lo, hi) in directions.items():
            if lo <= slot <= hi:
                idx = slot - lo
                if direction is None:
                    return f"virt.{idx}"
                return f"{driver}.{direction}.{idx}"
    return f"slot.{slot}"


# ── IOBus ─────────────────────────────────────────────────────────────────────

class IOBus:
    """
    Typed sparse IO bus.

    Signals must be registered before use.
    Each signal has exactly one declared writer (owner).
    Any number of readers allowed.
    """

    def __init__(self):
        self._slots    = {}              # slot → value
        self._owners   = {}              # slot → owner name
        self._names    = {}              # slot → signal name
        self._subs     = []              # change subscribers
        self._lock     = threading.Lock()

    # ── Registration ──────────────────────────────────────────────────────────

    def register(self, name, owner):
        """
        Register a signal and declare its owner (writer).
        Called during cfg load / startup.
        Raises ValueError on conflict.
        """
        slot = resolve(name)

        with self._lock:
            if slot in self._owners:
                existing = self._owners[slot]
                if existing != owner:
                    raise ValueError(
                        f"[BUS] CONFLICT: '{name}' (slot {slot}) "
                        f"already owned by '{existing}' — "
                        f"'{owner}' cannot also own it")
                return  # already registered by same owner

            self._slots[slot]  = 0
            self._owners[slot] = owner
            self._names[slot]  = name

    def _lazy_register(self, name, owner):
        """
        Auto-register a signal on first write.
        If already owned by a different owner, returns False (conflict).
        """
        slot = resolve(name)
        with self._lock:
            if slot in self._owners:
                return self._owners[slot] == owner
            self._slots[slot]  = 0
            self._owners[slot] = owner
            self._names[slot]  = name
            return True

    def registered_signals(self):
        """Return list of all registered (name, owner) pairs."""
        with self._lock:
            return [(self._names[s], self._owners[s])
                    for s in sorted(self._slots.keys())]

    def unregister(self, name, owner):
        """Remove a signal registration. No-op if not registered or owned by someone else."""
        slot = resolve(name)
        with self._lock:
            if self._owners.get(slot) != owner:
                return
            del self._owners[slot]
            del self._names[slot]
            del self._slots[slot]

    # ── Read / Write ──────────────────────────────────────────────────────────

    def write(self, name, value, source=None):
        """
        Write a signal value.
        If signal is not registered, auto-registers with source as owner (lazy).
        If already owned by a different source, write is rejected.
        """
        slot = resolve(name)

        with self._lock:
            if slot not in self._slots:
                if source:
                    # Lazy registration — first writer owns it
                    self._slots[slot]  = 0
                    self._owners[slot] = source
                    self._names[slot]  = name
                else:
                    print(f"[BUS] REJECTED write '{name}' — not registered and no source")
                    return False

            owner = self._owners[slot]
            if owner != 'conditioner_read' and source and source != owner:
                print(f"[BUS] REJECTED write '{name}' by '{source}' "
                      f"— owner is '{owner}'")
                return False

            if self._slots[slot] == value:
                return True  # no change

            self._slots[slot] = value

        self._notify(name, slot, value, source or owner)
        return True

    def read(self, name):
        """Read a signal value. Returns 0 silently for unregistered signals."""
        slot = resolve(name)
        with self._lock:
            return self._slots.get(slot, 0)

    def read_slot(self, slot):
        """Read by raw slot index (used internally by drivers)."""
        with self._lock:
            return self._slots.get(slot, 0)

    def write_slot(self, slot, value, source=None):
        """Write by raw slot index (used internally by drivers)."""
        name = slot_to_name(slot)
        return self.write(name, value, source)

    def zero_owned_by(self, owner):
        """Zero all signals owned by a given source (e.g. on opMode drop)."""
        changed = []
        with self._lock:
            for slot, own in self._owners.items():
                if own == owner and self._slots[slot] != 0:
                    self._slots[slot] = 0
                    changed.append((slot, self._names[slot]))

        for slot, name in changed:
            self._notify(name, slot, 0, owner)

    # ── Subscriptions ─────────────────────────────────────────────────────────

    def subscribe(self, callback):
        """
        Register change callback.
        callback(name, value, source)
        """
        with self._lock:
            self._subs.append(callback)

    def unsubscribe(self, callback):
        with self._lock:
            if callback in self._subs:
                self._subs.remove(callback)

    # ── Status ────────────────────────────────────────────────────────────────

    def snapshot(self):
        """Return all non-zero signal values as {name: value}."""
        with self._lock:
            return {self._names[s]: v
                    for s, v in self._slots.items() if v != 0}

    def dump(self):
        """Print all registered signals — for debug."""
        print(f"[BUS] Registered signals ({len(self._slots)}):")
        for slot in sorted(self._slots.keys()):
            name  = self._names[slot]
            owner = self._owners[slot]
            value = self._slots[slot]
            print(f"  slot {slot:4d}  {name:20s}  owner={owner:20s}  val={value}")

    # ── Internal ─────────────────────────────────────────────────────────────

    def _notify(self, name, slot, value, source):
        with self._lock:
            subs = list(self._subs)
        for cb in subs:
            try:
                cb(name, value, source)
            except Exception as e:
                print(f"[BUS] Subscriber error: {e}")


# ── Smoke test ────────────────────────────────────────────────────────────────

if __name__ == '__main__':
    io = IOBus()

    # Register signals
    io.register('xkop.i.101', owner='xkop_driver')
    io.register('xkop.o.101', owner='ug405')
    io.register('virt.0',     owner='conditioner')
    io.register('gp.o.0',     owner='conditioner')

    # Subscribe
    events = []
    io.subscribe(lambda name, val, src: events.append((name, val, src)))

    # Write
    io.write('xkop.i.101', 1, source='xkop_driver')
    io.write('xkop.o.101', 1, source='ug405')
    io.write('xkop.o.101', 1, source='ug405')   # duplicate — no event
    io.write('virt.0',     1, source='conditioner')

    # Conflict test
    try:
        io.register('xkop.i.101', owner='rtig')
    except ValueError as e:
        print(f"\nConflict caught: {e}")

    # Wrong owner test
    io.write('xkop.o.101', 0, source='rtig')   # rejected

    print(f"\nEvents fired: {len(events)}")
    for e in events:
        print(f"  {e}")

    print(f"\nSnapshot: {io.snapshot()}")
    print()
    io.dump()

    # Address resolution tests
    print("\nAddress resolution:")
    for name in ['xkop.i.0', 'xkop.i.255', 'xkop.o.0', 'xkop.o.255',
                 'gp.i.0', 'gp.o.0', 'virt.0', 'virt.511',
                 'cm5.i.0', 'modb.o.255']:
        print(f"  {name:20s} → slot {resolve(name)}")
