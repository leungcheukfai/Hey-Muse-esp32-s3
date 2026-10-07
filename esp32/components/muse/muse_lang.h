/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Traditional Chinese UI copy. Unknown strings are returned unchanged. */
const char *muse_lang_get(const char *english);

/* Instructions sent to Muse and Gemini to keep written and spoken replies in
 * the language chosen by the user. */
const char *muse_lang_chat_instruction(int cantonese);
const char *muse_lang_tts_style(int cantonese);

#ifdef __cplusplus
}
#endif
