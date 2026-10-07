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

"""Save a Hugging Face access token to the connected Muse over USB."""

import argparse
import getpass
import sys
import time

import ports

TOKEN_MAX = 128
ACK_TIMEOUT_S = 5.0


def send_token(port, token, serial_factory=None, timeout_s=ACK_TIMEOUT_S):
    """Send one token over the Muse USB console; return saved/rejected/timeout."""
    try:
        token_bytes = token.encode("ascii")
    except UnicodeEncodeError as exc:
        raise ValueError("Hugging Face tokens must contain ASCII characters.") from exc
    if not token_bytes:
        raise ValueError("The Hugging Face token is empty.")
    if len(token_bytes) > TOKEN_MAX:
        raise ValueError(f"The token is longer than the device limit of {TOKEN_MAX} characters.")

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
        ser.write(b">hf.token=" + token_bytes + b"\n")
        ser.flush()

        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            line = ser.readline()
            if b"cmd hf.token -> ok" in line:
                return "saved"
            if b"cmd hf.token -> error:" in line:
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

    token = getpass.getpass("Hugging Face access token (input hidden): ").strip()
    try:
        result = send_token(port, token)
    except (OSError, RuntimeError, ValueError) as exc:
        print(str(exc), file=sys.stderr)
        return 1
    finally:
        token = ""

    if result == "saved":
        print("Hugging Face token saved. Check Muse Settings → Voice Replies for ‘Hugging Face TTS: token saved’.")
        return 0
    if result == "rejected":
        print("Muse rejected the token. Check that it is no longer than 128 ASCII characters.", file=sys.stderr)
        return 1
    print("No confirmation from Muse. Check that the board is awake and the USB port is correct.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
