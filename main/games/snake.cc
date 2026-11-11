#include "snake.h"
#include "game_manager.h"
#include <esp_log.h>
#include <esp_random.h>

#define TAG "SnakeGame"

SnakeGame* SnakeGame::instance_ = nullptr;

static void SnakeExitBtnCb(lv_event_t* e) {
    GameManager::GetInstance().ExitCurrentGame();
}

static void SnakeControlBtnCb(lv_event_t* e) {
    if (!SnakeGame::instance_) return;
    int dir = reinterpret_cast<intptr_t>(lv_event_get_user_data(e));
    switch (dir) {
        case 0: SnakeGame::instance_->ChangeDirection(0, -1); break; // Up
        case 1: SnakeGame::instance_->ChangeDirection(0, 1);  break; // Down
        case 2: SnakeGame::instance_->ChangeDirection(-1, 0); break; // Left
        case 3: SnakeGame::instance_->ChangeDirection(1, 0);  break; // Right
    }
}

lv_obj_t* SnakeGame::CreateScreen() {
    Destroy();
    instance_ = new SnakeGame();
    instance_->screen_ = lv_obj_create(nullptr);
    lv_obj_set_size(instance_->screen_, 240, 320);
    lv_obj_set_style_bg_color(instance_->screen_, lv_color_hex(0x0A0F1D), 0);
    lv_obj_set_style_pad_all(instance_->screen_, 0, 0);
    lv_obj_set_scrollbar_mode(instance_->screen_, LV_SCROLLBAR_MODE_OFF);

    instance_->InitUI(instance_->screen_);
    instance_->Start();
    return instance_->screen_;
}

void SnakeGame::Destroy() {
    if (instance_) {
        instance_->Stop();
        delete instance_;
        instance_ = nullptr;
    }
}

void SnakeGame::InitUI(lv_obj_t* parent) {
    // Top Bar (240 x 28)
    lv_obj_t* top_bar = lv_obj_create(parent);
    lv_obj_set_size(top_bar, 240, 28);
    lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_style_pad_all(top_bar, 0, 0);
    lv_obj_set_scrollbar_mode(top_bar, LV_SCROLLBAR_MODE_OFF);

    score_label_ = lv_label_create(top_bar);
    lv_label_set_text(score_label_, "SNAKE | SCORE: 0");
    lv_obj_set_style_text_color(score_label_, lv_color_hex(0xFACC15), 0);
    lv_obj_align(score_label_, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t* exit_btn = lv_button_create(top_bar);
    lv_obj_set_size(exit_btn, 52, 22);
    lv_obj_align(exit_btn, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_set_style_bg_color(exit_btn, lv_color_hex(0xDC2626), 0);
    lv_obj_set_style_radius(exit_btn, 4, 0);
    lv_obj_add_event_cb(exit_btn, SnakeExitBtnCb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* exit_lbl = lv_label_create(exit_btn);
    lv_label_set_text(exit_lbl, "EXIT");
    lv_obj_set_style_text_color(exit_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(exit_lbl);

    // Board container (220 x 220 px) at x=10, y=32
    board_ = lv_obj_create(parent);
    lv_obj_set_pos(board_, 10, 32);
    lv_obj_set_size(board_, kGridWidth * kCellSize + 4, kGridHeight * kCellSize + 4);
    lv_obj_set_style_bg_color(board_, lv_color_hex(0x020617), 0);
    lv_obj_set_style_border_width(board_, 2, 0);
    lv_obj_set_style_border_color(board_, lv_color_hex(0x334155), 0);
    lv_obj_set_style_pad_all(board_, 0, 0);
    lv_obj_set_scrollbar_mode(board_, LV_SCROLLBAR_MODE_OFF);

    for (int r = 0; r < kGridHeight; ++r) {
        for (int c = 0; c < kGridWidth; ++c) {
            lv_obj_t* cell = lv_obj_create(board_);
            lv_obj_set_pos(cell, c * kCellSize, r * kCellSize);
            lv_obj_set_size(cell, kCellSize - 1, kCellSize - 1);
            lv_obj_set_style_bg_color(cell, lv_color_hex(0x020617), 0);
            lv_obj_set_style_border_width(cell, 0, 0);
            lv_obj_set_style_radius(cell, 2, 0);
            cells_[r][c] = cell;
        }
    }

    game_over_label_ = lv_label_create(board_);
    lv_label_set_text(game_over_label_, "GAME OVER!\nTAP ANY DIR");
    lv_obj_set_style_text_color(game_over_label_, lv_color_hex(0xEF4444), 0);
    lv_obj_set_style_text_align(game_over_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(game_over_label_);
    lv_obj_add_flag(game_over_label_, LV_OBJ_FLAG_HIDDEN);

    // Controls D-Pad at bottom (y=254..314)
    auto create_dir_btn = [parent](const char* label, int x, int y, int dir) {
        lv_obj_t* btn = lv_button_create(parent);
        lv_obj_set_pos(btn, x, y);
        lv_obj_set_size(btn, 44, 28);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x334155), 0);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_add_event_cb(btn, SnakeControlBtnCb, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<intptr_t>(dir)));

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, label);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xF8FAFC), 0);
        lv_obj_center(lbl);
        return btn;
    };

    create_dir_btn("^", 98, 254, 0);   // UP
    create_dir_btn("<", 48, 284, 2);   // LEFT
    create_dir_btn("v", 98, 284, 1);   // DOWN
    create_dir_btn(">", 148, 284, 3);  // RIGHT
}

