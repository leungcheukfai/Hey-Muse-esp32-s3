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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

typedef bool (*muse_gemini_pcm_cb_t)(const int16_t *pcm, size_t frames, void *context);

/* Sends reply text to Gemini TTS over HTTPS and streams 16 kHz mono PCM to cb.
 * The caller owns and must clear api_key after this call. */
esp_err_t muse_gemini_tts_generate(const char *api_key, const char *text, const char *style,
                                   muse_gemini_pcm_cb_t cb, void *context);
