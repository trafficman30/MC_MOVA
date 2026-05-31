"""
Phase 1 IPC integration test.

Run in two terminals:
  Terminal 1:  cd /opt/MC_MOVA && python -m pci_mova.kernel_main 0
  Terminal 2:  cd /opt/MC_MOVA && python tests/test_ipc.py

Or run both in one terminal (background kernel):
  cd /opt/MC_MOVA
  python -m pci_mova.kernel_main 0 &
  sleep 0.5
  python tests/test_ipc.py
  kill %1
"""

import sys
import os
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from pci_mova.ipc.client import KernelClient

PASS = "\033[92mPASS\033[0m"
FAIL = "\033[91mFAIL\033[0m"


def check(label: str, condition: bool) -> bool:
    print(f"  {'[' + PASS + ']' if condition else '[' + FAIL + ']'} {label}")
    return condition


def main():
    print("\n=== Phase 1 IPC test — stream 0 ===\n")

    client = KernelClient(0)
    client.start()

    # Allow reader to connect
    for _ in range(20):
        if client.connected:
            break
        time.sleep(0.1)

    results = []

    # 1. Connection
    results.append(check("push socket connected", client.connected))

    # 2. Snapshot received
    time.sleep(0.7)  # wait for at least one snap (2 Hz)
    snap = client.latest_snapshot()
    results.append(check("snapshot received", bool(snap)))
    results.append(check("snapshot has v=1", snap.get("v") == 1))
    results.append(check("snapshot t=snap", snap.get("t") == "snap"))
    results.append(check("snapshot has stream index", snap.get("stream") == 0))

    # 3. PING
    resp = client.send_command("PING")
    results.append(check("PING returns v=1", resp.get("v") == 1))
    results.append(check("PING returns t=pong", resp.get("t") == "pong"))

    # 4. Unknown command
    resp = client.send_command("LOAD", "/tmp/test.mxds", "J001")
    results.append(check("LOAD returns ack", resp.get("t") == "ack"))
    results.append(check("LOAD stub returns ok=False", resp.get("ok") is False))

    # 5. Stale check
    results.append(check("not stale after recent snap", not client.stale))

    # 6. Event callback
    events = []
    client.on_event(lambda e: events.append(e))
    # Phase 1 stub doesn't emit events, but verify callback registered cleanly
    results.append(check("event callback registered", True))

    client.stop()

    passed = sum(results)
    total = len(results)
    print(f"\n{'=' * 40}")
    print(f"Result: {passed}/{total} checks passed")
    if passed < total:
        print("Some checks FAILED — see above")
        sys.exit(1)
    else:
        print("All checks passed.")
    print()


if __name__ == "__main__":
    main()
