#include "pong.h"
#include "game_manager.h"
#include <esp_log.h>
#include <esp_random.h>
#include <cmath>

#define TAG "PongGame"

PongGame* PongGame::instance_ = nullptr;

static void PongExitBtnCb(lv_event_t* e) {
    GameManager::GetInstance().ExitCurrentGame();
}

void PongGame::TouchEventCallback(lv_event_t* e) {
    if (!instance_) return;
    lv_point_t point;
    lv_indev_get_point(lv_indev_active(), &point);
    // Touch controls left player paddle
    instance_->SetPlayerPaddleY(point.y - 20 - (kPaddleH / 2));
}

lv_obj_t* PongGame::CreateScreen() {
    Destroy();
    instance_ = new PongGame();
    instance_->screen_ = lv_obj_create(nullptr);
    lv_obj_set_size(instance_->screen_, 320, 240);
    lv_obj_set_style_bg_color(instance_->screen_, lv_color_hex(0x020617), 0);
    lv_obj_set_style_pad_all(instance_->screen_, 0, 0);
    lv_obj_set_scrollbar_mode(instance_->screen_, LV_SCROLLBAR_MODE_OFF);

    instance_->InitUI(instance_->screen_);
    instance_->Start();
    return instance_->screen_;
}

void PongGame::Destroy() {
    if (instance_) {
        instance_->Stop();
        delete instance_;
        instance_ = nullptr;
    }
}

