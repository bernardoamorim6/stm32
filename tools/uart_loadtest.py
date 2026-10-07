#!/usr/bin/env python3
"""Load test for the interrupt driven UART echo on the NUCLEO-C031C6.

Sends a few kilobytes of a known pattern and checks that every byte comes back
in the same order. A ten byte test proves nothing: the races this is looking
for only appear once the 63 byte rings have filled and wrapped hundreds of
times, with transmit and receive both busy.

Standard library only, so no pyserial needed. stty does the port setup.

    python3 tools/uart_loadtest.py
    python3 tools/uart_loadtest.py --bytes 16384 --chunk 1
"""

import argparse
import os
import select
import subprocess
import sys
import threading
import time


def configure_port(port, baud):
    subprocess.run(
        ["stty", "-F", port, str(baud), "raw", "-echo", "-echoe", "-echok",
         "cs8", "-parenb", "-cstopb", "-crtscts", "-ixon", "-ixoff"],
        check=True,
    )


def drain(fd, seconds=0.3):
    """Throw away anything already queued, such as the reset banner."""
    deadline = time.monotonic() + seconds
    junk = bytearray()
    while time.monotonic() < deadline:
        ready, _, _ = select.select([fd], [], [], 0.05)
        if ready:
            junk += os.read(fd, 4096)
    return bytes(junk)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/ttyACM0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--bytes", type=int, default=4096,
                    help="how many bytes to push through")
    ap.add_argument("--chunk", type=int, default=64,
                    help="write size; 1 stresses the per byte path hardest")
    ap.add_argument("--timeout", type=float, default=15.0)
    args = ap.parse_args()

    configure_port(args.port, args.baud)
    fd = os.open(args.port, os.O_RDWR | os.O_NOCTTY)

    try:
        banner = drain(fd)
        if banner:
            print(f"discarded {len(banner)} byte(s) already queued: {banner!r}")

        # 251 is prime and under 256, so the pattern never lines up with the
        # 64 byte ring. A block swapped or repeated shows up as a mismatch
        # rather than hiding behind an identical byte.
        payload = bytes((i % 251) for i in range(args.bytes))

        received = bytearray()
        done = threading.Event()
        timeline = []          # (seconds since start, total bytes received)
        started_holder = []

        def reader():
            deadline = time.monotonic() + args.timeout
            t0 = started_holder[0] if started_holder else time.monotonic()
            while len(received) < len(payload) and time.monotonic() < deadline:
                ready, _, _ = select.select([fd], [], [], 0.1)
                if ready:
                    chunk = os.read(fd, 4096)
                    if chunk:
                        received.extend(chunk)
                        timeline.append((time.monotonic() - t0, len(received)))
            done.set()

        t = threading.Thread(target=reader, daemon=True)
        t.start()

        print(f"sending {len(payload)} bytes in {args.chunk} byte writes at {args.baud} baud")
        started = time.monotonic()
        started_holder.append(started)
        sent = 0
        while sent < len(payload):
            sent += os.write(fd, payload[sent:sent + args.chunk])

        done.wait(args.timeout + 1)
        elapsed = time.monotonic() - started

        print(f"sent     {sent} bytes")
        print(f"received {len(received)} bytes")

        # Averaging over the whole timeout hides the difference between a slow
        # link and one that worked then stopped. These two numbers separate
        # them: if the last byte landed long before the timeout, it stalled.
        if timeline:
            first_t, _ = timeline[0]
            last_t, _ = timeline[-1]
            active = last_t - first_t
            print(f"first byte back at  T+{first_t:.3f} s")
            print(f"last  byte back at  T+{last_t:.3f} s")
            if active > 0:
                print(f"rate while flowing  {(len(received) - timeline[0][1]) / active:.0f} B/s")
            if last_t < args.timeout - 1.0:
                print(f"STALLED: nothing more arrived for the last "
                      f"{elapsed - last_t:.1f} s")
            print("progress (seconds, bytes):")
            step = max(1, len(timeline) // 12)
            for t_, n_ in timeline[::step]:
                print(f"   T+{t_:6.3f}  {n_}")
            if timeline[-1] not in timeline[::step]:
                print(f"   T+{timeline[-1][0]:6.3f}  {timeline[-1][1]}")

        if len(received) != len(payload):
            missing = len(payload) - len(received)
            print(f"\nFAIL: {missing} byte(s) never came back")
            if received:
                idx = next((i for i, (a, b) in enumerate(zip(payload, received)) if a != b), None)
                if idx is not None:
                    print(f"      first mismatch at offset {idx}")
            return 1

        if bytes(received) != payload:
            idx = next(i for i, (a, b) in enumerate(zip(payload, received)) if a != b)
            lo = max(0, idx - 8)
            hi = min(len(payload), idx + 8)
            print(f"\nFAIL: data came back corrupted or reordered at offset {idx}")
            print(f"      sent     {payload[lo:hi].hex(' ')}")
            print(f"      received {bytes(received[lo:hi]).hex(' ')}")
            return 1

        print(f"\nPASS: all {len(payload)} bytes returned in order")
        return 0

    finally:
        os.close(fd)


if __name__ == "__main__":
    sys.exit(main())
