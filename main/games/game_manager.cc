#include "game_manager.h"
#include "game_launcher.h"
#include "tetris.h"
#include "breakout.h"
#include "application.h"
#include <esp_log.h>

#define TAG "GameManager"

GameManager& GameManager::GetInstance() {
    static GameManager instance;
    return instance;
}

GameManager::GameManager() {}

GameManager::~GameManager() {}

static void OnFloatingGamesBtnClicked(lv_event_t* e) {
    GameManager::GetInstance().ShowLauncher();
}

void GameManager::Init() {
    chatbot_screen_ = lv_screen_active();
    CreateFloatingGamesButton();
    ESP_LOGI(TAG, "GameManager initialized");
}

void GameManager::CreateFloatingGamesButton() {
    if (!chatbot_screen_ || games_btn_) return;

    // Stylish Floating Games Button at bottom-right (x=245, y=198, w=68, h=32)
    games_btn_ = lv_button_create(chatbot_screen_);
    lv_obj_set_pos(games_btn_, 242, 196);
    lv_obj_set_size(games_btn_, 70, 36);
    lv_obj_set_style_bg_color(games_btn_, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(games_btn_, 2, 0);
    lv_obj_set_style_border_color(games_btn_, lv_color_hex(0x38BDF8), 0); // Cyan border
    lv_obj_set_style_radius(games_btn_, 8, 0);
    lv_obj_set_style_pad_all(games_btn_, 0, 0);
    lv_obj_add_event_cb(games_btn_, OnFloatingGamesBtnClicked, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* lbl = lv_label_create(games_btn_);
    lv_label_set_text(lbl, "GAMES");
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x38BDF8), 0);
    lv_obj_center(lbl);
}

void GameManager::ShowLauncher() {
    if (!chatbot_screen_) {
        chatbot_screen_ = lv_screen_active();
    }

    if (!launcher_screen_) {
        launcher_screen_ = GameLauncher::CreateScreen();
    }

    in_launcher_ = true;
    current_game_ = GameType::kNone;
    lv_screen_load(launcher_screen_);
    ESP_LOGI(TAG, "Switched to Game Launcher");
}

void GameManager::StartGame(GameType game) {
    current_game_ = game;
    in_launcher_ = false;

    if (game == GameType::kTetris) {
        game_screen_ = TetrisGame::CreateScreen();
        lv_screen_load(game_screen_);
        ESP_LOGI(TAG, "Started Tetris");
    } else if (game == GameType::kBreakout) {
        game_screen_ = BreakoutGame::CreateScreen();
        lv_screen_load(game_screen_);
        ESP_LOGI(TAG, "Started Breakout");
    }
}

void GameManager::ExitCurrentGame() {
    if (current_game_ == GameType::kTetris) {
        TetrisGame::Destroy();
    } else if (current_game_ == GameType::kBreakout) {
        BreakoutGame::Destroy();
    }

    current_game_ = GameType::kNone;
    game_screen_ = nullptr;
    ShowLauncher();
}

void GameManager::ExitToChatbot() {
    in_launcher_ = false;
    current_game_ = GameType::kNone;
    if (chatbot_screen_) {
        lv_screen_load(chatbot_screen_);
        ESP_LOGI(TAG, "Returned to Optimus Prime Chatbot screen");
    }
}
