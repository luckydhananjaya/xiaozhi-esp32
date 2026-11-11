#include "gameboy.h"
#include "game_manager.h"
#include <esp_log.h>
#include <esp_vfs_fat.h>
#include <driver/sdmmc_host.h>
#include <dirent.h>
#include <sys/stat.h>

#define ENABLE_LCD 1
#include "peanut_gb.h"

#define TAG "GameBoyApp"

GameBoyApp* GameBoyApp::instance_ = nullptr;

static void GameBoyExitBtnCb(lv_event_t* e) {
    GameManager::GetInstance().ExitCurrentGame();
}

static void GameBoyScanBtnCb(lv_event_t* e) {
    if (GameBoyApp::instance_) {
        GameBoyApp::instance_->ScanSDCard();
    }
}

lv_obj_t* GameBoyApp::CreateScreen() {
    Destroy();
    instance_ = new GameBoyApp();
    instance_->screen_ = lv_obj_create(nullptr);
    lv_obj_set_size(instance_->screen_, 320, 240);
    lv_obj_set_style_bg_color(instance_->screen_, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_pad_all(instance_->screen_, 0, 0);
    lv_obj_set_scrollbar_mode(instance_->screen_, LV_SCROLLBAR_MODE_OFF);

    instance_->InitUI(instance_->screen_);
    instance_->ScanSDCard();
    return instance_->screen_;
}

void GameBoyApp::Destroy() {
    if (instance_) {
        if (instance_->loop_timer_) {
            lv_timer_delete(instance_->loop_timer_);
            instance_->loop_timer_ = nullptr;
        }
        if (instance_->rom_buffer_) {
            free(instance_->rom_buffer_);
            instance_->rom_buffer_ = nullptr;
        }
        delete instance_;
        instance_ = nullptr;
    }
}

void GameBoyApp::InitUI(lv_obj_t* parent) {
    // Header (320 x 28)
    header_ = lv_obj_create(parent);
    lv_obj_set_size(header_, 320, 28);
    lv_obj_align(header_, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header_, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(header_, 0, 0);
    lv_obj_set_style_pad_all(header_, 0, 0);

    lv_obj_t* title = lv_label_create(header_);
    lv_label_set_text(title, "GAME BOY EMULATOR");
    lv_obj_set_style_text_color(title, lv_color_hex(0x818CF8), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 12, 0);

    lv_obj_t* exit_btn = lv_button_create(header);
    lv_obj_set_size(exit_btn, 60, 22);
    lv_obj_align(exit_btn, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_set_style_bg_color(exit_btn, lv_color_hex(0xDC2626), 0);
    lv_obj_set_style_radius(exit_btn, 4, 0);
    lv_obj_add_event_cb(exit_btn, GameBoyExitBtnCb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* exit_lbl = lv_label_create(exit_btn);
    lv_label_set_text(exit_lbl, "EXIT");
    lv_obj_set_style_text_color(exit_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(exit_lbl);

    // Status / Message Container
    status_label_ = lv_label_create(parent);
    lv_label_set_text(status_label_, "Scanning MicroSD Card (/sdcard/roms)...");
    lv_obj_set_style_text_color(status_label_, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(status_label_, LV_ALIGN_CENTER, 0, -20);

    // Rescan Button
    lv_obj_t* scan_btn = lv_button_create(parent);
    lv_obj_set_size(scan_btn, 110, 34);
    lv_obj_align(scan_btn, LV_ALIGN_CENTER, 0, 45);
    lv_obj_set_style_bg_color(scan_btn, lv_color_hex(0x4F46E5), 0);
    lv_obj_set_style_radius(scan_btn, 6, 0);
    lv_obj_add_event_cb(scan_btn, GameBoyScanBtnCb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* scan_lbl = lv_label_create(scan_btn);
    lv_label_set_text(scan_lbl, "RE-SCAN SD");
    lv_obj_set_style_text_color(scan_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(scan_lbl);
}

void GameBoyApp::ScanSDCard() {
    rom_files_.clear();
    DIR* dir = opendir("/sdcard/roms");
    if (!dir) {
        dir = opendir("/sdcard");
    }

    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if (name.size() > 3 && (name.substr(name.size() - 3) == ".gb" || name.substr(name.size() - 4) == ".gbc")) {
                rom_files_.push_back(name);
            }
        }
        closedir(dir);
    }

    if (rom_files_.empty()) {
        lv_label_set_text(status_label_,
            "NO .GB ROMS FOUND\n\n"
            "Insert MicroSD card with Game Boy ROMs into:\n"
            "/sdcard/roms/your_game.gb\n\n"
            "Then tap [RE-SCAN SD]");
        lv_obj_set_style_text_color(status_label_, lv_color_hex(0xF59E0B), 0);
    } else {
        std::string found_msg = "Found " + std::to_string(rom_files_.size()) + " ROM(s):\n";
        for (size_t i = 0; i < rom_files_.size() && i < 3; ++i) {
            found_msg += "- " + rom_files_[i] + "\n";
        }
        lv_label_set_text(status_label_, found_msg.c_str());
        lv_obj_set_style_text_color(status_label_, lv_color_hex(0x22C55E), 0);
    }
}

void GameBoyApp::LoadROM(const std::string& path) {
    // ROM loading logic for Peanut-GB
    ESP_LOGI(TAG, "Loading ROM: %s", path.c_str());
}

void GameBoyApp::HandleButton(uint8_t gb_btn, bool pressed) {
    // Input mapping
}

void GameBoyApp::LoopCallback(lv_timer_t* t) {
    // Peanut-GB frame execution
}
