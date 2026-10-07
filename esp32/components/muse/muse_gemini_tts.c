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

#include "muse_gemini_tts.h"

#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "mbedtls/base64.h"

static const char *TAG = "muse_gemini_tts";

#define RESPONSE_MAX (2 * 1024 * 1024)
#define RESPONSE_INITIAL 8192
#define B64_CHUNK 4096
#define PCM_CHUNK (B64_CHUNK / 4 * 3)
#define MIC_RATE 16000
#define GEMINI_TTS_RATE 24000
#define GEMINI_TTS_MODEL "gemini-2.5-flash-preview-tts"

typedef struct {
    uint8_t *data;
    size_t len;
    size_t cap;
    bool overflow;
} response_t;

static esp_err_t on_http_event(esp_http_client_event_t *event)
{
    response_t *response = event->user_data;
    if (event->event_id != HTTP_EVENT_ON_DATA || event->data_len <= 0 || !response) {
        return ESP_OK;
    }
    size_t add = (size_t)event->data_len;
    if (response->len + add > RESPONSE_MAX) {
        response->overflow = true;
        return ESP_ERR_INVALID_SIZE;
    }
    size_t need = response->len + add + 1;
    if (need > response->cap) {
        size_t cap = response->cap ? response->cap : RESPONSE_INITIAL;
        while (cap < need && cap < RESPONSE_MAX + 1) {
            cap *= 2;
        }
        if (cap > RESPONSE_MAX + 1) {
            cap = RESPONSE_MAX + 1;
        }
        uint8_t *grown = heap_caps_realloc(response->data, cap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!grown) {
            return ESP_ERR_NO_MEM;
        }
        response->data = grown;
        response->cap = cap;
    }
    memcpy(response->data + response->len, event->data, add);
    response->len += add;
    response->data[response->len] = '\0';
    return ESP_OK;
}

static int sample_rate_from_mime(const char *mime)
{
    const char *rate = mime ? strstr(mime, "rate=") : NULL;
    if (rate) {
        char *end = NULL;
        long parsed = strtol(rate + 5, &end, 10);
        if (end != rate + 5 && parsed >= 8000 && parsed <= 48000) {
            return (int)parsed;
        }
    }
    return GEMINI_TTS_RATE;
}

static const char *audio_data(cJSON *root, int *sample_rate)
{
    cJSON *candidates = cJSON_GetObjectItem(root, "candidates");
    cJSON *candidate;
    cJSON_ArrayForEach(candidate, candidates) {
        cJSON *content = cJSON_GetObjectItem(candidate, "content");
        cJSON *parts = cJSON_GetObjectItem(content, "parts");
        cJSON *item;
        cJSON_ArrayForEach(item, parts) {
            cJSON *inline_data = cJSON_GetObjectItem(item, "inlineData");
            const char *data = cJSON_GetStringValue(cJSON_GetObjectItem(inline_data, "data"));
            if (data) {
                const char *mime = cJSON_GetStringValue(cJSON_GetObjectItem(inline_data, "mimeType"));
                *sample_rate = sample_rate_from_mime(mime);
                return data;
            }
        }
    }
    return NULL;
}

typedef struct {
    uint32_t step;
    uint32_t pos;
} pcm_resampler_t;

static void pcm_resampler_init(pcm_resampler_t *resampler, int input_rate)
{
    resampler->step = (uint32_t)(((uint64_t)input_rate << 16) / MIC_RATE);
    resampler->pos = 0;
}

static size_t resample_to_mic_rate(pcm_resampler_t *resampler, const int16_t *input, size_t frames,
                                   int16_t *output)
{
    size_t out = 0;
    while ((resampler->pos >> 16) < frames) {
        output[out++] = input[resampler->pos >> 16];
        resampler->pos += resampler->step;
    }
    resampler->pos -= (uint32_t)(frames << 16);
    return out;
}

