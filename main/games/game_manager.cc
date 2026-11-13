#include "game_manager.h"
#include "game_launcher.h"
#include "tetris.h"
#include "breakout.h"
#include "snake.h"
#include "flappy.h"
#include "invaders.h"
#include "pong.h"
#include "puzzle2048.h"
#include "gameboy.h"
#include "settings_ui.h"
#include "application.h"
#include <esp_log.h>

#define TAG "GameManager"

// The arcade screens are laid out at 320x240. On that size this is a no-op, so
// the Hosyond board is unchanged. Smaller panels (this fork's 284x240 board)
// get a uniform scale from the top-left.
static void FitArcadeScreen(lv_obj_t* scr) {
    if (scr == nullptr) {
        return;
    }
    lv_display_t* disp = lv_display_get_default();
    if (disp == nullptr) {
        return;
    }
    const int32_t hor = lv_display_get_horizontal_resolution(disp);
    const int32_t ver = lv_display_get_vertical_resolution(disp);
    if (hor >= 320 && ver >= 240) {
        return;
    }
    int32_t zoom = (hor * 256) / 320;
    const int32_t zoom_y = (ver * 256) / 240;
    if (zoom_y < zoom) {
        zoom = zoom_y;
    }
    if (zoom < 64) {
        zoom = 64;
    }
    lv_obj_set_style_transform_pivot_x(scr, 0, 0);
    lv_obj_set_style_transform_pivot_y(scr, 0, 0);
    lv_obj_set_style_transform_scale(scr, zoom, 0);
}

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
    if (games_btn_ || !chatbot_screen_) return;

    // High-visibility Games Button on chatbot screen at bottom-right (x=216, y=186, w=96, h=46)
    games_btn_ = lv_button_create(chatbot_screen_);
    lv_display_t* disp = lv_display_get_default();
    const int32_t hor = disp ? lv_display_get_horizontal_resolution(disp) : 320;
    const int32_t ver = disp ? lv_display_get_vertical_resolution(disp) : 240;
    if (hor >= 320 && ver >= 240) {
        lv_obj_set_pos(games_btn_, 216, 186);
        lv_obj_set_size(games_btn_, 96, 46);
    } else {
        lv_obj_set_size(games_btn_, 72, 36);
        lv_obj_align(games_btn_, LV_ALIGN_BOTTOM_RIGHT, -4, -4);
    }
    lv_obj_set_style_bg_color(games_btn_, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_bg_opa(games_btn_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(games_btn_, 2, 0);
    lv_obj_set_style_border_color(games_btn_, lv_color_hex(0x00E5FF), 0); // Neon Cyan border
    lv_obj_set_style_radius(games_btn_, 8, 0);
    lv_obj_set_style_pad_all(games_btn_, 0, 0);
    lv_obj_move_foreground(games_btn_);
    lv_obj_add_event_cb(games_btn_, OnFloatingGamesBtnClicked, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* lbl = lv_label_create(games_btn_);
    lv_label_set_text(lbl, "GAMES");
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x00E5FF), 0);
    lv_obj_center(lbl);
}

void GameManager::ShowLauncher() {
    if (!chatbot_screen_) {
        chatbot_screen_ = lv_screen_active();
    }
    if (games_btn_) {
        lv_obj_add_flag(games_btn_, LV_OBJ_FLAG_HIDDEN);
    }

    if (!launcher_screen_) {
        launcher_screen_ = GameLauncher::CreateScreen();
    }

    in_launcher_ = true;
    current_game_ = GameType::kNone;
    FitArcadeScreen(launcher_screen_);
    lv_screen_load(launcher_screen_);
    ESP_LOGI(TAG, "Switched to Game Launcher (320x240)");
}

void GameManager::ShowSettings() {
    if (!chatbot_screen_) {
        chatbot_screen_ = lv_screen_active();
    }
    if (games_btn_) {
        lv_obj_add_flag(games_btn_, LV_OBJ_FLAG_HIDDEN);
    }
    StartGame(GameType::kSettings);
}

void GameManager::StartGame(GameType game) {
    current_game_ = game;
    in_launcher_ = false;
    if (games_btn_) {
        lv_obj_add_flag(games_btn_, LV_OBJ_FLAG_HIDDEN);
    }

    if (game == GameType::kTetris) {
        game_screen_ = TetrisGame::CreateScreen();
        FitArcadeScreen(game_screen_);
        lv_screen_load(game_screen_);
        ESP_LOGI(TAG, "Started Tetris");
    } else if (game == GameType::kBreakout) {
        game_screen_ = BreakoutGame::CreateScreen();
        FitArcadeScreen(game_screen_);
        lv_screen_load(game_screen_);
        ESP_LOGI(TAG, "Started Breakout");
    } else if (game == GameType::kSnake) {
        game_screen_ = SnakeGame::CreateScreen();
        FitArcadeScreen(game_screen_);
        lv_screen_load(game_screen_);
        ESP_LOGI(TAG, "Started Snake");
    } else if (game == GameType::kFlappy) {
        game_screen_ = FlappyGame::CreateScreen();
        FitArcadeScreen(game_screen_);
        lv_screen_load(game_screen_);
        ESP_LOGI(TAG, "Started Flappy Bird");
    } else if (game == GameType::kInvaders) {
        game_screen_ = InvadersGame::CreateScreen();
        FitArcadeScreen(game_screen_);
        lv_screen_load(game_screen_);
        ESP_LOGI(TAG, "Started Space Invaders");
    } else if (game == GameType::kPong) {
        game_screen_ = PongGame::CreateScreen();
        FitArcadeScreen(game_screen_);
        lv_screen_load(game_screen_);
        ESP_LOGI(TAG, "Started Pong");
    } else if (game == GameType::kPuzzle2048) {
        game_screen_ = Puzzle2048Game::CreateScreen();
        FitArcadeScreen(game_screen_);
        lv_screen_load(game_screen_);
        ESP_LOGI(TAG, "Started 2048");
    } else if (game == GameType::kGameBoy) {
        game_screen_ = GameBoyApp::CreateScreen();
        FitArcadeScreen(game_screen_);
        lv_screen_load(game_screen_);
        ESP_LOGI(TAG, "Started GameBoy App");
    } else if (game == GameType::kSettings) {
        game_screen_ = SettingsUI::CreateScreen();
        FitArcadeScreen(game_screen_);
        lv_screen_load(game_screen_);
        ESP_LOGI(TAG, "Started Settings Screen");
    }
}

void GameManager::ExitCurrentGame() {
    if (current_game_ == GameType::kTetris) {
        TetrisGame::Destroy();
    } else if (current_game_ == GameType::kBreakout) {
        BreakoutGame::Destroy();
    } else if (current_game_ == GameType::kSnake) {
        SnakeGame::Destroy();
    } else if (current_game_ == GameType::kFlappy) {
        FlappyGame::Destroy();
    } else if (current_game_ == GameType::kInvaders) {
        InvadersGame::Destroy();
    } else if (current_game_ == GameType::kPong) {
        PongGame::Destroy();
    } else if (current_game_ == GameType::kPuzzle2048) {
        Puzzle2048Game::Destroy();
    } else if (current_game_ == GameType::kGameBoy) {
        GameBoyApp::Destroy();
    } else if (current_game_ == GameType::kSettings) {
        SettingsUI::Destroy();
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
    if (games_btn_) {
        lv_obj_remove_flag(games_btn_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(games_btn_);
    }
}
