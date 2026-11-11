#include "settings_ui.h"
#include "game_manager.h"
#include "screen_sleep_manager.h"
#include "boards/common/board.h"
#include "boards/common/backlight.h"
#include "audio/audio_codec.h"
#include <esp_log.h>
#include <stdio.h>

#define TAG "SettingsUI"

SettingsUI* SettingsUI::instance_ = nullptr;

void SettingsUI::ExitBtnCb(lv_event_t* e) {
    GameManager::GetInstance().ExitCurrentGame();
}

void SettingsUI::BrightnessSliderCb(lv_event_t* e) {
    if (!instance_) return;
    int val = lv_slider_get_value(instance_->brightness_slider_);
    instance_->SetBrightness(val);
}

void SettingsUI::VolumeSliderCb(lv_event_t* e) {
    if (!instance_) return;
    int val = lv_slider_get_value(instance_->volume_slider_);
    instance_->SetVolume(val);
}

void SettingsUI::BrightnessStepCb(lv_event_t* e) {
    if (!instance_) return;
    int step = reinterpret_cast<intptr_t>(lv_event_get_user_data(e));
    if (step == 999) {
        instance_->SetBrightness(100);
    } else {
        instance_->SetBrightness(instance_->current_brightness_ + step);
    }
}

void SettingsUI::VolumeStepCb(lv_event_t* e) {
    if (!instance_) return;
    int step = reinterpret_cast<intptr_t>(lv_event_get_user_data(e));
    if (step == 999) {
        instance_->SetVolume(0); // Mute
    } else {
        instance_->SetVolume(instance_->current_volume_ + step);
    }
}

lv_obj_t* SettingsUI::CreateScreen() {
    Destroy();
    instance_ = new SettingsUI();

    // Query current hardware levels
    auto bl = Board::GetInstance().GetBacklight();
    if (bl) {
        instance_->current_brightness_ = bl->brightness();
        if (instance_->current_brightness_ <= 0) instance_->current_brightness_ = 100;
    }
    auto codec = Board::GetInstance().GetAudioCodec();
    if (codec) {
        instance_->current_volume_ = codec->output_volume();
    }

    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_set_size(scr, 320, 240); // 320x240 Landscape
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x020617), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

    instance_->screen_ = scr;
    instance_->InitUI(scr);
    return scr;
}

void SettingsUI::Destroy() {
    if (instance_) {
        delete instance_;
        instance_ = nullptr;
    }
}

void SettingsUI::SetBrightness(int val) {
    val = ((val + 5) / 10) * 10;
    if (val < 10) val = 10;
    if (val > 100) val = 100;
    current_brightness_ = val;

    auto bl = Board::GetInstance().GetBacklight();
    if (bl) {
        bl->SetBrightness(static_cast<uint8_t>(current_brightness_), true);
    }

    if (brightness_slider_) {
        lv_slider_set_value(brightness_slider_, current_brightness_, LV_ANIM_OFF);
    }
    if (brightness_label_) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", current_brightness_);
        lv_label_set_text(brightness_label_, buf);
    }
    ESP_LOGI(TAG, "Screen brightness set to %d%%", current_brightness_);
}

void SettingsUI::SetVolume(int val) {
    val = ((val + 5) / 10) * 10;
    if (val < 0) val = 0;
    if (val > 100) val = 100;
    current_volume_ = val;

    auto codec = Board::GetInstance().GetAudioCodec();
    if (codec) {
        codec->SetOutputVolume(current_volume_);
    }

    if (volume_slider_) {
        lv_slider_set_value(volume_slider_, current_volume_, LV_ANIM_OFF);
    }
    if (volume_label_) {
        char buf[16];
        if (current_volume_ == 0) {
            snprintf(buf, sizeof(buf), "MUTE");
        } else {
            snprintf(buf, sizeof(buf), "%d%%", current_volume_);
        }
        lv_label_set_text(volume_label_, buf);
    }
    ESP_LOGI(TAG, "Speaker volume set to %d%%", current_volume_);
}

