"""Receiver-only UDP evidence. The sender is this Python test, NOT Chataigne."""

import json
import socket
import statistics
import threading
import time
from pathlib import Path

from sim_receiver import PREFIX, Receiver, decode, encode

root = Path(__file__).resolve().parent
evidence = root / "evidence"
evidence.mkdir(exist_ok=True)
received = evidence / "receiver-loopback.jsonl"
if received.exists():
    received.write_text("")
transmitted = []
latencies = []
with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as client:
    client.bind(("127.0.0.1", 0))
    client.settimeout(2)
    receiver = Receiver(0, client.getsockname()[1], received)
    stop = threading.Event()
    errors = []

    def loop():
        try:
            while not stop.is_set():
                receiver.step(0.002)
        except Exception as exc:  # noqa: BLE001 - propagate worker failures into the test result
            errors.append(str(exc))

    thread = threading.Thread(target=loop)
    thread.start()
    try:
        commands = [(PREFIX + "state", ["RUNNING"])]
        commands += [
            (PREFIX + "move", ["cupboard-1", float(n % 91), 90.0]) for n in range(100)
        ]
        commands += [(PREFIX + "stop", ["cupboard-1"])]
        for address, args in commands:
            started = time.perf_counter()
            packet = encode(address, args)
            client.sendto(packet, ("127.0.0.1", receiver.port))
            transmitted.append(
                {
                    "source": "Python receiver-only test",
                    "monotonic": started,
                    "address": address,
                    "args": args,
                    "hex": packet.hex(),
                }
            )
            while True:
                messages = decode(client.recv(65507))
                acknowledged = False
                for path, values in messages:
                    if path == PREFIX + "error":
                        raise AssertionError(values)
                    if path == PREFIX + "ack":
                        ack = json.loads(values[0])
                        if ack == {"address": address, "args": args}:
                            acknowledged = True
                if acknowledged:
                    break
            latencies.append((time.perf_counter() - started) * 1000)
    finally:
        stop.set()
        thread.join(3)
        receiver.close()
    assert not errors, errors
    assert not thread.is_alive()
(evidence / "transmitter-loopback.jsonl").write_text(
    "".join(json.dumps(row) + "\n" for row in transmitted)
)
records = [json.loads(line) for line in received.read_text().splitlines()]
assert len([r for r in records if r["kind"] == "rx"]) == len(commands)
result = {
    "sender": "Python receiver-only diagnostic test; NOT Chataigne",
    "commands": len(commands),
    "acknowledged": len(latencies),
    "transport": "UDP 127.0.0.1 ephemeral ports",
    "round_trip_ms": {
        "min": min(latencies),
        "median": statistics.median(latencies),
        "p95": sorted(latencies)[int(len(latencies) * 0.95)],
        "max": max(latencies),
    },
    "native_engine_tested": False,
    "physical_hardware_tested": False,
}
(evidence / "loopback-result.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps(result, indent=2))