void PongGame::InitUI(lv_obj_t* parent) {
    // Header (320 x 28)
    lv_obj_t* header = lv_obj_create(parent);
    lv_obj_set_size(header, 320, 28);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_scrollbar_mode(header, LV_SCROLLBAR_MODE_OFF);

    score_label_ = lv_label_create(header);
    lv_label_set_text(score_label_, "YOU: 0  |  CPU: 0");
    lv_obj_set_style_text_color(score_label_, lv_color_hex(0x2DD4BF), 0);
    lv_obj_align(score_label_, LV_ALIGN_LEFT_MID, 12, 0);

    lv_obj_t* exit_btn = lv_button_create(header);
    lv_obj_set_size(exit_btn, 60, 22);
    lv_obj_align(exit_btn, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_set_style_bg_color(exit_btn, lv_color_hex(0xDC2626), 0);
    lv_obj_set_style_radius(exit_btn, 4, 0);
    lv_obj_add_event_cb(exit_btn, PongExitBtnCb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* exit_lbl = lv_label_create(exit_btn);
    lv_label_set_text(exit_lbl, "EXIT");
    lv_obj_set_style_text_color(exit_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(exit_lbl);

    // Play Area (320 x 212)
    play_area_ = lv_obj_create(parent);
    lv_obj_set_pos(play_area_, 0, 28);
    lv_obj_set_size(play_area_, 320, 212);
    lv_obj_set_style_bg_color(play_area_, lv_color_hex(0x020617), 0);
    lv_obj_set_style_border_width(play_area_, 0, 0);
    lv_obj_set_style_pad_all(play_area_, 0, 0);
    lv_obj_set_scrollbar_mode(play_area_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(play_area_, TouchEventCallback, LV_EVENT_PRESSING, nullptr);
    lv_obj_add_event_cb(play_area_, TouchEventCallback, LV_EVENT_PRESSED, nullptr);

    // Center dotted line
    for (int y = 6; y < 210; y += 18) {
        lv_obj_t* dot = lv_obj_create(play_area_);
        lv_obj_set_pos(dot, 159, y);
        lv_obj_set_size(dot, 2, 10);
        lv_obj_set_style_bg_color(dot, lv_color_hex(0x334155), 0);
        lv_obj_set_style_border_width(dot, 0, 0);
    }

    // Player Paddle (left: x=14)
    player_paddle_ = lv_obj_create(play_area_);
    lv_obj_set_size(player_paddle_, kPaddleW, kPaddleH);
    lv_obj_set_pos(player_paddle_, 14, static_cast<int32_t>(player_y_));
    lv_obj_set_style_bg_color(player_paddle_, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_border_width(player_paddle_, 0, 0);
    lv_obj_set_style_radius(player_paddle_, 4, 0);

    // AI Paddle (right: x=296)
    ai_paddle_ = lv_obj_create(play_area_);
    lv_obj_set_size(ai_paddle_, kPaddleW, kPaddleH);
    lv_obj_set_pos(ai_paddle_, 296, static_cast<int32_t>(ai_y_));
    lv_obj_set_style_bg_color(ai_paddle_, lv_color_hex(0xF43F5E), 0);
    lv_obj_set_style_border_width(ai_paddle_, 0, 0);
    lv_obj_set_style_radius(ai_paddle_, 4, 0);

    // Ball
    ball_obj_ = lv_obj_create(play_area_);
    lv_obj_set_size(ball_obj_, kBallSize, kBallSize);
    lv_obj_set_pos(ball_obj_, static_cast<int32_t>(ball_x_), static_cast<int32_t>(ball_y_));
    lv_obj_set_style_bg_color(ball_obj_, lv_color_hex(0xFACC15), 0);
    lv_obj_set_style_border_width(ball_obj_, 0, 0);
    lv_obj_set_style_radius(ball_obj_, 4, 0);

    // Message
    message_label_ = lv_label_create(play_area_);
    lv_label_set_text(message_label_, "");
    lv_obj_set_style_text_color(message_label_, lv_color_hex(0x22C55E), 0);
    lv_obj_set_style_text_align(message_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(message_label_);
    lv_obj_add_flag(message_label_, LV_OBJ_FLAG_HIDDEN);
}

void PongGame::Start() {
    ResetGame();
    timer_ = lv_timer_create(TimerCallback, 25, this); // 40 FPS
}

void PongGame::Stop() {
    if (timer_) {
        lv_timer_delete(timer_);
        timer_ = nullptr;
    }
}

void PongGame::SetPlayerPaddleY(int y) {
    if (game_over_) {
        ResetGame();
        return;
    }
    if (y < 4) y = 4;
    if (y > 212 - kPaddleH - 4) y = 212 - kPaddleH - 4;
    player_y_ = static_cast<float>(y);
    lv_obj_set_pos(player_paddle_, 14, static_cast<int32_t>(player_y_));
}

void PongGame::ResetBall() {
    ball_x_ = 160.0f;
    ball_y_ = 100.0f;
    ball_dx_ = (esp_random() % 2 == 0) ? 3.0f : -3.0f;
    ball_dy_ = ((esp_random() % 200) - 100) / 50.0f;
}

void PongGame::ResetGame() {
    player_score_ = 0;
    ai_score_ = 0;
    game_over_ = false;
    player_y_ = 80.0f;
    ai_y_ = 80.0f;

    if (score_label_) lv_label_set_text(score_label_, "YOU: 0  |  CPU: 0");
    if (message_label_) lv_obj_add_flag(message_label_, LV_OBJ_FLAG_HIDDEN);

    ResetBall();
    Render();
}

void PongGame::UpdatePhysics() {
    if (game_over_) return;

    ball_x_ += ball_dx_;
    ball_y_ += ball_dy_;

    // Top / bottom bounce
    if (ball_y_ <= 2.0f) {
        ball_y_ = 2.0f;
        ball_dy_ = -ball_dy_;
    } else if (ball_y_ >= 210.0f - kBallSize) {
        ball_y_ = 210.0f - kBallSize;
        ball_dy_ = -ball_dy_;
    }

    // AI paddle tracking (smooth with error)
    float ai_center = ai_y_ + (kPaddleH / 2.0f);
    if (ball_x_ > 100.0f) {
        if (ai_center < ball_y_ - 4.0f) ai_y_ += 2.4f;
        else if (ai_center > ball_y_ + 4.0f) ai_y_ -= 2.4f;
    }
    if (ai_y_ < 4.0f) ai_y_ = 4.0f;
    if (ai_y_ > 212.0f - kPaddleH - 4.0f) ai_y_ = 212.0f - kPaddleH - 4.0f;

    // Player Paddle collision (x=14..24)
    if (ball_x_ <= 24.0f && ball_x_ >= 10.0f) {
        if (ball_y_ + kBallSize >= player_y_ && ball_y_ <= player_y_ + kPaddleH) {
            ball_x_ = 25.0f;
            ball_dx_ = -ball_dx_ * 1.05f; // Slight speedup
            float hit_offset = (ball_y_ + (kBallSize / 2.0f)) - (player_y_ + (kPaddleH / 2.0f));
            ball_dy_ = hit_offset * 0.15f;
        }
    }

    // AI Paddle collision (x=296..306)
    if (ball_x_ + kBallSize >= 296.0f && ball_x_ <= 310.0f) {
        if (ball_y_ + kBallSize >= ai_y_ && ball_y_ <= ai_y_ + kPaddleH) {
            ball_x_ = 295.0f - kBallSize;
            ball_dx_ = -ball_dx_;
            float hit_offset = (ball_y_ + (kBallSize / 2.0f)) - (ai_y_ + (kPaddleH / 2.0f));
            ball_dy_ = hit_offset * 0.15f;
        }
    }

    // Player scores
    if (ball_x_ > 320.0f) {
        player_score_++;
        lv_label_set_text_fmt(score_label_, "YOU: %d  |  CPU: %d", player_score_, ai_score_);
        if (player_score_ >= 5) {
            game_over_ = true;
            lv_label_set_text(message_label_, "YOU WON!\nTAP TO PLAY AGAIN");
            lv_obj_remove_flag(message_label_, LV_OBJ_FLAG_HIDDEN);
        } else {
            ResetBall();
        }
    }

    // AI scores
    if (ball_x_ < -kBallSize) {
        ai_score_++;
        lv_label_set_text_fmt(score_label_, "YOU: %d  |  CPU: %d", player_score_, ai_score_);
        if (ai_score_ >= 5) {
            game_over_ = true;
            lv_label_set_text(message_label_, "CPU WON!\nTAP TO PLAY AGAIN");
            lv_obj_remove_flag(message_label_, LV_OBJ_FLAG_HIDDEN);
        } else {
            ResetBall();
        }
    }

    Render();
}

void PongGame::Render() {
    lv_obj_set_pos(player_paddle_, 14, static_cast<int32_t>(player_y_));
    lv_obj_set_pos(ai_paddle_, 296, static_cast<int32_t>(ai_y_));
    lv_obj_set_pos(ball_obj_, static_cast<int32_t>(ball_x_), static_cast<int32_t>(ball_y_));
}

void PongGame::TimerCallback(lv_timer_t* t) {
    auto self = static_cast<PongGame*>(lv_timer_get_user_data(t));
    if (self) self->UpdatePhysics();
}
