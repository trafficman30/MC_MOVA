# flir/flir_mock_server.py
# Run this separately:
#   python flir_mock_server.py             (default port 8765)
#   python flir_mock_server.py --port 8766 (second camera instance)

import asyncio
import json
import random
import argparse
from datetime import datetime, timezone
from websockets.server import serve

ZONES = [1, 2, 3, 4, 5]
CLASSES = ["CAR", "TRUCK", "BUS", "PEDESTRIAN", "BICYCLE", "VAN"]

async def handler(websocket):
    print("🔥 FLIR Mock Server: Client connected")
    try:
        while True:
            zone = random.choice(ZONES)
            event_type = random.choice(["Presence", "DilemmaZone", "Pedestrian"])
            state = random.choice(["Begin", "End"])

            now = datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")

            event = {
                "messageType": "Event",
                "time": now,
                "type": event_type,
                "zoneId": str(zone),
                "eventNumber": str(random.randint(10000, 99999)),
                "state": state,
                "class": random.choice(CLASSES + [None])
            }
            await websocket.send(json.dumps(event))
            print(f"🧪 Mock sent → Zone {zone} | {event_type} | {state}")
            await asyncio.sleep(random.uniform(0.8, 3.5))
    except Exception:
        pass

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', type=int, default=8765)
    args = parser.parse_args()

    async def _main():
        async with serve(handler, "0.0.0.0", args.port):
            print(f"FLIR Mock WebSocket Server running on ws://0.0.0.0:{args.port}")
            await asyncio.Future()

    asyncio.run(_main())
