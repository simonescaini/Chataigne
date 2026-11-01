import json
import math
import socket
import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from sim_receiver import PREFIX, Receiver, Simulator, decode, encode


class ProtocolTests(unittest.TestCase):
    def test_canonical_osc_bytes(self):
        # Independently specified OSC message bytes, not just encoder/decoder agreement.
        expected = b"/midnight/sim/move\0\0,sff\0\0\0\0cupboard-1\0\0" + struct.pack(
            ">ff", 45.0, 90.0
        )
        self.assertEqual(encode(PREFIX + "move", ["cupboard-1", 45.0, 90.0]), expected)
        self.assertEqual(
            decode(expected), [(PREFIX + "move", ["cupboard-1", 45.0, 90.0])]
        )

    def test_types_and_unicode(self):
        self.assertEqual(
            decode(encode("/test", ["cupboard", 45, 2.5])),
            [("/test", ["cupboard", 45, 2.5])],
        )
        self.assertEqual(
            decode(encode("/test", ["cupboard \u00e9"])),
            [("/test", ["cupboard \u00e9"])],
        )

    def test_immediate_bundle(self):
        msg = encode(PREFIX + "stop", ["cupboard-1"])
        packet = b"#bundle\0" + struct.pack(">Qi", 1, len(msg)) + msg
        self.assertEqual(decode(packet), [(PREFIX + "stop", ["cupboard-1"])])

    def test_future_bundle_is_not_scheduled(self):
        with self.assertRaisesRegex(ValueError, "Chataigne owns timing"):
            decode(b"#bundle\0" + struct.pack(">Q", 42))

    def test_malformed_packets(self):
        valid = encode(PREFIX + "move", ["cupboard-1", 45.0, 90.0])
        packets = [
            b"",
            b"not osc",
            valid[:-1],
            valid + b"x",
            b"/x\0x,i\0\0" + struct.pack(">i", 1),
            encode("/x", []).replace(b",\0", b",b"),
        ]
        for packet in packets:
            with (
                self.subTest(packet=packet),
                self.assertRaises((ValueError, UnicodeError)),
            ):
                decode(packet)

    def test_nonfinite_rejected(self):
        for number in (math.inf, -math.inf, math.nan):
            with self.assertRaises(ValueError):
                encode("/x", [number])
            with self.assertRaises(ValueError):
                decode(b"/x\0\0,f\0\0" + struct.pack(">f", number))

    def test_nul_rejected(self):
        with self.assertRaises(ValueError):
            encode("/x", ["bad\0name"])


class ModelTests(unittest.TestCase):
    def test_speed_arrival_and_no_overshoot(self):
        model = Simulator()
        model.apply(PREFIX + "move", ["A", 90, 45])
        model.advance(1)
        self.assertEqual(model.motors["A"].position, 45)
        model.advance(3)
        self.assertEqual(model.motors["A"].position, 90)
        self.assertFalse(model.motors["A"].moving)

    def test_stop_then_explicit_new_move(self):
        model = Simulator()
        model.apply(PREFIX + "move", ["A", 90, 45])
        model.advance(1)
        model.apply(PREFIX + "stop", ["A"])
        model.advance(5)
        self.assertEqual(model.motors["A"].position, 45)
        model.apply(PREFIX + "move", ["A", 0, 90])
        model.advance(0.5)
        self.assertEqual(model.motors["A"].position, 0)

    def test_independent_motors(self):
        model = Simulator()
        model.apply(PREFIX + "move", ["A", 90, 45])
        model.apply(PREFIX + "move", ["B", -20, 10])
        model.advance(1)
        model.apply(PREFIX + "stop", ["A"])
        model.advance(1)
        self.assertEqual(model.motors["A"].position, 45)
        self.assertEqual(model.motors["B"].position, -20)

    def test_invalid_actions_do_not_mutate(self):
        model = Simulator()
        for address, args in [
            (PREFIX + "move", ["A", 0, 0]),
            (PREFIX + "move", ["A", 1]),
            (PREFIX + "move", ["A", math.nan, 3]),
            (PREFIX + "move", ["", 1, 3]),
            (PREFIX + "stop", ["A", 1]),
            ("/physical/move", ["A", 0, 1]),
        ]:
            with self.subTest(args=args), self.assertRaises(ValueError):
                model.apply(address, args)
        self.assertEqual(list(model.motors), ["cupboard-1"])

    def test_state_notification_does_not_schedule_movement(self):
        model = Simulator()
        model.apply(PREFIX + "state", ["RUNNING"])
        model.advance(10)
        self.assertEqual(model.state, "RUNNING")
        self.assertEqual(model.motors["cupboard-1"].position, 0)


class SocketTests(unittest.TestCase):
    def test_real_udp_loopback_ack_feedback_and_diagnostics(self):
        with (
            tempfile.TemporaryDirectory() as directory,
            socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as client,
        ):
            client.bind(("127.0.0.1", 0))
            client.settimeout(1)
            path = Path(directory) / "receiver.jsonl"
            receiver = Receiver(0, client.getsockname()[1], path)
            try:
                client.sendto(
                    encode(PREFIX + "move", ["cupboard-1", 45.0, 90.0]),
                    ("127.0.0.1", receiver.port),
                )
                receiver.step(0.1)
                messages = []
                while len(messages) < 2:
                    messages.extend(decode(client.recv(65507)))
                self.assertIn(PREFIX + "ack", [a for a, _ in messages])
                self.assertIn(PREFIX + "feedback", [a for a, _ in messages])
                client.sendto(b"broken", ("127.0.0.1", receiver.port))
                receiver.step(0.1)
                records = [json.loads(line) for line in path.read_text().splitlines()]
                self.assertTrue(any(r["kind"] == "rx" for r in records))
                self.assertTrue(any(r["kind"] == "error" for r in records))
                client.sendto(
                    encode(PREFIX + "stop", ["cupboard-1"]),
                    ("127.0.0.1", receiver.port),
                )
                receiver.step(0.1)
                self.assertFalse(receiver.simulator.motors["cupboard-1"].moving)
            finally:
                receiver.close()


if __name__ == "__main__":
    unittest.main()
