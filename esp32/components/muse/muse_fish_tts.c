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

#include "muse_fish_tts.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"

#include "muse_settings.h"

static const char *TAG = "muse_fish_tts";

#define FISH_TTS_URL "https://api.fish.audio/v1/tts"
#define FISH_TTS_MODEL "s2.1-pro-free"
#define FISH_TTS_REFERENCE_ID "1df12c4bb692423283fde2bdc7f84093"
#define FISH_TTS_RATE 16000
#define FISH_AUDIO_MAX (8 * 1024 * 1024)
#define PCM_BATCH_FRAMES 1024
#define HTTP_READ_BYTES 2048

typedef struct {
    muse_fish_pcm_cb_t cb;
    void *context;
    int16_t pcm[PCM_BATCH_FRAMES];
    size_t frames;
    size_t bytes;
    bool have_pending_byte;
    uint8_t pending_byte;
    esp_err_t error;
} pcm_stream_t;

static esp_err_t emit_pcm(pcm_stream_t *stream)
{
    if (!stream->frames) {
        return ESP_OK;
    }
    if (!stream->cb(stream->pcm, stream->frames, stream->context)) {
        stream->error = ESP_ERR_INVALID_STATE;
        return stream->error;
    }
    stream->frames = 0;
    return ESP_OK;
}

static esp_err_t feed_pcm(pcm_stream_t *stream, const uint8_t *data, size_t len)
{
    if (len > FISH_AUDIO_MAX - stream->bytes) {
        stream->error = ESP_ERR_INVALID_SIZE;
        return stream->error;
    }
    stream->bytes += len;

    for (size_t i = 0; i < len; i++) {
        if (!stream->have_pending_byte) {
            stream->pending_byte = data[i];
            stream->have_pending_byte = true;
            continue;
        }
        stream->pcm[stream->frames++] = (int16_t)((uint16_t)stream->pending_byte | ((uint16_t)data[i] << 8));
        stream->have_pending_byte = false;
        if (stream->frames == PCM_BATCH_FRAMES && emit_pcm(stream) != ESP_OK) {
            return stream->error;
        }
    }
    return ESP_OK;
}

static char *request_json(const char *text)
{
    cJSON *root = cJSON_CreateObject();
    if (!root || !cJSON_AddStringToObject(root, "text", text) ||
        !cJSON_AddStringToObject(root, "reference_id", FISH_TTS_REFERENCE_ID) ||
        !cJSON_AddStringToObject(root, "format", "pcm") ||
        !cJSON_AddNumberToObject(root, "sample_rate", FISH_TTS_RATE)) {
        cJSON_Delete(root);
        return NULL;
    }
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

static esp_err_t write_all(esp_http_client_handle_t client, const char *data, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        int n = esp_http_client_write(client, data + sent, (int)(len - sent));
        if (n <= 0) {
            return ESP_FAIL;
        }
        sent += (size_t)n;
    }
    return ESP_OK;
}

esp_err_t muse_fish_tts_generate(const char *api_key, const char *text, muse_fish_pcm_cb_t cb, void *context)
{
    if (!api_key || !api_key[0] || !text || !text[0] || !cb) {
        return ESP_ERR_INVALID_ARG;
    }

    char *json = request_json(text);
    if (!json) {
        return ESP_ERR_NO_MEM;
    }
    size_t json_len = strlen(json);
    if (json_len > INT_MAX) {
        wipe_free(json);
        return ESP_ERR_INVALID_SIZE;
    }

    pcm_stream_t stream = {.cb = cb, .context = context, .error = ESP_OK};
    esp_http_client_config_t config = {
        .url = FISH_TTS_URL,
        .timeout_ms = 90000,
        .buffer_size = HTTP_READ_BYTES,
        .buffer_size_tx = 2048,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = client ? esp_http_client_set_method(client, HTTP_METHOD_POST) : ESP_ERR_NO_MEM;
    char authorization[MUSE_FISH_API_KEY_MAX + 8] = {0};
    snprintf(authorization, sizeof(authorization), "Bearer %s", api_key);
    if (err == ESP_OK) err = esp_http_client_set_header(client, "Content-Type", "application/json");
    if (err == ESP_OK) err = esp_http_client_set_header(client, "Authorization", authorization);
    if (err == ESP_OK) err = esp_http_client_set_header(client, "model", FISH_TTS_MODEL);
    if (err == ESP_OK) err = esp_http_client_open(client, (int)json_len);
    if (err == ESP_OK) err = write_all(client, json, json_len);
    wipe_free(json);

    int64_t response_len = -1;
    int status = 0;
    if (err == ESP_OK) {
        response_len = esp_http_client_fetch_headers(client);
        status = esp_http_client_get_status_code(client);
        if (response_len < 0) {
            err = ESP_FAIL;
        } else if (status < 200 || status >= 300) {
            ESP_LOGW(TAG, "Fish Audio TTS request returned HTTP %d", status);
            err = ESP_FAIL;
        } else if (response_len > (int64_t)FISH_AUDIO_MAX) {
            err = ESP_ERR_INVALID_SIZE;
        }
    }
    memset(authorization, 0, sizeof(authorization));

    uint8_t buffer[HTTP_READ_BYTES];
    size_t received = 0;
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "requesting speech from Fish Audio %s", FISH_TTS_MODEL);
        while (response_len == 0 || received < (size_t)response_len) {
            int n = esp_http_client_read(client, (char *)buffer, sizeof(buffer));
            if (n < 0) {
                err = ESP_FAIL;
                break;
            }
            if (!n) {
                if (response_len > 0 && received != (size_t)response_len) {
                    err = ESP_ERR_INVALID_RESPONSE;
                }
                break;
            }
            received += (size_t)n;
            err = feed_pcm(&stream, buffer, (size_t)n);
            if (err != ESP_OK) {
                break;
            }
        }
    }

    if (client) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
    }
    if (err == ESP_OK && stream.error != ESP_OK) {
        err = stream.error;
    }
    if (err == ESP_OK && (!stream.bytes || stream.have_pending_byte)) {
        err = ESP_ERR_INVALID_RESPONSE;
    }
    if (err == ESP_OK && response_len > 0 && received != (size_t)response_len) {
        err = ESP_ERR_INVALID_RESPONSE;
    }
    if (err == ESP_OK) {
        err = emit_pcm(&stream);
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Fish Audio TTS failed: %s", esp_err_to_name(err));
    }
    return err;
}
