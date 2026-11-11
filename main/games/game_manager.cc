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
        lv_screen_load(game_screen_);
        ESP_LOGI(TAG, "Started Tetris");
    } else if (game == GameType::kBreakout) {
        game_screen_ = BreakoutGame::CreateScreen();
        lv_screen_load(game_screen_);
        ESP_LOGI(TAG, "Started Breakout");
    } else if (game == GameType::kSnake) {
        game_screen_ = SnakeGame::CreateScreen();
        lv_screen_load(game_screen_);
        ESP_LOGI(TAG, "Started Snake");
    } else if (game == GameType::kFlappy) {
        game_screen_ = FlappyGame::CreateScreen();
        lv_screen_load(game_screen_);
        ESP_LOGI(TAG, "Started Flappy Bird");
    } else if (game == GameType::kInvaders) {
        game_screen_ = InvadersGame::CreateScreen();
        lv_screen_load(game_screen_);
        ESP_LOGI(TAG, "Started Space Invaders");
    } else if (game == GameType::kPong) {
        game_screen_ = PongGame::CreateScreen();
        lv_screen_load(game_screen_);
        ESP_LOGI(TAG, "Started Pong");
    } else if (game == GameType::kPuzzle2048) {
        game_screen_ = Puzzle2048Game::CreateScreen();
        lv_screen_load(game_screen_);
        ESP_LOGI(TAG, "Started 2048");
    } else if (game == GameType::kGameBoy) {
        game_screen_ = GameBoyApp::CreateScreen();
        lv_screen_load(game_screen_);
        ESP_LOGI(TAG, "Started GameBoy App");
    } else if (game == GameType::kSettings) {
        game_screen_ = SettingsUI::CreateScreen();
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
