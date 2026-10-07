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

static const char *audio_data(cJSON *root)
{
    cJSON *output = cJSON_GetObjectItem(root, "output_audio");
    const char *data = cJSON_GetStringValue(cJSON_GetObjectItem(output, "data"));
    if (data) {
        return data;
    }

    /* Also accept the REST response shape shown in Google's cURL example. */
    cJSON *steps = cJSON_GetObjectItem(root, "steps");
    cJSON *step;
    cJSON_ArrayForEach(step, steps) {
        if (strcmp(cJSON_GetStringValue(cJSON_GetObjectItem(step, "type")) ?: "", "model_output")) {
            continue;
        }
        cJSON *content = cJSON_GetObjectItem(step, "content");
        cJSON *item;
        cJSON_ArrayForEach(item, content) {
            if (!strcmp(cJSON_GetStringValue(cJSON_GetObjectItem(item, "type")) ?: "", "audio")) {
                data = cJSON_GetStringValue(cJSON_GetObjectItem(item, "data"));
                if (data) {
                    return data;
                }
            }
        }
    }
    return NULL;
}

static esp_err_t decode_audio(const char *encoded, muse_gemini_pcm_cb_t cb, void *context)
{
    size_t len = strlen(encoded);
    if (!len || len > RESPONSE_MAX || (len & 3)) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    uint8_t decoded[PCM_CHUNK];
    for (size_t off = 0; off < len;) {
        size_t chunk = len - off < B64_CHUNK ? len - off : B64_CHUNK;
        size_t out = 0;
        int rc = mbedtls_base64_decode(decoded, sizeof(decoded), &out,
                                       (const unsigned char *)encoded + off, chunk);
        if (rc != 0 || (out & 1)) {
            return ESP_ERR_INVALID_RESPONSE;
        }
        if (out && !cb((const int16_t *)decoded, out / sizeof(int16_t), context)) {
            return ESP_ERR_INVALID_STATE;
        }
        off += chunk;
    }
    return ESP_OK;
}

static char *request_json(const char *text, const char *style)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }
    cJSON *input = cJSON_AddArrayToObject(root, "input");
    cJSON *turn = cJSON_CreateObject();
    cJSON *content = turn ? cJSON_AddArrayToObject(turn, "content") : NULL;
    cJSON *speech = cJSON_CreateObject();
    cJSON *annotations = speech ? cJSON_AddArrayToObject(speech, "annotations") : NULL;
    cJSON *annotation = cJSON_CreateObject();
    cJSON *format = cJSON_CreateObject();
    cJSON *generation = cJSON_CreateObject();
    cJSON *speech_config = generation ? cJSON_AddArrayToObject(generation, "speech_config") : NULL;
    cJSON *voice = cJSON_CreateObject();
    if (!input || !turn || !content || !speech || !annotations || !annotation || !format ||
        !generation || !speech_config || !voice) {
        cJSON_Delete(root);
        cJSON_Delete(turn);
        cJSON_Delete(speech);
        cJSON_Delete(annotation);
        cJSON_Delete(format);
        cJSON_Delete(generation);
        cJSON_Delete(voice);
        return NULL;
    }

    cJSON_AddStringToObject(root, "model", "gemini-3.8-flash-tts");
    cJSON_AddStringToObject(turn, "type", "user_input");
    cJSON_AddStringToObject(speech, "type", "text");
    cJSON_AddStringToObject(speech, "text", text);
    cJSON_AddStringToObject(annotation, "type", "speech_metadata");
    cJSON_AddStringToObject(annotation, "style", style);
    cJSON_AddItemToArray(annotations, annotation);
    cJSON_AddItemToArray(content, speech);
    cJSON_AddItemToArray(input, turn);
    cJSON_AddStringToObject(format, "type", "audio");
    cJSON_AddStringToObject(format, "mime_type", "audio/l16");
    cJSON_AddNumberToObject(format, "sample_rate", MIC_RATE);
    cJSON_AddItemToObject(root, "response_format", format);
    cJSON_AddStringToObject(voice, "voice", "Kore");
    cJSON_AddItemToArray(speech_config, voice);
    cJSON_AddItemToObject(root, "generation_config", generation);
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
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
    esp_http_client_config_t config = {
        .url = "https://generativelanguage.googleapis.com/v1beta/interactions",
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
        ESP_LOGW(TAG, "Gemini TTS HTTP status %d", status);
        err = ESP_FAIL;
    }
    if (err == ESP_OK && response.data) {
        cJSON *root = cJSON_ParseWithLength((const char *)response.data, response.len);
        const char *encoded = root ? audio_data(root) : NULL;
        err = encoded ? decode_audio(encoded, cb, context) : ESP_ERR_INVALID_RESPONSE;
        cJSON_Delete(root);
    } else if (err == ESP_OK) {
        err = ESP_ERR_INVALID_RESPONSE;
    }
    heap_caps_free(response.data);
    return err;
}
