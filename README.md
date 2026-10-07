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

# Hey Muse for ESP32-S3

Experimental, English-first community firmware adding on-device **“Hey Muse”**
wake-word recognition to the Waveshare ESP32-S3-Touch-AMOLED-1.75C, using the
Muse Gadgets SDK. The on-device menus, phone setup, documentation, and Muse
replies are English-only. Push-to-talk remains available. The wake detector
runs locally; voice turns use the existing Muse connection.

This is an unofficial community project, not made or endorsed by Meta. It is
based on the [Muse Gadgets SDK](https://github.com/facebookincubator/muse-gadget-sdk).
The wake-word model is from [wobsoriano/hey-muse](https://github.com/wobsoriano/hey-muse);
its MIT license and attribution are included beside the model.

For a start-to-finish guide to installing ESP-IDF, building and flashing the
1.75C, pairing with Muse, saving a Fish Audio key, and testing voice replies,
see the [setup guide](docs/SETUP.md).

## Status

Wake-word detection and hands-free recording are experimental. The firmware
has been built and flashed on 1.75C hardware during development; the voice flow
still needs broader real-world testing. On the 1.75C, “Hey Muse” remains
available while the screen is asleep, including on battery. This keeps the
microphone and detector active and uses more battery than button-only sleep. See
[`esp32/devices/README.md`](esp32/devices/README.md) for board notes.

## Build and flash

1. Install and activate **ESP-IDF 6.0.1** with support for the `esp32s3` target.
   See the [Espressif ESP-IDF setup guide](https://docs.espressif.com/projects/esp-idf/en/release-v6.0/esp32s3/get-started/index.html).
2. From `esp32/`, open the 1.75C configuration menu:

   ```sh
   B=build-muse-waveshare-s3-175c
   idf.py -B "$B" -DIDF_TARGET=esp32s3 \
     -DSDKCONFIG="$B/sdkconfig" \
     -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;devices/sdkconfig.muse;devices/sdkconfig.muse-waveshare-s3-175c" \
     menuconfig
   ```

   Search for `GADGET_SDK_TOKEN`, enter your own Muse SDK token, then save and
   exit. Each builder needs their own token; never commit or share it.
3. Build and flash the Waveshare 1.75C from `esp32/`:

   ```sh
   tools/muse/board.sh build s3
   tools/muse/board.sh flash s3
   ```

   The flash helper detects the board's USB port when it is the only matching
   board connected. With multiple boards attached, pass the 1.75C serial number
   or port, for example `tools/muse/board.sh flash s3 /dev/cu.usbmodem…`.

The firmware build can contain the SDK token from `sdkconfig`. Do not publish a
`.bin` built with your personal token. Keep the token in the ignored local
build configuration and have each user build with their own token.

## Add spoken replies

All spoken replies use Fish Audio's S2.1 Pro Free TTS API. Muse's reply text is
sent to Fish over HTTPS; microphone audio is not sent to Fish. The API key is
saved on the device after flashing and is not compiled into the firmware. The
default voice is [this Fish Audio voice](https://fish.audio/app/m/1df12c4bb692423283fde2bdc7f84093).

Create a Fish Audio API key at <https://fish.audio/app/api-keys>. With the
1.75C connected by USB, run this from `esp32/` in an ESP-IDF terminal:

```sh
python tools/muse/save_fish_api_key.py
```

The helper finds the connected board and asks for the key with hidden input.
It sends the key over USB, prints only the save result, and stores it in
encrypted device NVS. Check **Settings > Voice replies** for
`Fish Audio API key saved`. You can also enter the key on the Bluetooth setup
page. Free-tier availability and fair-use limits follow Fish Audio's current
terms.

## License

The Muse Gadgets SDK source is licensed under Apache-2.0; see [`LICENSE`](LICENSE).
The Hey Muse model is distributed under MIT; see
[`esp32/components/muse/models/HEY-MUSE-LICENSE`](esp32/components/muse/models/HEY-MUSE-LICENSE).
