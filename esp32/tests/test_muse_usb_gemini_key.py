from __future__ import annotations

import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "muse"))

from save_gemini_key import send_key


class FakeSerial:
    def __init__(self, response: bytes = b"I muse_ble: cmd gemini.key -> ok\n") -> None:
        self.response = response
        self.writes: list[bytes] = []
        self.port = None
        self.baudrate = None
        self.timeout = None
        self.dtr = None
        self.rts = None

    def open(self) -> None:
        pass

    def reset_input_buffer(self) -> None:
        pass

    def write(self, data: bytes) -> int:
        self.writes.append(data)
        return len(data)

    def flush(self) -> None:
        pass

    def readline(self) -> bytes:
        response, self.response = self.response, b""
        return response

    def close(self) -> None:
        pass


class UsbGeminiKeyTest(unittest.TestCase):
    def test_sends_key_once_over_usb_and_only_returns_save_status(self) -> None:
        device = FakeSerial()

        result = send_key("/dev/test", "AIza-example", serial_factory=lambda: device, timeout_s=0.01)

        self.assertEqual(result, "saved")
        self.assertEqual(device.writes, [b">gemini.key=AIza-example\n"])
        self.assertEqual(device.dtr, False)
        self.assertEqual(device.rts, False)

    def test_does_not_send_keys_longer_than_device_limit(self) -> None:
        opened = False

        def factory() -> FakeSerial:
            nonlocal opened
            opened = True
            return FakeSerial()

        with self.assertRaisesRegex(ValueError, "128 characters"):
            send_key("/dev/test", "x" * 129, serial_factory=factory)
        self.assertFalse(opened)


if __name__ == "__main__":
    unittest.main()