void SnakeGame::Start() {
    ResetGame();
    timer_ = lv_timer_create(TimerCallback, 140, this);
}

void SnakeGame::Stop() {
    if (timer_) {
        lv_timer_delete(timer_);
        timer_ = nullptr;
    }
}

void SnakeGame::ResetGame() {
    snake_.clear();
    snake_.push_back({5, 7});
    snake_.push_back({4, 7});
    snake_.push_back({3, 7});
    dir_x_ = 1; dir_y_ = 0;
    next_dir_x_ = 1; next_dir_y_ = 0;
    score_ = 0;
    game_over_ = false;

    if (game_over_label_) lv_obj_add_flag(game_over_label_, LV_OBJ_FLAG_HIDDEN);
    if (score_label_) lv_label_set_text_fmt(score_label_, "SNAKE | SCORE: %d", score_);

    SpawnFood();
    Render();
}

void SnakeGame::SpawnFood() {
    while (true) {
        food_.x = esp_random() % kGridWidth;
        food_.y = esp_random() % kGridHeight;
        bool on_snake = false;
        for (const auto& p : snake_) {
            if (p == food_) { on_snake = true; break; }
        }
        if (!on_snake) break;
    }
}

void SnakeGame::ChangeDirection(int dx, int dy) {
    if (game_over_) {
        ResetGame();
        return;
    }
    // Prevent 180-degree turn
    if (dx != 0 && dir_x_ == -dx) return;
    if (dy != 0 && dir_y_ == -dy) return;
    next_dir_x_ = dx;
    next_dir_y_ = dy;
}

void SnakeGame::Step() {
    if (game_over_) return;

    dir_x_ = next_dir_x_;
    dir_y_ = next_dir_y_;

    Point head = snake_.front();
    head.x += dir_x_;
    head.y += dir_y_;

    // Wall collision
    if (head.x < 0 || head.x >= kGridWidth || head.y < 0 || head.y >= kGridHeight) {
        game_over_ = true;
        lv_obj_remove_flag(game_over_label_, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    // Self collision
    for (const auto& p : snake_) {
        if (p == head) {
            game_over_ = true;
            lv_obj_remove_flag(game_over_label_, LV_OBJ_FLAG_HIDDEN);
            return;
        }
    }

    snake_.push_front(head);

    if (head == food_) {
        score_ += 10;
        lv_label_set_text_fmt(score_label_, "SNAKE | SCORE: %d", score_);
        SpawnFood();
    } else {
        snake_.pop_back();
    }

    Render();
}

void SnakeGame::Render() {
    for (int r = 0; r < kGridHeight; ++r) {
        for (int c = 0; c < kGridWidth; ++c) {
            lv_obj_set_style_bg_color(cells_[r][c], lv_color_hex(0x020617), 0);
        }
    }

    // Food
    if (food_.y >= 0 && food_.y < kGridHeight && food_.x >= 0 && food_.x < kGridWidth) {
        lv_obj_set_style_bg_color(cells_[food_.y][food_.x], lv_color_hex(0xEF4444), 0);
    }

    // Snake
    bool first = true;
    for (const auto& p : snake_) {
        if (p.y >= 0 && p.y < kGridHeight && p.x >= 0 && p.x < kGridWidth) {
            lv_obj_set_style_bg_color(cells_[p.y][p.x], first ? lv_color_hex(0x22C55E) : lv_color_hex(0x16A34A), 0);
        }
        first = false;
    }
}

void SnakeGame::TimerCallback(lv_timer_t* t) {
    auto self = static_cast<SnakeGame*>(lv_timer_get_user_data(t));
    if (self) self->Step();
}
