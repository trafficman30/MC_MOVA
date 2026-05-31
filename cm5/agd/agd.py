#!/usr/bin/env python3
# agd650_simulator.py
# Simulates an AGD650 ZeroMQ publisher for testing the XKOP gateway.
#
# Publishes JSON frames at ~6.7fps (150ms) on tcp://*:9003
# Each zone has independent random detect/clear behaviour.

import zmq
import json
import time
import random
import argparse
from datetime import datetime

# ====================== CONFIG ======================
ZMQ_PORT    = 9003
FRAME_MS    = 150       # ~6.7fps
NUM_ZONES   = 16
SERIAL      = "SIM001-0001"

# Per-zone: probability of transitioning state each frame
P_GO_DETECT  = 0.04    # chance of a clear zone going to detect
P_GO_CLEAR   = 0.08    # chance of a detect zone going to clear

CLASSES = ["car", "pedestrian", "bus", "lorry", "cyclist"]

# Weighted so cars are most common
CLASS_WEIGHTS = [0.55, 0.20, 0.10, 0.10, 0.05]

# Max road-users to put in a zone when detect starts
MAX_USERS = 3

# ====================== STATE ======================
# Each zone: {"state": 0|1|2, "roadusers": [...]}
zone_states = [{"state": 2, "roadusers": []} for _ in range(NUM_ZONES)]


def random_roaduser(track_id: int) -> dict:
    return {
        "H": round(random.uniform(0.05, 0.40), 6),
        "W": round(random.uniform(0.05, 0.40), 6),
        "X": round(random.uniform(0.05, 0.95), 6),
        "Y": round(random.uniform(0.05, 0.95), 6),
        "className": random.choices(CLASSES, weights=CLASS_WEIGHTS)[0],
        "trackID": track_id
    }


def tick_zone(zone: dict, next_track_id: int) -> tuple[dict, int]:
    """Advance one zone by one frame. Returns updated zone and next trackID."""
    state = zone["state"]

    if state == 2:  # non-detect → maybe go to detect
        if random.random() < P_GO_DETECT:
            count = random.randint(1, MAX_USERS)
            users = []
            for _ in range(count):
                users.append(random_roaduser(next_track_id))
                next_track_id += 1
            return {"state": 1, "roadusers": users}, next_track_id

    elif state == 1:  # detect → maybe go to non-detect, or shift users slightly
        if random.random() < P_GO_CLEAR:
            return {"state": 2, "roadusers": []}, next_track_id
        else:
            # Jitter bounding boxes slightly to simulate tracking
            for ru in zone["roadusers"]:
                ru["X"] = max(0.01, min(0.99, ru["X"] + random.uniform(-0.01, 0.01)))
                ru["Y"] = max(0.01, min(0.99, ru["Y"] + random.uniform(-0.01, 0.01)))
            return zone, next_track_id

    return zone, next_track_id


def build_frame(frame_num: int) -> dict:
    ms_utc = int(time.time() * 1000)
    zones_out = []
    for i, z in enumerate(zone_states):
        zones_out.append({
            "index": i + 1,   # 1-based
            "state": z["state"],
            "roadusers": z["roadusers"]
        })
    return {
        "SN": SERIAL,
        "msUTC": ms_utc,
        "version": 2,
        "zones": zones_out
    }


def print_status():
    detect = [i + 1 for i, z in enumerate(zone_states) if z["state"] == 1]
    classes = {}
    for i, z in enumerate(zone_states):
        if z["state"] == 1:
            for ru in z["roadusers"]:
                classes.setdefault(i + 1, []).append(ru["className"])
    status = f"DETECT: {str(detect) if detect else 'none':<20}"
    if classes:
        detail = " | ".join(f"Z{z}:{','.join(c)}" for z, c in classes.items())
        status += f"  [{detail}]"
    print(f"\r[{datetime.now():%H:%M:%S}] {status}", end="", flush=True)


def main():
    global NUM_ZONES, zone_states, P_GO_DETECT, P_GO_CLEAR

    parser = argparse.ArgumentParser(description="AGD650 ZeroMQ Simulator")
    parser.add_argument("--port",       type=int,   default=ZMQ_PORT,   help="ZMQ port (default 9003)")
    parser.add_argument("--zones",      type=int,   default=NUM_ZONES,  help="Number of zones (default 16)")
    parser.add_argument("--fps",        type=float, default=6.7,        help="Frames per second (default 6.7)")
    parser.add_argument("--p-detect",   type=float, default=P_GO_DETECT,  help="P(clear→detect) per frame")
    parser.add_argument("--p-clear",    type=float, default=P_GO_CLEAR,   help="P(detect→clear) per frame")
    parser.add_argument("--busy",       action="store_true",            help="High activity mode (more detections)")
    args = parser.parse_args()
    NUM_ZONES   = args.zones
    zone_states = [{"state": 2, "roadusers": []} for _ in range(NUM_ZONES)]
    P_GO_DETECT = args.p_detect
    P_GO_CLEAR  = args.p_clear
    frame_interval = 1.0 / args.fps

    if args.busy:
        P_GO_DETECT = 0.15
        P_GO_CLEAR  = 0.05
        print("🔴 Busy mode - high activity")

    context = zmq.Context()
    sock = context.socket(zmq.PUB)
    sock.bind(f"tcp://*:{args.port}")

    print(f"AGD650 Simulator running on tcp://*:{args.port}")
    print(f"  Zones : {NUM_ZONES}  |  FPS: {args.fps}  |  "
          f"P(detect): {P_GO_DETECT}  |  P(clear): {P_GO_CLEAR}")
    print(f"  Serial: {SERIAL}")
    print("Press Ctrl+C to stop\n")

    # Brief pause so subscribers can connect before first frame
    time.sleep(0.5)

    frame_num  = 0
    track_id   = 0

    try:
        while True:
            t0 = time.time()

            # Advance all zones
            for i in range(NUM_ZONES):
                zone_states[i], track_id = tick_zone(zone_states[i], track_id)

            # Build and publish frame
            frame = build_frame(frame_num)
            sock.send(json.dumps(frame).encode())
            frame_num += 1

            print_status()

            # Sleep remainder of frame interval
            elapsed = time.time() - t0
            sleep_for = frame_interval - elapsed
            if sleep_for > 0:
                time.sleep(sleep_for)

    except KeyboardInterrupt:
        print(f"\n\nStopped after {frame_num} frames.")
        sock.close()
        context.term()


if __name__ == "__main__":
    main()