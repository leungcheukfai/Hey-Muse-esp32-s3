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

#include "muse_hf_tts.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"

#include "muse_settings.h"

static const char *TAG = "muse_hf_tts";

#define HF_TTS_URL "https://router.huggingface.co/fal-ai/fal-ai/kokoro/american-english"
#define HF_TTS_VOICE "af_heart"
#define HF_JSON_MAX (16 * 1024)
#define HF_WAV_MAX (8 * 1024 * 1024)
#define WAV_HEADER_BYTES 44
#define WAV_SAMPLE_RATE 24000
#define MIC_RATE 16000
#define PCM_BATCH_FRAMES 1024

typedef struct {
    uint8_t *data;
    size_t len;
    size_t cap;
    bool overflow;
} response_t;

static esp_err_t collect_json(esp_http_client_event_t *event)
{
    response_t *response = event->user_data;
    if (event->event_id != HTTP_EVENT_ON_DATA || event->data_len <= 0 || !response) {
        return ESP_OK;
    }
    size_t add = (size_t)event->data_len;
    if (response->len + add > HF_JSON_MAX) {
        response->overflow = true;
        return ESP_ERR_INVALID_SIZE;
    }
    size_t need = response->len + add + 1;
    if (need > response->cap) {
        size_t cap = response->cap ? response->cap : 1024;
        while (cap < need) {
            cap *= 2;
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

typedef struct {
    uint32_t step;
    uint32_t pos;
} pcm_resampler_t;

typedef struct {
    uint8_t header[WAV_HEADER_BYTES];
    size_t header_len;
    size_t bytes_seen;
    uint32_t audio_bytes_left;
    bool header_ready;
    bool have_pending_byte;
    uint8_t pending_byte;
    int16_t pcm[PCM_BATCH_FRAMES];
    size_t pcm_frames;
    pcm_resampler_t resampler;
    muse_hf_pcm_cb_t cb;
    void *context;
    esp_err_t error;
} wav_stream_t;

static uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static bool parse_wav_header(wav_stream_t *wav)
{
    const uint8_t *h = wav->header;
    if (memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4) || memcmp(h + 12, "fmt ", 4) ||
        read_le32(h + 16) != 16 || read_le16(h + 20) != 1 || read_le16(h + 22) != 1 ||
        read_le32(h + 24) != WAV_SAMPLE_RATE || read_le32(h + 28) != WAV_SAMPLE_RATE * 2 ||
        read_le16(h + 32) != 2 || read_le16(h + 34) != 16 || memcmp(h + 36, "data", 4)) {
        return false;
    }
    uint32_t data_bytes = read_le32(h + 40);
    uint64_t riff_bytes = (uint64_t)read_le32(h + 4) + 8;
    if (!data_bytes || (data_bytes & 1) || data_bytes > HF_WAV_MAX - WAV_HEADER_BYTES ||
        riff_bytes < WAV_HEADER_BYTES + data_bytes) {
        return false;
    }
    wav->audio_bytes_left = data_bytes;
    wav->header_ready = true;
    wav->resampler.step = ((uint32_t)WAV_SAMPLE_RATE << 16) / MIC_RATE;
    wav->resampler.pos = 0;
    return true;
}

static esp_err_t emit_pcm(wav_stream_t *wav)
{
    if (!wav->pcm_frames) {
        return ESP_OK;
    }
    int16_t output[PCM_BATCH_FRAMES + 4];
    size_t out_frames = 0;
    while ((wav->resampler.pos >> 16) < wav->pcm_frames) {
        output[out_frames++] = wav->pcm[wav->resampler.pos >> 16];
        wav->resampler.pos += wav->resampler.step;
    }
    wav->resampler.pos -= (uint32_t)(wav->pcm_frames << 16);
    wav->pcm_frames = 0;
    if (out_frames && !wav->cb(output, out_frames, wav->context)) {
        wav->error = ESP_ERR_INVALID_STATE;
        return wav->error;
    }
    return ESP_OK;
}

static esp_err_t wav_feed(wav_stream_t *wav, const uint8_t *data, size_t len)
{
    if (wav->error != ESP_OK) {
        return wav->error;
    }
    if (len > HF_WAV_MAX - wav->bytes_seen) {
        wav->error = ESP_ERR_INVALID_SIZE;
        return wav->error;
    }
    wav->bytes_seen += len;

    if (!wav->header_ready) {
        size_t needed = WAV_HEADER_BYTES - wav->header_len;
        size_t take = len < needed ? len : needed;
        memcpy(wav->header + wav->header_len, data, take);
        wav->header_len += take;
        data += take;
        len -= take;
        if (wav->header_len < WAV_HEADER_BYTES) {
            return ESP_OK;
        }
        if (!parse_wav_header(wav)) {
            wav->error = ESP_ERR_INVALID_RESPONSE;
            return wav->error;
        }
    }

    while (len && wav->audio_bytes_left) {
        uint8_t byte = *data++;
        len--;
        wav->audio_bytes_left--;
        if (!wav->have_pending_byte) {
            wav->pending_byte = byte;
            wav->have_pending_byte = true;
            continue;
        }
        wav->pcm[wav->pcm_frames++] = (int16_t)((uint16_t)wav->pending_byte | ((uint16_t)byte << 8));
        wav->have_pending_byte = false;
        if (wav->pcm_frames == PCM_BATCH_FRAMES && emit_pcm(wav) != ESP_OK) {
            return wav->error;
        }
    }
    return ESP_OK;
}

static esp_err_t stream_wav(esp_http_client_event_t *event)
{
    wav_stream_t *wav = event->user_data;
    if (event->event_id != HTTP_EVENT_ON_DATA || event->data_len <= 0 || !wav) {
        return ESP_OK;
    }
    return wav_feed(wav, event->data, (size_t)event->data_len);
}

static char *request_json(const char *text)
{
    cJSON *root = cJSON_CreateObject();
    if (!root || !cJSON_AddStringToObject(root, "text", text) ||
        !cJSON_AddStringToObject(root, "voice", HF_TTS_VOICE)) {
        cJSON_Delete(root);
        return NULL;
    }
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

static char *audio_url_from_response(const response_t *response)
{
    if (!response || !response->data || !response->len) {
        return NULL;
    }
    cJSON *root = cJSON_ParseWithLength((const char *)response->data, response->len);
    cJSON *audio = root ? cJSON_GetObjectItemCaseSensitive(root, "audio") : NULL;
    const char *url = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(audio, "url"));
    char *copy = url && !strncmp(url, "https://", 8) ? strdup(url) : NULL;
    cJSON_Delete(root);
    return copy;
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

esp_err_t muse_hf_tts_generate(const char *token, const char *text, muse_hf_pcm_cb_t cb, void *context)
{
    if (!token || !token[0] || !text || !text[0] || !cb) {
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

    response_t response = {0};
    esp_http_client_config_t config = {
        .url = HF_TTS_URL,
        .event_handler = collect_json,
        .user_data = &response,
        .timeout_ms = 90000,
        .buffer_size = 2048,
        .buffer_size_tx = 2048,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = client ? esp_http_client_set_method(client, HTTP_METHOD_POST) : ESP_ERR_NO_MEM;
    char authorization[MUSE_HF_TOKEN_MAX + 8] = {0};
    snprintf(authorization, sizeof(authorization), "Bearer %s", token);
    if (err == ESP_OK) err = esp_http_client_set_header(client, "Content-Type", "application/json");
    if (err == ESP_OK) err = esp_http_client_set_header(client, "Authorization", authorization);
    if (err == ESP_OK) err = esp_http_client_set_header(client, "Accept", "application/json");
    if (err == ESP_OK) err = esp_http_client_set_post_field(client, json, (int)json_len);
    ESP_LOGI(TAG, "requesting Kokoro English speech through Hugging Face");
    if (err == ESP_OK) err = esp_http_client_perform(client);
    int status = client ? esp_http_client_get_status_code(client) : 0;
    if (client) {
        esp_http_client_cleanup(client);
    }
    memset(authorization, 0, sizeof(authorization));
    wipe_free(json);

    if (err == ESP_OK && response.overflow) {
        err = ESP_ERR_INVALID_SIZE;
    }
    if (err == ESP_OK && (status < 200 || status >= 300)) {
        ESP_LOGW(TAG, "Hugging Face TTS request returned HTTP %d", status);
        err = ESP_FAIL;
    }
    char *audio_url = err == ESP_OK ? audio_url_from_response(&response) : NULL;
    if (err == ESP_OK && !audio_url) {
        err = ESP_ERR_INVALID_RESPONSE;
    }
    heap_caps_free(response.data);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Hugging Face TTS request failed: %s", esp_err_to_name(err));
        free(audio_url);
        return err;
    }

    wav_stream_t wav = {.cb = cb, .context = context, .error = ESP_OK};
    config = (esp_http_client_config_t){
        .url = audio_url,
        .event_handler = stream_wav,
        .user_data = &wav,
        .timeout_ms = 90000,
        .buffer_size = 4096,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    client = esp_http_client_init(&config);
    err = client ? esp_http_client_perform(client) : ESP_ERR_NO_MEM;
    status = client ? esp_http_client_get_status_code(client) : 0;
    if (client) {
        esp_http_client_cleanup(client);
    }
    free(audio_url);

    if (err == ESP_OK && (status < 200 || status >= 300)) {
        ESP_LOGW(TAG, "Hugging Face audio download returned HTTP %d", status);
        err = ESP_FAIL;
    }
    if (err == ESP_OK && wav.error != ESP_OK) {
        err = wav.error;
    }
    if (err == ESP_OK && (!wav.header_ready || wav.audio_bytes_left || wav.have_pending_byte)) {
        err = ESP_ERR_INVALID_RESPONSE;
    }
    if (err == ESP_OK) {
        err = emit_pcm(&wav);
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Hugging Face audio decode failed: %s", esp_err_to_name(err));
    }
    return err;
}
