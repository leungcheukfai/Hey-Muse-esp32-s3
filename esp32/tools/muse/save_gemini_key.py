#!/usr/bin/env python3
# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Save a Gemini API key to the connected Muse over USB, without echoing it."""

import argparse
import getpass
import sys
import time

import ports

KEY_MAX = 128
ACK_TIMEOUT_S = 5.0


def send_key(port, key, serial_factory=None, timeout_s=ACK_TIMEOUT_S):
    """Send one key over the Muse USB console; return saved/rejected/timeout."""
    try:
        key_bytes = key.encode("ascii")
    except UnicodeEncodeError as exc:
        raise ValueError("Gemini API keys must contain ASCII characters.") from exc
    if not key_bytes:
        raise ValueError("The Gemini API key is empty.")
    if len(key_bytes) > KEY_MAX:
        raise ValueError(f"The key is longer than the device limit of {KEY_MAX} characters.")

    if serial_factory is None:
        try:
            import serial
        except ImportError as exc:
            raise RuntimeError("pyserial is missing; activate the ESP-IDF Python environment first.") from exc
        serial_factory = serial.Serial

    ser = serial_factory()
    try:
        ser.port = port
        ser.baudrate = 115200
        ser.timeout = 0.2
        ser.dtr = False
        ser.rts = False
        ser.open()
        ser.reset_input_buffer()
        ser.write(b">gemini.key=" + key_bytes + b"\n")
        ser.flush()

        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            line = ser.readline()
            if b"cmd gemini.key -> ok" in line:
                return "saved"
            if b"cmd gemini.key -> error:" in line:
                return "rejected"
        return "timeout"
    finally:
        ser.close()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Muse USB serial port (auto-detected if omitted)")
    args = parser.parse_args(argv)

    try:
        port = args.port or ports.find("s3")
    except ports.NotFound as exc:
        print(f"{exc} Connect one 1.75C by USB, or pass --port.", file=sys.stderr)
        return 1
    except ImportError:
        print("pyserial is missing; activate the ESP-IDF Python environment first.", file=sys.stderr)
        return 1

    key = getpass.getpass("Gemini API key (input hidden): ").strip()
    try:
        result = send_key(port, key)
    except (OSError, RuntimeError, ValueError) as exc:
        print(str(exc), file=sys.stderr)
        return 1
    finally:
        key = ""

    if result == "saved":
        print("Gemini key saved. Check Muse Settings → Voice Replies for ‘Gemini TTS: key saved’.")
        return 0
    if result == "rejected":
        print("Muse rejected the key. Check that it is no longer than 128 ASCII characters.", file=sys.stderr)
        return 1
    print("No confirmation from Muse. Check that the board is awake and the USB port is correct.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
