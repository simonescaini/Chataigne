"""Loopback-only OSC motor simulation. Standard library; no CAN/serial dependencies."""

from __future__ import annotations

import argparse
import json
import math
import select
import socket
import struct
import time
from dataclasses import dataclass
from datetime import UTC, datetime
from pathlib import Path

PREFIX = "/midnight/sim/"


def _string(text):
    if "\0" in text:
        raise ValueError("OSC strings cannot contain NUL")
    data = text.encode("utf-8") + b"\0"
    return data + b"\0" * (-len(data) % 4)


def encode(address, args=()):
    if not address.startswith("/"):
        raise ValueError("OSC address must begin with /")
    tags, payload = ",", b""
    for arg in args:
        if isinstance(arg, str):
            tags += "s"
            payload += _string(arg)
        elif isinstance(arg, int) and not isinstance(arg, bool):
            tags += "i"
            payload += struct.pack(">i", arg)
        elif isinstance(arg, float) and math.isfinite(arg):
            tags += "f"
            payload += struct.pack(">f", arg)
        else:
            raise ValueError("Only strings, int32 and finite float32 are supported")
    return _string(address) + _string(tags) + payload


def decode(data, depth=0):
    """Decode messages and immediate bundles; no second timed-event scheduler."""
    if depth > 8 or len(data) > 65507:
        raise ValueError("Oversized/deep OSC packet")
    if data.startswith(b"#bundle\0"):
        if len(data) < 16 or data[8:16] != struct.pack(">Q", 1):
            raise ValueError(
                "Only immediate OSC bundles are accepted; Chataigne owns timing"
            )
        offset, messages = 16, []
        while offset < len(data):
            if offset + 4 > len(data):
                raise ValueError("Truncated bundle size")
            size = struct.unpack_from(">i", data, offset)[0]
            offset += 4
            if size <= 0 or offset + size > len(data):
                raise ValueError("Invalid bundle element")
            messages.extend(decode(data[offset : offset + size], depth + 1))
            offset += size
        return messages
    offset = 0

    def string():
        nonlocal offset
        end = data.find(b"\0", offset)
        if end < 0:
            raise ValueError("Unterminated OSC string")
        result = data[offset:end].decode("utf-8")
        padded = (end + 4) & ~3
        if padded > len(data) or any(data[end:padded]):
            raise ValueError("Invalid OSC padding")
        offset = padded
        return result

    address, tags = string(), string()
    if not address.startswith("/") or not tags.startswith(","):
        raise ValueError("Invalid OSC address/type tag")
    args = []
    for tag in tags[1:]:
        if tag == "s":
            args.append(string())
        elif tag in ("i", "f"):
            if offset + 4 > len(data):
                raise ValueError("Truncated OSC argument")
            value = struct.unpack_from(">" + tag, data, offset)[0]
            offset += 4
            if isinstance(value, float) and not math.isfinite(value):
                raise ValueError("Non-finite OSC value")
            args.append(value)
        else:
            raise ValueError("Unsupported OSC type: " + tag)
    if offset != len(data):
        raise ValueError("Trailing bytes in OSC packet")
    return [(address, args)]


@dataclass
class Motor:
    position: float = 0.0
    target: float = 0.0
    speed: float = 90.0
    moving: bool = False

    def advance(self, seconds):
        if seconds < 0 or not math.isfinite(seconds):
            raise ValueError("Invalid timestep")
        if self.moving:
            distance = self.target - self.position
            amount = min(abs(distance), self.speed * seconds)
            self.position += math.copysign(amount, distance)
            self.moving = self.position != self.target