void SettingsUI::InitUI(lv_obj_t* scr) {
    // Header Bar (320 x 30)
    lv_obj_t* header = lv_obj_create(scr);
    lv_obj_set_size(header, 320, 30);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_scrollbar_mode(header, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "SETTINGS");
    lv_obj_set_style_text_color(title, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 12, 0);

    lv_obj_t* exit_btn = lv_button_create(header);
    lv_obj_set_size(exit_btn, 52, 22);
    lv_obj_align(exit_btn, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_set_style_bg_color(exit_btn, lv_color_hex(0xDC2626), 0);
    lv_obj_set_style_radius(exit_btn, 4, 0);
    lv_obj_add_event_cb(exit_btn, ExitBtnCb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* exit_lbl = lv_label_create(exit_btn);
    lv_label_set_text(exit_lbl, "EXIT");
    lv_obj_set_style_text_color(exit_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(exit_lbl);

    // 1. BRIGHTNESS & SLEEP CARD (x=8, y=34, w=148, h=198)
    lv_obj_t* b_card = lv_obj_create(scr);
    lv_obj_set_pos(b_card, 8, 34);
    lv_obj_set_size(b_card, 148, 198);
    lv_obj_set_style_bg_color(b_card, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_width(b_card, 2, 0);
    lv_obj_set_style_border_color(b_card, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_radius(b_card, 8, 0);
    lv_obj_set_style_pad_all(b_card, 6, 0);
    lv_obj_set_scrollbar_mode(b_card, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* b_title = lv_label_create(b_card);
    lv_label_set_text(b_title, "BRIGHTNESS");
    lv_obj_set_style_text_color(b_title, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(b_title, LV_ALIGN_TOP_LEFT, 2, 0);

    brightness_label_ = lv_label_create(b_card);
    char b_buf[16];
    snprintf(b_buf, sizeof(b_buf), "%d%%", current_brightness_);
    lv_label_set_text(brightness_label_, b_buf);
    lv_obj_set_style_text_color(brightness_label_, lv_color_hex(0xFACC15), 0);
    lv_obj_align(brightness_label_, LV_ALIGN_TOP_RIGHT, -2, 0);

    brightness_slider_ = lv_slider_create(b_card);
    lv_obj_set_pos(brightness_slider_, 4, 26);
    lv_obj_set_size(brightness_slider_, 128, 20);
    lv_slider_set_range(brightness_slider_, 10, 100);
    lv_slider_set_value(brightness_slider_, current_brightness_, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(brightness_slider_, lv_color_hex(0x1E293B), LV_PART_MAIN);
    lv_obj_set_style_bg_color(brightness_slider_, lv_color_hex(0x38BDF8), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(brightness_slider_, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    lv_obj_add_event_cb(brightness_slider_, BrightnessSliderCb, LV_EVENT_VALUE_CHANGED, nullptr);

    // Steppers: [-10%], [MAX], [+10%]
    auto create_b_step = [b_card](const char* text, int x, int w, intptr_t step) {
        lv_obj_t* btn = lv_button_create(b_card);
        lv_obj_set_pos(btn, x, 54);
        lv_obj_set_size(btn, w, 32);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1E293B), 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x38BDF8), 0);
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_add_event_cb(btn, BrightnessStepCb, LV_EVENT_CLICKED, reinterpret_cast<void*>(step));

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, text);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(lbl);
    };
    create_b_step("-10%", 2, 40, -10);
    create_b_step("MAX", 46, 44, 999);
    create_b_step("+10%", 94, 40, 10);

    // Sleep Timeout Header
    lv_obj_t* sleep_title = lv_label_create(b_card);
    lv_label_set_text(sleep_title, "STANDBY TIMEOUT");
    lv_obj_set_style_text_color(sleep_title, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_pos(sleep_title, 2, 92);

    // Timeout Buttons: [15s], [30s], [60s], [OFF]
    int cur_timeout = ScreenSleepManager::GetInstance().GetTimeout();
    auto create_timeout_btn = [b_card, cur_timeout](const char* text, int x, int w, int sec) {
        lv_obj_t* btn = lv_button_create(b_card);
        lv_obj_set_pos(btn, x, 112);
        lv_obj_set_size(btn, w, 28);
        bool is_active = (cur_timeout == sec);
        lv_obj_set_style_bg_color(btn, lv_color_hex(is_active ? 0x0284C7 : 0x1E293B), 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(is_active ? 0x38BDF8 : 0x475569), 0);
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_add_event_cb(btn, [](lv_event_t* e) {
            int s = reinterpret_cast<intptr_t>(lv_event_get_user_data(e));
            ScreenSleepManager::GetInstance().SetTimeout(s);
            // Refresh settings screen to update active button highlight
            GameManager::GetInstance().ShowSettings();
        }, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<intptr_t>(sec)));

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, text);
        lv_obj_set_style_text_color(lbl, lv_color_hex(is_active ? 0xFFFFFF : 0xCBD5E1), 0);
        lv_obj_center(lbl);
    };
    create_timeout_btn("15s", 2, 30, 15);
    create_timeout_btn("30s", 34, 30, 30);
    create_timeout_btn("60s", 66, 30, 60);
    create_timeout_btn("OFF", 98, 38, 0);

    // Instant Sleep Button
    lv_obj_t* sleep_now_btn = lv_button_create(b_card);
    lv_obj_set_pos(sleep_now_btn, 4, 148);
    lv_obj_set_size(sleep_now_btn, 130, 32);
    lv_obj_set_style_bg_color(sleep_now_btn, lv_color_hex(0x312E81), 0); // Indigo-900
    lv_obj_set_style_border_width(sleep_now_btn, 1, 0);
    lv_obj_set_style_border_color(sleep_now_btn, lv_color_hex(0x818CF8), 0);
    lv_obj_set_style_radius(sleep_now_btn, 6, 0);
    lv_obj_add_event_cb(sleep_now_btn, [](lv_event_t* e) {
        ScreenSleepManager::GetInstance().Sleep();
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* sleep_now_lbl = lv_label_create(sleep_now_btn);
    lv_label_set_text(sleep_now_lbl, "SLEEP NOW");
    lv_obj_set_style_text_color(sleep_now_lbl, lv_color_hex(0xEEF2FF), 0);
    lv_obj_center(sleep_now_lbl);

    // 2. VOLUME CARD (x=164, y=34, w=148, h=198)
    lv_obj_t* v_card = lv_obj_create(scr);
    lv_obj_set_pos(v_card, 164, 34);
    lv_obj_set_size(v_card, 148, 198);
    lv_obj_set_style_bg_color(v_card, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_width(v_card, 2, 0);
    lv_obj_set_style_border_color(v_card, lv_color_hex(0x4ADE80), 0);
    lv_obj_set_style_radius(v_card, 8, 0);
    lv_obj_set_style_pad_all(v_card, 6, 0);
    lv_obj_set_scrollbar_mode(v_card, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* v_title = lv_label_create(v_card);
    lv_label_set_text(v_title, "VOLUME");
    lv_obj_set_style_text_color(v_title, lv_color_hex(0x4ADE80), 0);
    lv_obj_align(v_title, LV_ALIGN_TOP_LEFT, 2, 0);

    volume_label_ = lv_label_create(v_card);
    char v_buf[16];
    if (current_volume_ == 0) {
        snprintf(v_buf, sizeof(v_buf), "MUTE");
    } else {
        snprintf(v_buf, sizeof(v_buf), "%d%%", current_volume_);
    }
    lv_label_set_text(volume_label_, v_buf);
    lv_obj_set_style_text_color(volume_label_, lv_color_hex(0xFACC15), 0);
    lv_obj_align(volume_label_, LV_ALIGN_TOP_RIGHT, -2, 0);

    volume_slider_ = lv_slider_create(v_card);
    lv_obj_set_pos(volume_slider_, 4, 26);
    lv_obj_set_size(volume_slider_, 128, 20);
    lv_slider_set_range(volume_slider_, 0, 100);
    lv_slider_set_value(volume_slider_, current_volume_, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(volume_slider_, lv_color_hex(0x1E293B), LV_PART_MAIN);
    lv_obj_set_style_bg_color(volume_slider_, lv_color_hex(0x22C55E), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(volume_slider_, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    lv_obj_add_event_cb(volume_slider_, VolumeSliderCb, LV_EVENT_VALUE_CHANGED, nullptr);

    // Steppers: [-10%], [MUTE], [+10%]
    auto create_v_step = [v_card](const char* text, int x, int w, intptr_t step) {
        lv_obj_t* btn = lv_button_create(v_card);
        lv_obj_set_pos(btn, x, 54);
        lv_obj_set_size(btn, w, 32);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1E293B), 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x4ADE80), 0);
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_add_event_cb(btn, VolumeStepCb, LV_EVENT_CLICKED, reinterpret_cast<void*>(step));

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, text);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(lbl);
    };
    create_v_step("-10%", 2, 40, -10);
    create_v_step("MUTE", 46, 44, 999);
    create_v_step("+10%", 94, 40, 10);

    // Presets Header
    lv_obj_t* preset_title = lv_label_create(v_card);
    lv_label_set_text(preset_title, "QUICK PRESETS");
    lv_obj_set_style_text_color(preset_title, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_pos(preset_title, 2, 92);

    // Presets 30%, 50%, 80%, 100%
    auto create_v_preset = [v_card](const char* text, int x, int w, intptr_t level) {
        lv_obj_t* btn = lv_button_create(v_card);
        lv_obj_set_pos(btn, x, 112);
        lv_obj_set_size(btn, w, 28);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1E293B), 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x16A34A), 0);
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_add_event_cb(btn, [](lv_event_t* e) {
            int lvl = reinterpret_cast<intptr_t>(lv_event_get_user_data(e));
            if (SettingsUI::instance_) SettingsUI::instance_->SetVolume(lvl);
        }, LV_EVENT_CLICKED, reinterpret_cast<void*>(level));

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, text);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x4ADE80), 0);
        lv_obj_center(lbl);
    };
    create_v_preset("30%", 2, 30, 30);
    create_v_preset("50%", 34, 30, 50);
    create_v_preset("80%", 66, 30, 80);
    create_v_preset("MAX", 98, 38, 100);

    // Status Badge
    lv_obj_t* status_box = lv_obj_create(v_card);
    lv_obj_set_pos(status_box, 4, 148);
    lv_obj_set_size(status_box, 130, 32);
    lv_obj_set_style_bg_color(status_box, lv_color_hex(0x064E3B), 0); // Emerald-900
    lv_obj_set_style_border_width(status_box, 1, 0);
    lv_obj_set_style_border_color(status_box, lv_color_hex(0x34D399), 0);
    lv_obj_set_style_radius(status_box, 6, 0);
    lv_obj_set_style_pad_all(status_box, 0, 0);

    lv_obj_t* status_lbl = lv_label_create(status_box);
    lv_label_set_text(status_lbl, "OPTIMUS ONLINE");
    lv_obj_set_style_text_color(status_lbl, lv_color_hex(0xD1FAE5), 0);
    lv_obj_center(status_lbl);
}
