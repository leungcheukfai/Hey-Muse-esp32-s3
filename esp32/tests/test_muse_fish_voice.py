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
FISH_TTS_C = ROOT / "components" / "muse" / "muse_fish_tts.c"


class MuseFishVoiceTest(unittest.TestCase):
    def test_tts_request_uses_the_selected_voice_reference(self) -> None:
        source = FISH_TTS_C.read_text()

        self.assertIn(
            'static char *request_json(const char *voice_id, const char *text)',
            source,
        )
        self.assertIn(
            'cJSON_AddStringToObject(root, "reference_id", voice_id)',
            source,
        )
        self.assertIn('char *json = request_json(voice_id, text);', source)


if __name__ == "__main__":
    unittest.main()