class Simulator:
    def __init__(self):
        self.motors = {"cupboard-1": Motor()}
        self.state = "IDLE"

    def apply(self, address, args):
        if address == PREFIX + "state":
            if len(args) != 1 or args[0] not in ("IDLE", "ARMED", "RUNNING"):
                raise ValueError("state expects one of IDLE, ARMED, RUNNING")
            self.state = args[0]
            return
        if address not in (PREFIX + "move", PREFIX + "stop"):
            raise ValueError("Unsupported simulated action: " + address)
        if not args or not isinstance(args[0], str) or not args[0].strip():
            raise ValueError("Motor identifier must be a nonempty OSC string")
        if address.endswith("/move"):
            if len(args) != 3 or any(
                isinstance(v, bool)
                or not isinstance(v, (int, float))
                or not math.isfinite(v)
                for v in args[1:]
            ):
                raise ValueError(
                    "move expects identifier, finite position, finite speed"
                )
            if args[2] <= 0:
                raise ValueError("Movement speed must be positive")
            motor = self.motors.setdefault(args[0], Motor())
            motor.target, motor.speed, motor.moving = (
                float(args[1]),
                float(args[2]),
                True,
            )
        else:
            if len(args) != 1:
                raise ValueError("stop expects only a motor identifier")
            motor = self.motors.setdefault(args[0], Motor())
            motor.target, motor.moving = motor.position, False

    def advance(self, seconds):
        for motor in self.motors.values():
            motor.advance(seconds)


class Receiver:
    def __init__(self, port=19001, feedback_port=19002, log_path=None):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.bind(("127.0.0.1", port))
        self.port = self.socket.getsockname()[1]
        self.feedback = ("127.0.0.1", feedback_port)
        self.simulator = Simulator()
        self.log = None
        if log_path:
            Path(log_path).parent.mkdir(parents=True, exist_ok=True)
            self.log = Path(log_path).open("a", encoding="utf-8", buffering=1)  # noqa: SIM115 - receiver owns this handle until close()
        self.last_tick = time.perf_counter()
        self.next_feedback = self.last_tick
        self.record("ready", bind=["127.0.0.1", self.port], simulation_only=True)

    def record(self, kind, **fields):
        item = {
            "utc": datetime.now(UTC).isoformat(),
            "monotonic": time.perf_counter(),
            "kind": kind,
            **fields,
        }
        if self.log:
            self.log.write(json.dumps(item, allow_nan=False) + "\n")

    def send(self, address, args):
        self.socket.sendto(encode(address, args), self.feedback)
        self.record("tx", address=address, args=args)

    def step(self, timeout=0.01):
        ready, _, _ = select.select([self.socket], [], [], timeout)
        now = time.perf_counter()
        self.simulator.advance(now - self.last_tick)
        self.last_tick = now
        if ready:
            data, peer = self.socket.recvfrom(65507)
            self.record("packet", peer=list(peer), hex=data.hex())
            try:
                for address, args in decode(data):
                    self.simulator.apply(address, args)
                    self.record("rx", address=address, args=args)
                    self.send(
                        PREFIX + "ack",
                        [
                            json.dumps(
                                {"address": address, "args": args},
                                separators=(",", ":"),
                            )
                        ],
                    )
            except (ValueError, UnicodeError, struct.error) as exc:
                self.record("error", message=str(exc))
                self.send(PREFIX + "error", [str(exc)])
        now = time.perf_counter()
        if now >= self.next_feedback:
            self.next_feedback = now + 0.05
            for name, motor in self.simulator.motors.items():
                self.send(PREFIX + "feedback", [name, motor.position, motor.target])

    def close(self):
        self.record("closed")
        self.socket.close()
        if self.log:
            self.log.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=19001)
    parser.add_argument("--feedback-port", type=int, default=19002)
    parser.add_argument("--log", default="Midnight/runtime/receiver.jsonl")
    parser.add_argument(
        "--seconds",
        type=float,
        default=0,
        help="Exit after this many seconds; 0 runs until Ctrl+C",
    )
    args = parser.parse_args()
    if not 0 <= args.port <= 65535 or not 1 <= args.feedback_port <= 65535:
        parser.error("Invalid UDP port")
    try:
        receiver = Receiver(args.port, args.feedback_port, args.log)
    except OSError as exc:
        parser.exit(2, f"Receiver could not bind: {exc}\n")
    print(
        f"SIMULATION ONLY: listening on 127.0.0.1:{receiver.port}; no CAN or serial access",
        flush=True,
    )
    started = time.perf_counter()
    try:
        while not args.seconds or time.perf_counter() - started < args.seconds:
            receiver.step()
    except KeyboardInterrupt:
        pass
    finally:
        receiver.close()


if __name__ == "__main__":
    main()
