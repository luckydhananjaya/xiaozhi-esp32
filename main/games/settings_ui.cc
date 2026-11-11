#include "settings_ui.h"
#include "game_manager.h"
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
    lv_obj_set_size(scr, 240, 320);
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
    // Snap to 10% steps
    val = ((val + 5) / 10) * 10;
    if (val < 10) val = 10; // Prevent screen turning completely black
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
    // Snap to 10% steps
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
    // Header Bar (240 x 30)
    lv_obj_t* header = lv_obj_create(scr);
    lv_obj_set_size(header, 240, 30);
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
    lv_obj_set_size(exit_btn, 54, 22);
    lv_obj_align(exit_btn, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_set_style_bg_color(exit_btn, lv_color_hex(0xDC2626), 0);
    lv_obj_set_style_radius(exit_btn, 4, 0);
    lv_obj_add_event_cb(exit_btn, ExitBtnCb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* exit_lbl = lv_label_create(exit_btn);
    lv_label_set_text(exit_lbl, "EXIT");
    lv_obj_set_style_text_color(exit_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(exit_lbl);

    // 1. BRIGHTNESS CARD (y=36, w=224, h=124, x=8)
    lv_obj_t* b_card = lv_obj_create(scr);
    lv_obj_set_pos(b_card, 8, 36);
    lv_obj_set_size(b_card, 224, 124);
    lv_obj_set_style_bg_color(b_card, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_width(b_card, 2, 0);
    lv_obj_set_style_border_color(b_card, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_radius(b_card, 8, 0);
    lv_obj_set_style_pad_all(b_card, 8, 0);
    lv_obj_set_scrollbar_mode(b_card, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* b_title = lv_label_create(b_card);
    lv_label_set_text(b_title, "BRIGHTNESS");
    lv_obj_set_style_text_color(b_title, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(b_title, LV_ALIGN_TOP_LEFT, 4, 0);

    brightness_label_ = lv_label_create(b_card);
    char b_buf[16];
    snprintf(b_buf, sizeof(b_buf), "%d%%", current_brightness_);
    lv_label_set_text(brightness_label_, b_buf);
    lv_obj_set_style_text_color(brightness_label_, lv_color_hex(0xFACC15), 0);
    lv_obj_align(brightness_label_, LV_ALIGN_TOP_RIGHT, -4, 0);

    brightness_slider_ = lv_slider_create(b_card);
    lv_obj_set_pos(brightness_slider_, 4, 28);
    lv_obj_set_size(brightness_slider_, 200, 20);
    lv_slider_set_range(brightness_slider_, 10, 100);
    lv_slider_set_value(brightness_slider_, current_brightness_, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(brightness_slider_, lv_color_hex(0x1E293B), LV_PART_MAIN);
    lv_obj_set_style_bg_color(brightness_slider_, lv_color_hex(0x38BDF8), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(brightness_slider_, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    lv_obj_add_event_cb(brightness_slider_, BrightnessSliderCb, LV_EVENT_VALUE_CHANGED, nullptr);

    // Steppers: [-10%], [MAX 100%], [+10%]
    auto create_b_step = [b_card](const char* text, int x, int w, intptr_t step) {
        lv_obj_t* btn = lv_button_create(b_card);
        lv_obj_set_pos(btn, x, 60);
        lv_obj_set_size(btn, w, 42);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1E293B), 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x38BDF8), 0);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_add_event_cb(btn, BrightnessStepCb, LV_EVENT_CLICKED, reinterpret_cast<void*>(step));

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, text);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xF8FAFC), 0);
        lv_obj_center(lbl);
    };

    create_b_step("-10%", 4, 60, -10);
    create_b_step("100%", 70, 68, 999);
    create_b_step("+10%", 144, 60, 10);

    // 2. VOLUME CARD (y=168, w=224, h=124, x=8)
    lv_obj_t* v_card = lv_obj_create(scr);
    lv_obj_set_pos(v_card, 8, 168);
    lv_obj_set_size(v_card, 224, 124);
    lv_obj_set_style_bg_color(v_card, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_width(v_card, 2, 0);
    lv_obj_set_style_border_color(v_card, lv_color_hex(0x22C55E), 0);
    lv_obj_set_style_radius(v_card, 8, 0);
    lv_obj_set_style_pad_all(v_card, 8, 0);
    lv_obj_set_scrollbar_mode(v_card, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* v_title = lv_label_create(v_card);
    lv_label_set_text(v_title, "VOLUME");
    lv_obj_set_style_text_color(v_title, lv_color_hex(0x22C55E), 0);
    lv_obj_align(v_title, LV_ALIGN_TOP_LEFT, 4, 0);

    volume_label_ = lv_label_create(v_card);
    char v_buf[16];
    if (current_volume_ == 0) snprintf(v_buf, sizeof(v_buf), "MUTE");
    else snprintf(v_buf, sizeof(v_buf), "%d%%", current_volume_);
    lv_label_set_text(volume_label_, v_buf);
    lv_obj_set_style_text_color(volume_label_, lv_color_hex(0x22C55E), 0);
    lv_obj_align(volume_label_, LV_ALIGN_TOP_RIGHT, -4, 0);

    volume_slider_ = lv_slider_create(v_card);
    lv_obj_set_pos(volume_slider_, 4, 28);
    lv_obj_set_size(volume_slider_, 200, 20);
    lv_slider_set_range(volume_slider_, 0, 100);
    lv_slider_set_value(volume_slider_, current_volume_, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(volume_slider_, lv_color_hex(0x1E293B), LV_PART_MAIN);
    lv_obj_set_style_bg_color(volume_slider_, lv_color_hex(0x22C55E), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(volume_slider_, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    lv_obj_add_event_cb(volume_slider_, VolumeSliderCb, LV_EVENT_VALUE_CHANGED, nullptr);

    // Steppers: [-10%], [MUTE 0%], [+10%]
    auto create_v_step = [v_card](const char* text, int x, int w, intptr_t step) {
        lv_obj_t* btn = lv_button_create(v_card);
        lv_obj_set_pos(btn, x, 60);
        lv_obj_set_size(btn, w, 42);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1E293B), 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x22C55E), 0);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_add_event_cb(btn, VolumeStepCb, LV_EVENT_CLICKED, reinterpret_cast<void*>(step));

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, text);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xF8FAFC), 0);
        lv_obj_center(lbl);
    };

    create_v_step("-10%", 4, 60, -10);
    create_v_step("MUTE", 70, 68, 999);
    create_v_step("+10%", 144, 60, 10);

    // Footer
    lv_obj_t* footer = lv_label_create(scr);
    lv_label_set_text(footer, "HOSYOND ESP32-S3 • OPTIMUS PRIME");
    lv_obj_set_style_text_color(footer, lv_color_hex(0x64748B), 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -8);
}
