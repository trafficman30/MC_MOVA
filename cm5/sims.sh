#!/bin/bash
# /opt/CM5/sims.sh — start/stop demo simulators (FLIR mock + AGD650)
#
# Usage:
#   ./sims.sh start    — start all simulators
#   ./sims.sh stop     — stop all simulators
#   ./sims.sh restart  — stop then start
#   ./sims.sh status   — show running state

PYTHON=/opt/ug405-env/bin/python3
ROOT=/opt/CM5
PID_DIR=/tmp/cm5-sims

SIMS=(
    "flir1|$PYTHON $ROOT/flir/flir_mock_server.py --port 8765|8765"
    "flir2|$PYTHON $ROOT/flir/flir_mock_server.py --port 8766|8766"
    "agd1|$PYTHON $ROOT/agd/agd.py --port 9003 --zones 8|9003"
    "agd2|$PYTHON $ROOT/agd/agd.py --port 9004 --zones 8|9004"
)

mkdir -p "$PID_DIR"

_start_one() {
    local name=$1
    local cmd=$2
    local pidfile="$PID_DIR/$name.pid"

    if [ -f "$pidfile" ] && kill -0 "$(cat $pidfile)" 2>/dev/null; then
        echo "  $name: already running (pid $(cat $pidfile))"
        return
    fi

    $cmd > "$PID_DIR/$name.log" 2>&1 &
    echo $! > "$pidfile"
    sleep 0.3
    if kill -0 "$(cat $pidfile)" 2>/dev/null; then
        echo "  $name: started (pid $(cat $pidfile))"
    else
        echo "  $name: FAILED — check $PID_DIR/$name.log"
    fi
}

_stop_one() {
    local name=$1
    local pidfile="$PID_DIR/$name.pid"

    if [ ! -f "$pidfile" ]; then
        echo "  $name: not running"
        return
    fi

    local pid=$(cat "$pidfile")
    if kill "$pid" 2>/dev/null; then
        echo "  $name: stopped (pid $pid)"
    else
        echo "  $name: already gone"
    fi
    rm -f "$pidfile"
}

_status_one() {
    local name=$1
    local pidfile="$PID_DIR/$name.pid"

    if [ -f "$pidfile" ] && kill -0 "$(cat $pidfile)" 2>/dev/null; then
        echo "  $name: running  (pid $(cat $pidfile))"
    else
        echo "  $name: stopped"
        rm -f "$pidfile"
    fi
}

do_start() {
    echo "Starting simulators..."
    for entry in "${SIMS[@]}"; do
        local name="${entry%%|*}"
        local rest="${entry#*|}"
        local cmd="${rest%%|*}"
        _start_one "$name" "$cmd"
    done
    echo "Done."
}

do_stop() {
    echo "Stopping simulators..."
    for entry in "${SIMS[@]}"; do
        local name="${entry%%|*}"
        local port="${entry##*|}"
        _stop_one "$name"
        # also kill any orphan holding the port (e.g. started outside this script)
        local orphan=$(ss -tlnp | grep ":${port} " | grep -oP 'pid=\K[0-9]+')
        if [ -n "$orphan" ]; then
            kill "$orphan" 2>/dev/null && echo "  $name: killed orphan on port $port (pid $orphan)"
        fi
    done
    echo "Done."
}

do_status() {
    echo "Simulator status:"
    for entry in "${SIMS[@]}"; do
        local name="${entry%%|*}"
        _status_one "$name"
    done
}

case "${1:-status}" in
    start)   do_start ;;
    stop)    do_stop ;;
    restart) do_stop; sleep 1; do_start ;;
    status)  do_status ;;
    *)
        echo "Usage: $0 {start|stop|restart|status}"
        exit 1
        ;;
esac
