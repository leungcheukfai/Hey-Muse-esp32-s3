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

#include "muse_lang.h"

const char *muse_lang_get(const char *english)
{
    /* This English-first build displays the SDK's English UI strings directly. */
    return english ? english : "";
}

const char *muse_lang_chat_instruction(void)
{
    return "Reply in clear, natural English. Keep names and technical terms clear.";
}

const char *muse_lang_tts_style(void)
{
    return "Speak naturally in clear English with neutral pronunciation. Read only the provided reply text.";
}