static esp_err_t decode_audio(const char *encoded, int input_rate, muse_gemini_pcm_cb_t cb, void *context)
{
    size_t len = strlen(encoded);
    if (!len || len > RESPONSE_MAX || (len & 3)) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    union {
        uint32_t alignment;
        uint8_t bytes[PCM_CHUNK];
    } decoded;
    int16_t resampled[PCM_CHUNK + 4];
    pcm_resampler_t resampler;
    pcm_resampler_init(&resampler, input_rate);
    for (size_t off = 0; off < len;) {
        size_t chunk = len - off < B64_CHUNK ? len - off : B64_CHUNK;
        size_t out = 0;
        int rc = mbedtls_base64_decode(decoded.bytes, sizeof(decoded.bytes), &out,
                                       (const unsigned char *)encoded + off, chunk);
        if (rc != 0 || (out & 1)) {
            return ESP_ERR_INVALID_RESPONSE;
        }
        if (out) {
            const int16_t *pcm = (const int16_t *)decoded.bytes;
            size_t frames = out / sizeof(int16_t);
            if (input_rate != MIC_RATE) {
                frames = resample_to_mic_rate(&resampler, pcm, frames, resampled);
                pcm = resampled;
            }
            if (frames && !cb(pcm, frames, context)) {
                return ESP_ERR_INVALID_STATE;
            }
        }
        off += chunk;
    }
    return ESP_OK;
}

static char *request_json(const char *text, const char *style)
{
    static const char *const instruction =
        "Do not read these instructions aloud. Read only the reply below, verbatim.";
    size_t style_len = strlen(style);
    size_t text_len = strlen(text);
    size_t instruction_len = strlen(instruction);
    if (style_len > SIZE_MAX - instruction_len ||
        style_len + instruction_len > SIZE_MAX - 4 ||
        text_len > SIZE_MAX - style_len - instruction_len - 4) {
        return NULL;
    }
    size_t prompt_cap = style_len + instruction_len + text_len + 4;
    char *prompt = (char *)malloc(prompt_cap);
    if (!prompt) {
        return NULL;
    }
    snprintf(prompt, prompt_cap, "%s\n%s\n\n%s", style, instruction, text);

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        free(prompt);
        return NULL;
    }
    cJSON *contents = cJSON_AddArrayToObject(root, "contents");
    cJSON *content = cJSON_CreateObject();
    cJSON *parts = content ? cJSON_AddArrayToObject(content, "parts") : NULL;
    cJSON *part = cJSON_CreateObject();
    if (!contents || !content || !parts || !part || !cJSON_AddStringToObject(part, "text", prompt)) {
        cJSON_Delete(part);
        cJSON_Delete(content);
        cJSON_Delete(root);
        free(prompt);
        return NULL;
    }
    if (!cJSON_AddItemToArray(parts, part) || !cJSON_AddItemToArray(contents, content)) {
        /* cJSON has not taken ownership of the item on a failed insertion. */
        cJSON_Delete(content);
        cJSON_Delete(root);
        free(prompt);
        return NULL;
    }
    cJSON *generation = cJSON_AddObjectToObject(root, "generationConfig");
    cJSON *modalities = generation ? cJSON_AddArrayToObject(generation, "responseModalities") : NULL;
    cJSON *speech_config = generation ? cJSON_AddObjectToObject(generation, "speechConfig") : NULL;
    cJSON *voice_config = speech_config ? cJSON_AddObjectToObject(speech_config, "voiceConfig") : NULL;
    cJSON *prebuilt = voice_config ? cJSON_AddObjectToObject(voice_config, "prebuiltVoiceConfig") : NULL;
    if (!generation || !modalities || !speech_config || !voice_config || !prebuilt ||
        !cJSON_AddItemToArray(modalities, cJSON_CreateString("AUDIO")) ||
        !cJSON_AddStringToObject(prebuilt, "voiceName", "Kore")) {
        cJSON_Delete(root);
        free(prompt);
        return NULL;
    }
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    free(prompt);
    return json;
}

