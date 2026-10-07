<!--
Copyright (c) Meta Platforms, Inc. and affiliates.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->

# First-time setup: Waveshare ESP32-S3 Touch-AMOLED-1.75C

This guide takes you from a new checkout to a paired device with English voice
replies. It is written for macOS and Linux. The supported, tested target is the
Waveshare ESP32-S3-Touch-AMOLED-1.75C.

Hey Muse is an unofficial community project based on the
[Muse Gadgets SDK](https://github.com/facebookincubator/muse-gadget-sdk). It is
not made or endorsed by Meta. Wake-word detection and hands-free recording are
experimental; use push-to-talk if you need a reliable manual way to start a
voice turn.

## What you need

- A Waveshare ESP32-S3-Touch-AMOLED-1.75C.
- A USB cable that carries data, and a computer running macOS or Linux.
- [ESP-IDF v6.0.1](https://github.com/espressif/esp-idf/tree/v6.0.1), including
  its `esp32s3` tools.
- A Muse SDK token from [gadgets.muse.ai](https://gadgets.muse.ai/settings/sdk-tokens).
  Read the [Gadget SDK Terms](https://gadgets.muse.ai/sdk-terms).
- The Muse mobile app, with developer mode and community devices available.
- A Fish Audio API key if you want spoken replies. Without it, replies appear
  as captions on the display.

Use a trusted 2.4 GHz Wi-Fi network for the device. Do not post your SDK token,
Fish Audio API key, Wi-Fi password, or a firmware binary built with your
personal SDK token.

## 1. Get the source

Open Terminal and clone the project:

```sh
git clone https://github.com/leungcheukfai/Hey-Muse-esp32-s3.git
cd Hey-Muse-esp32-s3/esp32
PROJECT_DIR="$PWD"
```

Keep the terminal open for the remaining steps. The commands below assume the
repository is in `~/Hey-Muse-esp32-s3`; if you cloned it elsewhere, use that
location instead.

## 2. Install and activate ESP-IDF

Install the prerequisites for your operating system using
[Espressif's ESP-IDF v6.0 setup instructions](https://docs.espressif.com/projects/esp-idf/en/release-v6.0/esp32s3/get-started/index.html).
On macOS, Homebrew and Apple's command-line developer tools are commonly used.

Install the ESP-IDF v6.0.1 source and tools for ESP32-S3:

```sh
mkdir -p ~/esp
cd ~/esp
git clone --branch v6.0.1 --recursive https://github.com/espressif/esp-idf.git esp-idf-v6
cd esp-idf-v6
./install.sh esp32s3
. ./export.sh
idf.py --version
cd "$PROJECT_DIR"
```

The version command should report ESP-IDF v6.0.1. Continue in this terminal.
If you open a new one, activate the same installation and return to the
repository's `esp32/` directory before using `idf.py`:

```sh
. ~/esp/esp-idf-v6/export.sh
```

Using one ESP-IDF installation consistently avoids build-directory errors
caused by switching between different Python environments or ESP-IDF versions.

## 3. Add your Muse SDK token to the local build configuration

Get your own token from the Muse SDK tokens page. From the repository's
`esp32/` directory, open the Waveshare 1.75C configuration menu:

```sh
B=build-muse-waveshare-s3-175c
idf.py -B "$B" -DIDF_TARGET=esp32s3 \
  -DSDKCONFIG="$B/sdkconfig" \
  -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;devices/sdkconfig.muse;devices/sdkconfig.muse-waveshare-s3-175c" \
  menuconfig
```

In the menu, find **Gadget SDK token** (search for `GADGET_SDK_TOKEN`), enter
your token, then save and exit. This writes it into your local build's
`sdkconfig`; it is not a source file. Each developer should use their own
token. The token is included in the firmware binary, so do not share or publish
a binary built with your personal token.

## 4. Build and flash the board

Connect the board directly to the computer with the USB data cable. From the
`esp32/` directory, build and flash it:

```sh
tools/muse/board.sh build s3
tools/muse/board.sh flash s3
```

The helper looks for the connected 1.75C. If more than one supported board is
connected, specify this board's serial port on the flash command. On macOS,
ports usually look like `/dev/cu.usbmodem…`; on Linux, they often look like
`/dev/ttyACM0` or `/dev/ttyUSB0`:

```sh
tools/muse/board.sh flash s3 /dev/cu.usbmodem1101
```

Use the port shown on your computer, not the example port above. Flashing
replaces the firmware but preserves the device's NVS settings, including its
Wi-Fi, pairing, and Fish Audio key. It does not erase the whole flash.

## 5. Pair the device with Muse

1. Start the Muse app on your phone and enable **Settings → Devices → Developer
   mode**.
2. Choose **Settings → Devices → Add Device** and select the device advertised
   as `MuseGadget-XXXXXX`.
3. Follow the app and on-device prompts to confirm pairing and connect the
   device to your Wi-Fi network.
4. Wait for the device to finish connecting to Muse. The app and the screen
   show the connection state.

Community devices do not have manufacturer attestation. Pair the board on a
network you trust.

## 6. Add a Fish Audio API key for spoken replies

Create an API key in [Fish Audio](https://fish.audio/app/api-keys). With the
board connected over USB, return to the repository's `esp32/` directory and
run:

```sh
python tools/muse/save_fish_api_key.py
```

The helper detects the board and prompts for the API key with hidden input.
Paste the key and press Return; the characters will not appear as you type.
The helper sends the key over USB and saves it in the device's encrypted NVS.
It does not print the key. You do not need to rebuild or reflash to change the
key. Close any serial monitor that is using the board's USB port before
running the helper.

On the device, open **Settings → Voice Replies** and confirm that the status
says **Fish Audio API key saved**. For spoken replies, the device sends Muse's
reply text to Fish Audio over HTTPS. Microphone audio continues to go to Muse;
it is not sent to Fish Audio. Fish Audio's model availability, free usage, and
account limits can change; check its current account page and terms. The
firmware uses [this Fish Audio voice](https://fish.audio/app/m/1df12c4bb692423283fde2bdc7f84093)
by default.

## 7. Try Hey Muse

With the device awake and connected to Wi-Fi:

1. Say **“Hey Muse.”** The wake detector runs locally; the screen should turn
   on and play its alert sound.
2. Say your request in English.
3. A two-second pause after speech ends the recording and submits it to Muse.
   Push-to-talk remains available as a manual alternative.
4. The reply appears on the display and is spoken through Fish Audio when the
   API key is saved and the service is available.

Wake listening uses the board's audio hardware. When battery sleep turns that
hardware off, wake-word detection is suspended; press the board's physical
button to wake it. For hands-free use, keep the board awake or powered by USB.
This firmware is English-first and does not translate replies.

## Troubleshooting

| What you see | What to try |
|---|---|
| `idf.py: command not found` | In that terminal, run `. ~/esp/esp-idf-v6/export.sh`. |
| The build reports a different Python environment or ESP-IDF version | Use the same v6.0.1 `export.sh` in the terminal where you configured and built this `B` directory. Do not mix ESP-IDF installations. |
| No serial port is detected | Check that the cable supports data, reconnect the board, and close apps that may have opened its port. List macOS ports with `ls /dev/cu.usb*`; Linux ports are commonly under `/dev/ttyACM*` or `/dev/ttyUSB*`. Pass the correct port to the flash command. |
| Replies show as text but there is no speech | Open **Settings → Voice Replies** and confirm the Fish key is saved. Check that the device is awake, online, and able to reach Fish Audio; account limits or API errors can also prevent speech. Without a key, captions still work. |
| Hey Muse does not respond after pressing the physical sleep button | Press the physical button to wake the device first. Wake-word listening is suspended while battery sleep has powered down the audio hardware. |
| The device does not connect to Muse | Confirm pairing in the Muse app, verify the Wi-Fi network and password in the setup flow, and check the serial log for the first connection error. |

To view the serial log, activate ESP-IDF, go to `esp32/`, and run
`idf.py -B build-muse-waveshare-s3-175c -p PORT monitor`. Replace `PORT` with
the device's serial port and press `Ctrl+]` to exit the monitor.

## Rebuild after updating the source

From the repository's `esp32/` directory, activate ESP-IDF, pull the source,
then build and flash again:

```sh
git pull
. ~/esp/esp-idf-v6/export.sh
tools/muse/board.sh build s3
tools/muse/board.sh flash s3
```

The build uses your local SDK token. Never commit `sdkconfig`, API keys,
credentials, or firmware binaries that contain your personal token.

## More information

- [Project overview and source](../README.md)
- [ESP32 firmware notes](../esp32/README.md)
- [Waveshare board profile](../esp32/devices/sdkconfig.muse-waveshare-s3-175c)
- [ESP-IDF v6.0 documentation](https://docs.espressif.com/projects/esp-idf/en/release-v6.0/esp32s3/get-started/index.html)
- [Fish Audio API key setup](https://docs.fish.audio/developer-guide/getting-started/api-key)
