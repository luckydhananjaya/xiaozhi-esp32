#include "breakout.h"
#include "game_manager.h"
#include <esp_random.h>
#include <esp_log.h>
#include <stdio.h>
#include <cmath>

#define TAG "Breakout"

BreakoutGame* BreakoutGame::instance_ = nullptr;

static const uint32_t kRowColors[5] = {
    0xEF4444, // Red (Row 0)
    0xF97316, // Orange (Row 1)
    0xEAB308, // Yellow (Row 2)
    0x22C55E, // Green (Row 3)
    0x06B6D4  // Cyan (Row 4)
};

static void OnBreakoutExitClicked(lv_event_t* e) {
    if (BreakoutGame::instance_) {
        BreakoutGame::instance_->Stop();
    }
    GameManager::GetInstance().ExitCurrentGame();
}

lv_obj_t* BreakoutGame::CreateScreen() {
    Destroy();
    instance_ = new BreakoutGame();

    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_set_size(scr, 320, 240);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x050814), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

    instance_->screen_ = scr;
    instance_->InitUI(scr);
    instance_->Start();
    return scr;
}

void BreakoutGame::Destroy() {
    if (instance_) {
        instance_->Stop();
        delete instance_;
        instance_ = nullptr;
    }
}

void BreakoutGame::InitUI(lv_obj_t* scr) {
    // Top Bar
    lv_obj_t* top_bar = lv_obj_create(scr);
    lv_obj_set_size(top_bar, 320, 32);
    lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_style_pad_all(top_bar, 0, 0);
    lv_obj_set_style_pad_left(top_bar, 10, 0);
    lv_obj_set_style_pad_right(top_bar, 8, 0);
    lv_obj_set_scrollbar_mode(top_bar, LV_SCROLLBAR_MODE_OFF);

    score_label_ = lv_label_create(top_bar);
    lv_label_set_text(score_label_, "SCORE: 0");
    lv_obj_set_style_text_color(score_label_, lv_color_hex(0xF8FAFC), 0);
    lv_obj_align(score_label_, LV_ALIGN_LEFT_MID, 0, 0);

    lives_label_ = lv_label_create(top_bar);
    lv_label_set_text(lives_label_, "LIVES: 3");
    lv_obj_set_style_text_color(lives_label_, lv_color_hex(0xEF4444), 0);
    lv_obj_align(lives_label_, LV_ALIGN_CENTER, -10, 0);

    // Red EXIT button
    lv_obj_t* exit_btn = lv_button_create(top_bar);
    lv_obj_set_size(exit_btn, 60, 24);
    lv_obj_align(exit_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(exit_btn, lv_color_hex(0xDC2626), 0);
    lv_obj_set_style_radius(exit_btn, 4, 0);
    lv_obj_add_event_cb(exit_btn, OnBreakoutExitClicked, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* exit_lbl = lv_label_create(exit_btn);
    lv_label_set_text(exit_lbl, "EXIT");
    lv_obj_set_style_text_color(exit_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(exit_lbl);

    // Create Bricks (5 rows x 8 cols)
    const int b_w = 34;
    const int b_h = 10;
    const int start_x = 10;
    const int start_y = 38;

    for (int r = 0; r < kBrickRows; ++r) {
        for (int c = 0; c < kBrickCols; ++c) {
            lv_obj_t* b = lv_obj_create(scr);
            lv_obj_set_pos(b, start_x + c * 38, start_y + r * 14);
            lv_obj_set_size(b, b_w, b_h);
            lv_obj_set_style_bg_color(b, lv_color_hex(kRowColors[r]), 0);
            lv_obj_set_style_radius(b, 2, 0);
            lv_obj_set_style_border_width(b, 0, 0);
            lv_obj_set_style_pad_all(b, 0, 0);
            bricks_[r][c] = b;
            brick_alive_[r][c] = true;
        }
    }

    // Paddle
    paddle_obj_ = lv_obj_create(scr);
    lv_obj_set_pos(paddle_obj_, paddle_x_, 195);
    lv_obj_set_size(paddle_obj_, kPaddleWidth, kPaddleHeight);
    lv_obj_set_style_bg_color(paddle_obj_, lv_color_hex(0x38BDF8), 0); // Cyan
    lv_obj_set_style_radius(paddle_obj_, 4, 0);
    lv_obj_set_style_border_width(paddle_obj_, 0, 0);
    lv_obj_set_style_pad_all(paddle_obj_, 0, 0);

    // Ball
    ball_obj_ = lv_obj_create(scr);
    lv_obj_set_pos(ball_obj_, static_cast<int>(ball_x_), static_cast<int>(ball_y_));
    lv_obj_set_size(ball_obj_, kBallSize, kBallSize);
    lv_obj_set_style_bg_color(ball_obj_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(ball_obj_, 4, 0);
    lv_obj_set_style_border_width(ball_obj_, 0, 0);
    lv_obj_set_style_pad_all(ball_obj_, 0, 0);

    // Message label (Game Over / You Win)
    message_label_ = lv_label_create(scr);
    lv_label_set_text(message_label_, "");
    lv_obj_set_style_text_color(message_label_, lv_color_hex(0xEAB308), 0);
    lv_obj_align(message_label_, LV_ALIGN_CENTER, 0, 30);

    // Bottom Touch Area (Touch anywhere at y >= 150 to drag paddle)
    touch_area_ = lv_obj_create(scr);
    lv_obj_set_pos(touch_area_, 0, 150);
    lv_obj_set_size(touch_area_, 320, 90);
    lv_obj_set_style_bg_opa(touch_area_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(touch_area_, 0, 0);
    lv_obj_set_style_pad_all(touch_area_, 0, 0);
    lv_obj_add_event_cb(touch_area_, TouchEventCallback, LV_EVENT_PRESSING, this);
    lv_obj_add_event_cb(touch_area_, TouchEventCallback, LV_EVENT_PRESSED, this);
}

void BreakoutGame::TouchEventCallback(lv_event_t* e) {
    auto* game = static_cast<BreakoutGame*>(lv_event_get_user_data(e));
    if (!game) return;

    lv_indev_t* indev = lv_indev_active();
    if (indev) {
        lv_point_t p;
        lv_indev_get_point(indev, &p);
        game->SetPaddleX(p.x - kPaddleWidth / 2);
    }
}

void BreakoutGame::SetPaddleX(int x) {
    if (x < 4) x = 4;
    if (x > 320 - kPaddleWidth - 4) x = 320 - kPaddleWidth - 4;
    paddle_x_ = x;
    lv_obj_set_pos(paddle_obj_, paddle_x_, 195);
}

void BreakoutGame::ResetBall() {
    ball_x_ = paddle_x_ + kPaddleWidth / 2 - kBallSize / 2;
    ball_y_ = 175.0f;
    ball_dx_ = (esp_random() % 2 == 0) ? 2.5f : -2.5f;
    ball_dy_ = -2.8f;
    lv_obj_set_pos(ball_obj_, static_cast<int>(ball_x_), static_cast<int>(ball_y_));
}

void BreakoutGame::ResetGame() {
    score_ = 0;
    lives_ = 3;
    game_over_ = false;
    for (int r = 0; r < kBrickRows; ++r) {
        for (int c = 0; c < kBrickCols; ++c) {
            brick_alive_[r][c] = true;
            lv_obj_remove_flag(bricks_[r][c], LV_OBJ_FLAG_HIDDEN);
        }
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "SCORE: %d", score_);
    lv_label_set_text(score_label_, buf);
    snprintf(buf, sizeof(buf), "LIVES: %d", lives_);
    lv_label_set_text(lives_label_, buf);
    lv_label_set_text(message_label_, "");
    SetPaddleX(132);
    ResetBall();
}

void BreakoutGame::Start() {
    ResetGame();
    if (!timer_) {
        timer_ = lv_timer_create(TimerCallback, 25, this); // 40 FPS
    }
}

void BreakoutGame::Stop() {
    if (timer_) {
        lv_timer_delete(timer_);
        timer_ = nullptr;
    }
}

void BreakoutGame::TimerCallback(lv_timer_t* t) {
    auto* game = static_cast<BreakoutGame*>(lv_timer_get_user_data(t));
    if (game && !game->game_over_) {
        game->UpdatePhysics();
    }
}

void BreakoutGame::UpdatePhysics() {
    ball_x_ += ball_dx_;
    ball_y_ += ball_dy_;

    // Wall bounce
    if (ball_x_ <= 4) {
        ball_x_ = 4;
        ball_dx_ = -ball_dx_;
    } else if (ball_x_ >= 320 - kBallSize - 4) {
        ball_x_ = 320 - kBallSize - 4;
        ball_dx_ = -ball_dx_;
    }

    if (ball_y_ <= 34) {
        ball_y_ = 34;
        ball_dy_ = -ball_dy_;
    }

    // Paddle collision
    if (ball_y_ + kBallSize >= 195 && ball_y_ <= 205) {
        if (ball_x_ + kBallSize >= paddle_x_ && ball_x_ <= paddle_x_ + kPaddleWidth) {
            ball_y_ = 195 - kBallSize;
            float hit_pos = (ball_x_ + kBallSize / 2.0f) - (paddle_x_ + kPaddleWidth / 2.0f);
            float norm = hit_pos / (kPaddleWidth / 2.0f);
            ball_dx_ = norm * 3.5f;
            ball_dy_ = -std::abs(ball_dy_);
        }
    }

    // Bottom loss
    if (ball_y_ > 225) {
        lives_--;
        char buf[32];
        snprintf(buf, sizeof(buf), "LIVES: %d", lives_);
        lv_label_set_text(lives_label_, buf);

        if (lives_ <= 0) {
            game_over_ = true;
            lv_label_set_text(message_label_, "GAME OVER!");
            return;
        } else {
            ResetBall();
            return;
        }
    }

    // Brick collision
    const int b_w = 34;
    const int b_h = 10;
    const int start_x = 10;
    const int start_y = 38;

    bool all_cleared = true;
    for (int r = 0; r < kBrickRows; ++r) {
        for (int c = 0; c < kBrickCols; ++c) {
            if (brick_alive_[r][c]) {
                all_cleared = false;
                int bx = start_x + c * 38;
                int by = start_y + r * 14;

                if (ball_x_ + kBallSize >= bx && ball_x_ <= bx + b_w &&
                    ball_y_ + kBallSize >= by && ball_y_ <= by + b_h) {
                    brick_alive_[r][c] = false;
                    lv_obj_add_flag(bricks_[r][c], LV_OBJ_FLAG_HIDDEN);
                    ball_dy_ = -ball_dy_;

                    score_ += (5 - r) * 10;
                    char buf[32];
                    snprintf(buf, sizeof(buf), "SCORE: %d", score_);
                    lv_label_set_text(score_label_, buf);
                    break;
                }
            }
        }
    }

    if (all_cleared) {
        game_over_ = true;
        lv_label_set_text(message_label_, "YOU WIN!");
    }

    lv_obj_set_pos(ball_obj_, static_cast<int>(ball_x_), static_cast<int>(ball_y_));
}
