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

from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
VOICE_C = ROOT / "components" / "muse" / "muse_voice.c"
WAKE_CPP = ROOT / "components" / "muse" / "muse_wake.cpp"
BLE_SETUP_HTML = ROOT / "tools" / "muse" / "ble_setup.html"


class MuseWakeScreenTest(unittest.TestCase):
    def test_hands_free_recording_allows_longer_pauses_and_messages(self) -> None:
        source = VOICE_C.read_text()

        self.assertIn("#define MAX_SECS 30", source)
        self.assertIn("#define AUTO_SILENCE_SECS 2", source)

    def test_two_seconds_of_silence_submits_the_hands_free_note(self) -> None:
        source = VOICE_C.read_text()
        silence_end = (
            "if (hands_free && !released && speech_seen && "
            "quiet_frames >= AUTO_SILENCE_FRAMES)"
        )
        self.assertIn(silence_end, source)

        voice_task = source[source.index("static void voice_task"):source.index("esp_err_t muse_voice_start")]
        self.assertLess(voice_task.index("bool ok = record("), voice_task.index("pending_down = finish_note();"))

    def test_detected_wake_word_clears_screen_sleep_before_recording(self) -> None:
        source = VOICE_C.read_text()
        detected = source.index("bool detected = idle_capture();")
        queued_event = source.index("if (xQueueReceive(s_queue, &ev, 0)", detected)
        wake_handling = source[detected:queued_event]

        self.assertIn("if (detected)", wake_handling)
        self.assertIn("muse_state_set_asleep(false);", wake_handling)

    def test_detector_reset_reuses_the_interpreter_between_voice_turns(self) -> None:
        source = WAKE_CPP.read_text()
        reset_start = source.index('extern "C" void muse_wake_reset(void)')
        reset_end = source.index('extern "C" bool muse_wake_enabled(void)', reset_start)
        reset = source[reset_start:reset_end]

        self.assertRegex(reset, r"if\s*\(s_interpreter\s*&&\s*s_interpreter->Reset\(\)")
        self.assertIn('ESP_LOGW(TAG, "could not reset Hey Muse model', reset)
        self.assertIn("constexpr size_t WARMUP_WINDOWS = 20;", source)

    def test_phone_setup_explains_why_settings_are_disabled_until_connected(self) -> None:
        source = BLE_SETUP_HTML.read_text()
        self.assertIn('id="controls-hint"', source)
        self.assertIn("Connect to Muse above to unlock these settings.", source)


if __name__ == "__main__":
    unittest.main()
