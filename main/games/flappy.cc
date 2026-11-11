#include "flappy.h"
#include "game_manager.h"
#include <esp_log.h>
#include <esp_random.h>

#define TAG "FlappyGame"

FlappyGame* FlappyGame::instance_ = nullptr;

static void FlappyExitBtnCb(lv_event_t* e) {
    GameManager::GetInstance().ExitCurrentGame();
}

void FlappyGame::TouchEventCallback(lv_event_t* e) {
    if (instance_) {
        instance_->Flap();
    }
}

lv_obj_t* FlappyGame::CreateScreen() {
    Destroy();
    instance_ = new FlappyGame();
    instance_->screen_ = lv_obj_create(nullptr);
    lv_obj_set_size(instance_->screen_, 320, 240);
    lv_obj_set_style_bg_color(instance_->screen_, lv_color_hex(0x0284C7), 0); // Sky blue
    lv_obj_set_style_pad_all(instance_->screen_, 0, 0);
    lv_obj_set_scrollbar_mode(instance_->screen_, LV_SCROLLBAR_MODE_OFF);

    instance_->InitUI(instance_->screen_);
    instance_->Start();
    return instance_->screen_;
}

void FlappyGame::Destroy() {
    if (instance_) {
        instance_->Stop();
        delete instance_;
        instance_ = nullptr;
    }
}

