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

Experimental community firmware adding on-device **“Hey Muse”** wake-word
recognition to the Waveshare ESP32-S3-Touch-AMOLED-1.75C, using the Muse
Gadgets SDK. Push-to-talk remains available. The wake detector runs locally;
voice turns use the existing Muse connection.

This is an unofficial community project, not made or endorsed by Meta. It is
based on the [Muse Gadgets SDK](https://github.com/facebookincubator/muse-gadget-sdk).
The wake-word model is from [wobsoriano/hey-muse](https://github.com/wobsoriano/hey-muse);
its MIT license and attribution are included beside the model.

## Status

Wake-word detection and hands-free recording are experimental. This source
snapshot has not yet been built and validated on the 1.75C hardware. Listening
is intended to operate while the device is awake or on USB power and to stop
when battery sleep powers the audio hardware down. See
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

## Language

繁體中文社群使用說明：[README.zh-Hant.md](README.zh-Hant.md)

## License

The Muse Gadgets SDK source is licensed under Apache-2.0; see [`LICENSE`](LICENSE).
The Hey Muse model is distributed under MIT; see
[`esp32/components/muse/models/HEY-MUSE-LICENSE`](esp32/components/muse/models/HEY-MUSE-LICENSE).
