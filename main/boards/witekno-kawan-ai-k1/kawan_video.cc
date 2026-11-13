#include "kawan_video.h"

#include "audio_codec.h"
#include "board.h"
#include "display.h"
#include "settings.h"

#include <esp_audio_simple_dec.h>
#include <esp_audio_simple_dec_default.h>
#include <esp_heap_caps.h>
#include <esp_jpeg_dec.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <vector>

#define TAG "KawanVideo"

namespace {
std::atomic<bool> playing{false};
std::string active_url;
lv_obj_t* screen = nullptr;
lv_obj_t* canvas = nullptr;
lv_obj_t* status = nullptr;
uint8_t* canvas_buf = nullptr;
constexpr int kView = 240;

std::string SavedUrl() {
    Settings settings("video", false);
    return settings.GetString("url");
}

void SetStatus(const char* text) {
    if (status == nullptr) {
        return;
    }
    DisplayLockGuard lock(Board::GetInstance().GetDisplay());
    lv_label_set_text(status, text);
}

bool DecodeFrame(const uint8_t* jpeg, int length) {
    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
    config.output_type = JPEG_PIXEL_FORMAT_RGB565_LE;
    config.scale.width = kView;
    config.scale.height = kView;
    jpeg_dec_handle_t decoder = nullptr;
    if (jpeg_dec_open(&config, &decoder) != JPEG_ERR_OK) {
        return false;
    }
    jpeg_dec_io_t io = {};
    io.inbuf = const_cast<uint8_t*>(jpeg);
    io.inbuf_len = length;
    jpeg_dec_header_info_t header = {};
    jpeg_error_t err = jpeg_dec_parse_header(decoder, &io, &header);
    if (err != JPEG_ERR_OK) {
        jpeg_dec_close(decoder);
        config.scale.width = 0;
        config.scale.height = 0;
        if (jpeg_dec_open(&config, &decoder) != JPEG_ERR_OK) {
            return false;
        }
        io.inbuf = const_cast<uint8_t*>(jpeg);
        io.inbuf_len = length;
        err = jpeg_dec_parse_header(decoder, &io, &header);
        if (err != JPEG_ERR_OK) {
            jpeg_dec_close(decoder);
            return false;
        }
    }
    const int out_w = header.width > 0 ? header.width : kView;
    const int out_h = header.height > 0 ? header.height : kView;
    if (out_w > 640 || out_h > 640) {
        jpeg_dec_close(decoder);
        return false;
    }
    uint8_t* rgb = static_cast<uint8_t*>(
        heap_caps_aligned_alloc(16, static_cast<size_t>(out_w) * out_h * 2, MALLOC_CAP_SPIRAM));
    if (rgb == nullptr) {
        jpeg_dec_close(decoder);
        return false;
    }
    io.outbuf = rgb;
    err = jpeg_dec_process(decoder, &io);
    jpeg_dec_close(decoder);
    if (err != JPEG_ERR_OK) {
        heap_caps_free(rgb);
        return false;
    }
    auto* dst = reinterpret_cast<uint16_t*>(canvas_buf);
    const auto* src = reinterpret_cast<uint16_t*>(rgb);
    for (int y = 0; y < kView; ++y) {
        const int sy = y * out_h / kView;
        for (int x = 0; x < kView; ++x) {
            const int sx = x * out_w / kView;
            dst[y * kView + x] = src[sy * out_w + sx];
        }
    }
    heap_caps_free(rgb);
    DisplayLockGuard lock(Board::GetInstance().GetDisplay());
    if (canvas != nullptr) {
        lv_obj_invalidate(canvas);
    }
    return true;
}

void PlayPcm(const int16_t* samples, int count);

void AudioTask(void* arg) {
    std::string url = *static_cast<std::string*>(arg);
    delete static_cast<std::string*>(arg);
    auto http = Board::GetInstance().GetNetwork()->CreateHttp(0);
    http->SetTimeout(8000);
    if (auto opened = http->Open("GET", url); !opened) {
        vTaskDelete(nullptr);
        return;
    }
    const bool mp3 = url.find(".mp3") != std::string::npos;
    esp_audio_simple_dec_handle_t decoder = nullptr;
    if (mp3) {
        esp_audio_simple_dec_register_default();
        esp_audio_simple_dec_cfg_t cfg = {};
        cfg.dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_MP3;
        esp_audio_simple_dec_open(&cfg, &decoder);
    }
    uint8_t chunk[1024];
    std::vector<uint8_t> pending;
    uint8_t pcm_store[4096];
    while (playing) {
        auto read = http->Read(reinterpret_cast<char*>(chunk), sizeof(chunk));
        if (!read || *read <= 0) {
            break;
        }
        if (!mp3) {
            PlayPcm(reinterpret_cast<int16_t*>(chunk), *read / 2);
            continue;
        }
        pending.insert(pending.end(), chunk, chunk + *read);
        esp_audio_simple_dec_raw_t raw = {};
        raw.buffer = pending.data();
        raw.len = pending.size();
        esp_audio_simple_dec_out_t out = {};
        out.buffer = pcm_store;
        out.len = sizeof(pcm_store);
        if (decoder != nullptr && esp_audio_simple_dec_process(decoder, &raw, &out) == ESP_AUDIO_ERR_OK &&
            out.decoded_size > 0) {
            PlayPcm(reinterpret_cast<int16_t*>(pcm_store), out.decoded_size / 2);
        }
        if (raw.consumed > 0 && raw.consumed <= pending.size()) {
            pending.erase(pending.begin(), pending.begin() + raw.consumed);
        } else if (pending.size() > 16 * 1024) {
            pending.clear();
        }
    }
    if (decoder != nullptr) {
        esp_audio_simple_dec_close(decoder);
    }
    http->Close();
    vTaskDelete(nullptr);
}

void PlayPcm(const int16_t* samples, int count) {
    auto* codec = Board::GetInstance().GetAudioCodec();
    if (codec == nullptr || count <= 0) {
        return;
    }
    codec->EnableOutput(true);
    std::vector<int16_t> frame(samples, samples + count);
    codec->OutputData(frame);
}

void PlayerTask(void* arg) {
    std::string url = *static_cast<std::string*>(arg);
    delete static_cast<std::string*>(arg);
    auto http = Board::GetInstance().GetNetwork()->CreateHttp(0);
    http->SetTimeout(8000);
    if (auto opened = http->Open("GET", url); !opened) {
        SetStatus("Open failed");
        playing = false;
        vTaskDelete(nullptr);
        return;
    }
    SetStatus("Playing");
    std::vector<uint8_t> pending;
    uint8_t chunk[1024];
    int64_t next_frame_us = esp_timer_get_time();
    while (playing) {
        auto read = http->Read(reinterpret_cast<char*>(chunk), sizeof(chunk));
        if (!read || *read <= 0) {
            break;
        }
        pending.insert(pending.end(), chunk, chunk + *read);
        if (pending.size() > 256 * 1024) {
            pending.erase(pending.begin(), pending.begin() + (pending.size() - 128 * 1024));
        }
        static const uint8_t kSoi[] = {0xff, 0xd8};
        static const uint8_t kEoi[] = {0xff, 0xd9};
        auto start = std::search(pending.begin(), pending.end(), kSoi, kSoi + 2);
        auto end = std::search(start, pending.end(), kEoi, kEoi + 2);
        if (start == pending.end() || end == pending.end()) {
            continue;
        }
        end += 2;
        const int64_t now = esp_timer_get_time();
        if (now >= next_frame_us) {
            DecodeFrame(&*start, static_cast<int>(end - start));
            next_frame_us = now + (1000000 / 12);
        }
        // audio/l16 parts are raw little-endian mono samples between JPEGs
        // only when the server prefixes them with 'P','C','M', rate is 24000.
        pending.erase(pending.begin(), end);
    }
    http->Close();
    playing = false;
    SetStatus("Stopped");
    vTaskDelete(nullptr);
}

void ShowScreen() {
    DisplayLockGuard lock(Board::GetInstance().GetDisplay());
    if (screen == nullptr) {
        screen = lv_obj_create(lv_layer_top());
        lv_obj_remove_style_all(screen);
        lv_obj_set_size(screen, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
        canvas_buf = static_cast<uint8_t*>(
            heap_caps_malloc(kView * kView * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        canvas = lv_canvas_create(screen);
        lv_canvas_set_buffer(canvas, canvas_buf, kView, kView, LV_COLOR_FORMAT_RGB565);
        lv_obj_center(canvas);
        status = lv_label_create(screen);
        lv_obj_set_style_text_color(status, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(status, LV_ALIGN_BOTTOM_MID, 0, -4);
    }
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_HIDDEN);
}
}  // namespace

namespace KawanVideo {
void Start(const std::string& url) {
    std::string use = url.empty() ? SavedUrl() : url;
    if (!url.empty()) {
        Settings settings("video", true);
        settings.SetString("url", url);
    }
    if (use.empty()) {
        ShowScreen();
        SetStatus("Set video URL in NVS");
        return;
    }
    Stop();
    ShowScreen();
    active_url = use;
    playing = true;
    auto* copy = new std::string(use);
    xTaskCreate(PlayerTask, "kawan_mjpeg", 8192, copy, 4, nullptr);
    Settings audio("video", false);
    std::string audio_url = audio.GetString("audio_url");
    if (!audio_url.empty()) {
        auto* audio_copy = new std::string(audio_url);
        xTaskCreate(AudioTask, "kawan_audio", 8192, audio_copy, 4, nullptr);
    }
}

void Stop() {
    playing = false;
    vTaskDelay(pdMS_TO_TICKS(50));
    DisplayLockGuard lock(Board::GetInstance().GetDisplay());
    if (screen != nullptr) {
        lv_obj_add_flag(screen, LV_OBJ_FLAG_HIDDEN);
    }
}

bool Playing() { return playing; }

std::string Url() { return active_url.empty() ? SavedUrl() : active_url; }
}  // namespace KawanVideo
