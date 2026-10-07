# Contributing to Hey Muse for ESP32-S3

Thanks for helping improve this English-first community firmware project.

## Before opening a pull request

- Describe the board and ESP-IDF version used to reproduce a bug.
- Keep changes focused and update the relevant documentation.
- Build the Waveshare 1.75C target with `tools/muse/board.sh build s3` from
  `esp32/` when your changes affect the firmware.
- Never commit a Muse SDK token, a speech-service API key, Wi-Fi credentials,
  or private audio recordings.

Please include the build or reproduction details in your pull request. Hardware
changes should say which board was used and what was verified on the device.

## License

The firmware source is licensed under Apache-2.0. The Hey Muse wake-word model
has its own MIT license and attribution beside the model. See [`LICENSE`](LICENSE)
and `esp32/components/muse/models/HEY-MUSE-LICENSE`.