static void log_api_error(const response_t *response, const char *api_key, const char *text,
                          const char *style)
{
    if (!response || !response->data || !response->len) {
        return;
    }
    cJSON *root = cJSON_ParseWithLength((const char *)response->data, response->len);
    cJSON *error = root ? cJSON_GetObjectItemCaseSensitive(root, "error") : NULL;
    cJSON *code_item = error ? cJSON_GetObjectItemCaseSensitive(error, "code") : NULL;
    cJSON *status_item = error ? cJSON_GetObjectItemCaseSensitive(error, "status") : NULL;
    cJSON *message_item = error ? cJSON_GetObjectItemCaseSensitive(error, "message") : NULL;
    const char *status = cJSON_GetStringValue(status_item);
    const char *message = cJSON_GetStringValue(message_item);
    char safe_message[192] = "no structured error message";
    if (message) {
        if ((api_key && api_key[0] && strstr(message, api_key)) ||
            (text && text[0] && strstr(message, text)) ||
            (style && style[0] && strstr(message, style))) {
            snprintf(safe_message, sizeof(safe_message), "%s", "error details redacted");
        } else {
            size_t n = strlen(message);
            if (n >= sizeof(safe_message)) {
                n = sizeof(safe_message) - 1;
            }
            for (size_t i = 0; i < n; i++) {
                unsigned char ch = (unsigned char)message[i];
                safe_message[i] = ch >= 0x20 && ch != 0x7f ? (char)ch : ' ';
            }
            safe_message[n] = '\0';
        }
    }
    ESP_LOGW(TAG, "Gemini TTS API error: code %d, status %s, %s",
             cJSON_IsNumber(code_item) ? (int)cJSON_GetNumberValue(code_item) : 0,
             status ? status : "unknown", safe_message);
    cJSON_Delete(root);
}

static void wipe_free(char *data)
{
    if (!data) {
        return;
    }
    volatile char *p = (volatile char *)data;
    size_t len = strlen(data);
    while (len--) {
        *p++ = 0;
    }
    cJSON_free(data);
}

esp_err_t muse_gemini_tts_generate(const char *api_key, const char *text, const char *style,
                                   muse_gemini_pcm_cb_t cb, void *context)
{
    if (!api_key || !api_key[0] || !text || !text[0] || !style || !cb) {
        return ESP_ERR_INVALID_ARG;
    }

    char *json = request_json(text, style);
    if (!json) {
        return ESP_ERR_NO_MEM;
    }
    response_t response = {0};
    char url[160];
    snprintf(url, sizeof(url), "https://generativelanguage.googleapis.com/v1beta/models/%s:generateContent",
             GEMINI_TTS_MODEL);
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = on_http_event,
        .user_data = &response,
        .timeout_ms = 90000,
        .buffer_size = 2048,
        .buffer_size_tx = 2048,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = client ? esp_http_client_set_method(client, HTTP_METHOD_POST) : ESP_ERR_NO_MEM;
    if (err == ESP_OK) err = esp_http_client_set_header(client, "Content-Type", "application/json");
    if (err == ESP_OK) err = esp_http_client_set_header(client, "x-goog-api-key", api_key);
    if (err == ESP_OK) err = esp_http_client_set_post_field(client, json, (int)strlen(json));
    ESP_LOGI(TAG, "Gemini TTS request using %s", GEMINI_TTS_MODEL);
    if (err == ESP_OK) err = esp_http_client_perform(client);
    int status = client ? esp_http_client_get_status_code(client) : 0;
    if (client) {
        esp_http_client_cleanup(client);
    }
    wipe_free(json);

    if (err == ESP_OK && response.overflow) {
        err = ESP_ERR_INVALID_SIZE;
    }
    if (err == ESP_OK && (status < 200 || status >= 300)) {
        log_api_error(&response, api_key, text, style);
        ESP_LOGW(TAG, "Gemini TTS HTTP %d (%u response bytes)", status, (unsigned)response.len);
        err = ESP_FAIL;
    }
    if (err == ESP_OK && response.data) {
        cJSON *root = cJSON_ParseWithLength((const char *)response.data, response.len);
        int sample_rate = GEMINI_TTS_RATE;
        const char *encoded = root ? audio_data(root, &sample_rate) : NULL;
        err = encoded ? decode_audio(encoded, sample_rate, cb, context) : ESP_ERR_INVALID_RESPONSE;
        cJSON_Delete(root);
    } else if (err == ESP_OK) {
        err = ESP_ERR_INVALID_RESPONSE;
    }
    if (err != ESP_OK) {
        /* Keep response text and credentials out of logs. */
        ESP_LOGW(TAG, "Gemini TTS failed: %s (HTTP %d, %u response bytes)",
                 esp_err_to_name(err), status, (unsigned)response.len);
    }
    heap_caps_free(response.data);
    return err;
}