void FlappyGame::InitUI(lv_obj_t* parent) {
    // Play area (touchable surface across 320 x 205)
    play_area_ = lv_obj_create(parent);
    lv_obj_set_pos(play_area_, 0, 0);
    lv_obj_set_size(play_area_, 320, 210);
    lv_obj_set_style_bg_color(play_area_, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_border_width(play_area_, 0, 0);
    lv_obj_set_style_pad_all(play_area_, 0, 0);
    lv_obj_set_scrollbar_mode(play_area_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(play_area_, TouchEventCallback, LV_EVENT_PRESSED, nullptr);

    // Pipes objects
    for (int i = 0; i < kMaxPipes; ++i) {
        pipe_top_objs_[i] = lv_obj_create(play_area_);
        lv_obj_set_style_bg_color(pipe_top_objs_[i], lv_color_hex(0x22C55E), 0);
        lv_obj_set_style_border_width(pipe_top_objs_[i], 2, 0);
        lv_obj_set_style_border_color(pipe_top_objs_[i], lv_color_hex(0x15803D), 0);
        lv_obj_set_style_radius(pipe_top_objs_[i], 4, 0);

        pipe_bot_objs_[i] = lv_obj_create(play_area_);
        lv_obj_set_style_bg_color(pipe_bot_objs_[i], lv_color_hex(0x22C55E), 0);
        lv_obj_set_style_border_width(pipe_bot_objs_[i], 2, 0);
        lv_obj_set_style_border_color(pipe_bot_objs_[i], lv_color_hex(0x15803D), 0);
        lv_obj_set_style_radius(pipe_bot_objs_[i], 4, 0);
    }

    // Bird (18 x 16 yellow rounded box)
    bird_obj_ = lv_obj_create(play_area_);
    lv_obj_set_size(bird_obj_, 20, 16);
    lv_obj_set_pos(bird_obj_, 60, 100);
    lv_obj_set_style_bg_color(bird_obj_, lv_color_hex(0xFACC15), 0);
    lv_obj_set_style_border_width(bird_obj_, 2, 0);
    lv_obj_set_style_border_color(bird_obj_, lv_color_hex(0xCA8A04), 0);
    lv_obj_set_style_radius(bird_obj_, 6, 0);

    // Ground bar (320 x 30)
    lv_obj_t* ground = lv_obj_create(parent);
    lv_obj_set_pos(ground, 0, 210);
    lv_obj_set_size(ground, 320, 30);
    lv_obj_set_style_bg_color(ground, lv_color_hex(0x84CC16), 0);
    lv_obj_set_style_border_width(ground, 0, 0);
    lv_obj_set_style_pad_all(ground, 0, 0);

    // Score label top-left
    score_label_ = lv_label_create(parent);
    lv_label_set_text(score_label_, "SCORE: 0");
    lv_obj_set_style_text_color(score_label_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(score_label_, LV_ALIGN_TOP_LEFT, 10, 8);

    // EXIT Button top-right
    lv_obj_t* exit_btn = lv_button_create(parent);
    lv_obj_set_size(exit_btn, 60, 24);
    lv_obj_align(exit_btn, LV_ALIGN_TOP_RIGHT, -8, 6);
    lv_obj_set_style_bg_color(exit_btn, lv_color_hex(0xDC2626), 0);
    lv_obj_set_style_radius(exit_btn, 4, 0);
    lv_obj_add_event_cb(exit_btn, FlappyExitBtnCb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* exit_lbl = lv_label_create(exit_btn);
    lv_label_set_text(exit_lbl, "EXIT");
    lv_obj_set_style_text_color(exit_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(exit_lbl);

    // Game Over Message
    game_over_label_ = lv_label_create(play_area_);
    lv_label_set_text(game_over_label_, "GAME OVER!\nTAP TO RETRY");
    lv_obj_set_style_text_color(game_over_label_, lv_color_hex(0xEF4444), 0);
    lv_obj_set_style_text_align(game_over_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(game_over_label_);
    lv_obj_add_flag(game_over_label_, LV_OBJ_FLAG_HIDDEN);
}

void FlappyGame::Start() {
    ResetGame();
    timer_ = lv_timer_create(TimerCallback, 30, this); // ~33 FPS
}

void FlappyGame::Stop() {
    if (timer_) {
        lv_timer_delete(timer_);
        timer_ = nullptr;
    }
}

void FlappyGame::ResetGame() {
    bird_y_ = 100.0f;
    bird_vy_ = 0.0f;
    score_ = 0;
    game_over_ = false;

    if (score_label_) lv_label_set_text(score_label_, "SCORE: 0");
    if (game_over_label_) lv_obj_add_flag(game_over_label_, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < kMaxPipes; ++i) {
        pipes_[i].x = 340.0f + i * 125.0f;
        pipes_[i].gap_y = 50 + (esp_random() % 90); // gap center between 50 and 140
        pipes_[i].gap_height = 68; // generous gap for thumb play
        pipes_[i].scored = false;
    }

    Render();
}

void FlappyGame::Flap() {
    if (game_over_) {
        ResetGame();
        return;
    }
    bird_vy_ = -5.5f; // Jump upward
}

void FlappyGame::UpdatePhysics() {
    if (game_over_) return;

    // Gravity
    bird_vy_ += 0.38f;
    bird_y_ += bird_vy_;

    // Ground / Ceiling collision
    if (bird_y_ >= 194.0f) {
        bird_y_ = 194.0f;
        game_over_ = true;
        lv_obj_remove_flag(game_over_label_, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (bird_y_ <= 0.0f) {
        bird_y_ = 0.0f;
        bird_vy_ = 0.0f;
    }

    const float kPipeWidth = 32.0f;
    const float kBirdX = 60.0f;
    const float kBirdW = 20.0f;
    const float kBirdH = 16.0f;

    // Move pipes
    for (int i = 0; i < kMaxPipes; ++i) {
        pipes_[i].x -= 2.2f;

        // Pipe recycled when scrolled off screen
        if (pipes_[i].x < -kPipeWidth) {
            pipes_[i].x += kMaxPipes * 125.0f;
            pipes_[i].gap_y = 50 + (esp_random() % 90);
            pipes_[i].scored = false;
        }

        // Score update
        if (!pipes_[i].scored && (pipes_[i].x + kPipeWidth) < kBirdX) {
            pipes_[i].scored = true;
            score_++;
            lv_label_set_text_fmt(score_label_, "SCORE: %d", score_);
        }

        // Collision check
        if (kBirdX + kBirdW > pipes_[i].x && kBirdX < pipes_[i].x + kPipeWidth) {
            float top_pipe_bottom = pipes_[i].gap_y - (pipes_[i].gap_height / 2.0f);
            float bot_pipe_top = pipes_[i].gap_y + (pipes_[i].gap_height / 2.0f);

            if (bird_y_ < top_pipe_bottom || (bird_y_ + kBirdH) > bot_pipe_top) {
                game_over_ = true;
                lv_obj_remove_flag(game_over_label_, LV_OBJ_FLAG_HIDDEN);
                return;
            }
        }
    }

    Render();
}

void FlappyGame::Render() {
    if (bird_obj_) {
        lv_obj_set_pos(bird_obj_, 60, static_cast<int32_t>(bird_y_));
    }

    const int kPipeWidth = 32;
    for (int i = 0; i < kMaxPipes; ++i) {
        int x = static_cast<int>(pipes_[i].x);
        int top_h = pipes_[i].gap_y - (pipes_[i].gap_height / 2);
        int bot_y = pipes_[i].gap_y + (pipes_[i].gap_height / 2);
        int bot_h = 210 - bot_y;

        if (top_h < 4) top_h = 4;
        if (bot_h < 4) bot_h = 4;

        lv_obj_set_pos(pipe_top_objs_[i], x, 0);
        lv_obj_set_size(pipe_top_objs_[i], kPipeWidth, top_h);

        lv_obj_set_pos(pipe_bot_objs_[i], x, bot_y);
        lv_obj_set_size(pipe_bot_objs_[i], kPipeWidth, bot_h);
    }
}

void FlappyGame::TimerCallback(lv_timer_t* t) {
    auto self = static_cast<FlappyGame*>(lv_timer_get_user_data(t));
    if (self) self->UpdatePhysics();
}
