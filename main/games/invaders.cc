#include "invaders.h"
#include "game_manager.h"
#include <esp_log.h>
#include <esp_random.h>

#define TAG "InvadersGame"

InvadersGame* InvadersGame::instance_ = nullptr;

static void InvadersExitBtnCb(lv_event_t* e) {
    GameManager::GetInstance().ExitCurrentGame();
}

static void InvadersBtnCb(lv_event_t* e) {
    if (!InvadersGame::instance_) return;
    int action = reinterpret_cast<intptr_t>(lv_event_get_user_data(e));
    if (action == 0) InvadersGame::instance_->MoveLeft();
    else if (action == 1) InvadersGame::instance_->MoveRight();
    else if (action == 2) InvadersGame::instance_->Fire();
}

lv_obj_t* InvadersGame::CreateScreen() {
    Destroy();
    instance_ = new InvadersGame();
    instance_->screen_ = lv_obj_create(nullptr);
    lv_obj_set_size(instance_->screen_, 320, 240);
    lv_obj_set_style_bg_color(instance_->screen_, lv_color_hex(0x020617), 0);
    lv_obj_set_style_pad_all(instance_->screen_, 0, 0);
    lv_obj_set_scrollbar_mode(instance_->screen_, LV_SCROLLBAR_MODE_OFF);

    instance_->InitUI(instance_->screen_);
    instance_->Start();
    return instance_->screen_;
}

void InvadersGame::Destroy() {
    if (instance_) {
        instance_->Stop();
        delete instance_;
        instance_ = nullptr;
    }
}

void InvadersGame::InitUI(lv_obj_t* parent) {
    // Header Bar (320 x 26)
    lv_obj_t* header = lv_obj_create(parent);
    lv_obj_set_size(header, 320, 26);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_scrollbar_mode(header, LV_SCROLLBAR_MODE_OFF);

    score_label_ = lv_label_create(header);
    lv_label_set_text(score_label_, "INVADERS | SCORE: 0");
    lv_obj_set_style_text_color(score_label_, lv_color_hex(0xA855F7), 0);
    lv_obj_align(score_label_, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t* exit_btn = lv_button_create(header);
    lv_obj_set_size(exit_btn, 55, 20);
    lv_obj_align(exit_btn, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_set_style_bg_color(exit_btn, lv_color_hex(0xDC2626), 0);
    lv_obj_set_style_radius(exit_btn, 4, 0);
    lv_obj_add_event_cb(exit_btn, InvadersExitBtnCb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* exit_lbl = lv_label_create(exit_btn);
    lv_label_set_text(exit_lbl, "EXIT");
    lv_obj_set_style_text_color(exit_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(exit_lbl);

    // Play Area (320 x 170 px) at y=26
    play_area_ = lv_obj_create(parent);
    lv_obj_set_pos(play_area_, 0, 26);
    lv_obj_set_size(play_area_, 320, 168);
    lv_obj_set_style_bg_color(play_area_, lv_color_hex(0x020617), 0);
    lv_obj_set_style_border_width(play_area_, 0, 0);
    lv_obj_set_style_pad_all(play_area_, 0, 0);
    lv_obj_set_scrollbar_mode(play_area_, LV_SCROLLBAR_MODE_OFF);

    // Aliens (3 rows x 6 cols)
    static const uint32_t kRowColors[kRows] = { 0xEC4899, 0xA855F7, 0x06B6D4 };
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            lv_obj_t* alien = lv_obj_create(play_area_);
            lv_obj_set_size(alien, kAlienW, kAlienH);
            lv_obj_set_style_bg_color(alien, lv_color_hex(kRowColors[r]), 0);
            lv_obj_set_style_border_width(alien, 0, 0);
            lv_obj_set_style_radius(alien, 3, 0);
            alien_objs_[r][c] = alien;
        }
    }

    // Player Bullets
    for (int i = 0; i < kMaxPlayerBullets; ++i) {
        player_bullet_objs_[i] = lv_obj_create(play_area_);
        lv_obj_set_size(player_bullet_objs_[i], 3, 8);
        lv_obj_set_style_bg_color(player_bullet_objs_[i], lv_color_hex(0x38BDF8), 0);
        lv_obj_set_style_border_width(player_bullet_objs_[i], 0, 0);
        lv_obj_add_flag(player_bullet_objs_[i], LV_OBJ_FLAG_HIDDEN);
    }

    // Alien Bullets
    for (int i = 0; i < kMaxAlienBullets; ++i) {
        alien_bullet_objs_[i] = lv_obj_create(play_area_);
        lv_obj_set_size(alien_bullet_objs_[i], 3, 8);
        lv_obj_set_style_bg_color(alien_bullet_objs_[i], lv_color_hex(0xF43F5E), 0);
        lv_obj_set_style_border_width(alien_bullet_objs_[i], 0, 0);
        lv_obj_add_flag(alien_bullet_objs_[i], LV_OBJ_FLAG_HIDDEN);
    }

    // Player Tank (28 x 12 px)
    player_obj_ = lv_obj_create(play_area_);
    lv_obj_set_size(player_obj_, 28, 12);
    lv_obj_set_pos(player_obj_, static_cast<int32_t>(player_x_), 152);
    lv_obj_set_style_bg_color(player_obj_, lv_color_hex(0x22C55E), 0);
    lv_obj_set_style_border_width(player_obj_, 0, 0);
    lv_obj_set_style_radius(player_obj_, 3, 0);

    // Bottom Controls (y=196..240, h=44)
    lv_obj_t* bottom_bar = lv_obj_create(parent);
    lv_obj_set_pos(bottom_bar, 0, 196);
    lv_obj_set_size(bottom_bar, 320, 44);
    lv_obj_set_style_bg_color(bottom_bar, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_width(bottom_bar, 0, 0);
    lv_obj_set_style_pad_all(bottom_bar, 0, 0);

    auto create_btn = [bottom_bar](const char* label, int x, int w, int action, uint32_t bg_hex) {
        lv_obj_t* btn = lv_button_create(bottom_bar);
        lv_obj_set_pos(btn, x, 4);
        lv_obj_set_size(btn, w, 36);
        lv_obj_set_style_bg_color(btn, lv_color_hex(bg_hex), 0);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_add_event_cb(btn, InvadersBtnCb, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<intptr_t>(action)));

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, label);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(lbl);
    };

    create_btn("< LEFT", 12, 80, 0, 0x334155);
    create_btn("RIGHT >", 102, 80, 1, 0x334155);
    create_btn("FIRE !", 200, 108, 2, 0xDC2626);

    // Game Over Label
    game_over_label_ = lv_label_create(play_area_);
    lv_label_set_text(game_over_label_, "GAME OVER!\nTAP FIRE TO RETRY");
    lv_obj_set_style_text_color(game_over_label_, lv_color_hex(0xEF4444), 0);
    lv_obj_set_style_text_align(game_over_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(game_over_label_);
    lv_obj_add_flag(game_over_label_, LV_OBJ_FLAG_HIDDEN);
}

void InvadersGame::Start() {
    ResetGame();
    timer_ = lv_timer_create(TimerCallback, 30, this);
}

void InvadersGame::Stop() {
    if (timer_) {
        lv_timer_delete(timer_);
        timer_ = nullptr;
    }
}

void InvadersGame::ResetGame() {
    player_x_ = 146.0f;
    fleet_x_ = 20.0f;
    fleet_y_ = 15.0f;
    fleet_dx_ = 1.2f;
    score_ = 0;
    game_over_ = false;

    if (score_label_) lv_label_set_text(score_label_, "INVADERS | SCORE: 0");
    if (game_over_label_) lv_obj_add_flag(game_over_label_, LV_OBJ_FLAG_HIDDEN);

    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            alien_alive_[r][c] = true;
            if (alien_objs_[r][c]) lv_obj_remove_flag(alien_objs_[r][c], LV_OBJ_FLAG_HIDDEN);
        }
    }

    for (int i = 0; i < kMaxPlayerBullets; ++i) {
        player_bullets_[i].active = false;
        if (player_bullet_objs_[i]) lv_obj_add_flag(player_bullet_objs_[i], LV_OBJ_FLAG_HIDDEN);
    }

    for (int i = 0; i < kMaxAlienBullets; ++i) {
        alien_bullets_[i].active = false;
        if (alien_bullet_objs_[i]) lv_obj_add_flag(alien_bullet_objs_[i], LV_OBJ_FLAG_HIDDEN);
    }

    Render();
}

void InvadersGame::MoveLeft() {
    if (game_over_) return;
    player_x_ -= 12.0f;
    if (player_x_ < 4.0f) player_x_ = 4.0f;
    lv_obj_set_pos(player_obj_, static_cast<int32_t>(player_x_), 152);
}

void InvadersGame::MoveRight() {
    if (game_over_) return;
    player_x_ += 12.0f;
    if (player_x_ > 288.0f) player_x_ = 288.0f;
    lv_obj_set_pos(player_obj_, static_cast<int32_t>(player_x_), 152);
}

void InvadersGame::Fire() {
    if (game_over_) {
        ResetGame();
        return;
    }

    for (int i = 0; i < kMaxPlayerBullets; ++i) {
        if (!player_bullets_[i].active) {
            player_bullets_[i].active = true;
            player_bullets_[i].x = player_x_ + 13.0f;
            player_bullets_[i].y = 146.0f;
            lv_obj_remove_flag(player_bullet_objs_[i], LV_OBJ_FLAG_HIDDEN);
            break;
        }
    }
}

void InvadersGame::UpdateGame() {
    if (game_over_) return;

    // Move fleet
    fleet_x_ += fleet_dx_;
    const float kFleetW = kCols * 36.0f;
    if (fleet_x_ + kFleetW > 316.0f || fleet_x_ < 4.0f) {
        fleet_dx_ = -fleet_dx_;
        fleet_y_ += 6.0f; // step down
    }

    // Alien reaches player level
    if (fleet_y_ + (kRows * 22.0f) >= 148.0f) {
        game_over_ = true;
        lv_obj_remove_flag(game_over_label_, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    // Move player bullets
    for (int i = 0; i < kMaxPlayerBullets; ++i) {
        if (player_bullets_[i].active) {
            player_bullets_[i].y -= 4.0f;
            if (player_bullets_[i].y < 0) {
                player_bullets_[i].active = false;
                lv_obj_add_flag(player_bullet_objs_[i], LV_OBJ_FLAG_HIDDEN);
            } else {
                // Check collision with aliens
                for (int r = 0; r < kRows; ++r) {
                    for (int c = 0; c < kCols; ++c) {
                        if (alien_alive_[r][c]) {
                            float ax = fleet_x_ + c * 36.0f;
                            float ay = fleet_y_ + r * 22.0f;
                            if (player_bullets_[i].x >= ax && player_bullets_[i].x <= ax + kAlienW &&
                                player_bullets_[i].y >= ay && player_bullets_[i].y <= ay + kAlienH) {
                                alien_alive_[r][c] = false;
                                player_bullets_[i].active = false;
                                lv_obj_add_flag(player_bullet_objs_[i], LV_OBJ_FLAG_HIDDEN);
                                lv_obj_add_flag(alien_objs_[r][c], LV_OBJ_FLAG_HIDDEN);
                                score_ += 20;
                                lv_label_set_text_fmt(score_label_, "INVADERS | SCORE: %d", score_);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    // Random alien shooting
    if ((esp_random() % 40) == 0) {
        for (int i = 0; i < kMaxAlienBullets; ++i) {
            if (!alien_bullets_[i].active) {
                int col = esp_random() % kCols;
                for (int r = kRows - 1; r >= 0; --r) {
                    if (alien_alive_[r][col]) {
                        alien_bullets_[i].active = true;
                        alien_bullets_[i].x = fleet_x_ + col * 36.0f + (kAlienW / 2.0f);
                        alien_bullets_[i].y = fleet_y_ + r * 22.0f + kAlienH;
                        lv_obj_remove_flag(alien_bullet_objs_[i], LV_OBJ_FLAG_HIDDEN);
                        break;
                    }
                }
                break;
            }
        }
    }

    // Move alien bullets
    for (int i = 0; i < kMaxAlienBullets; ++i) {
        if (alien_bullets_[i].active) {
            alien_bullets_[i].y += 3.0f;
            if (alien_bullets_[i].y > 165.0f) {
                alien_bullets_[i].active = false;
                lv_obj_add_flag(alien_bullet_objs_[i], LV_OBJ_FLAG_HIDDEN);
            } else if (alien_bullets_[i].y >= 150.0f && alien_bullets_[i].y <= 164.0f &&
                       alien_bullets_[i].x >= player_x_ && alien_bullets_[i].x <= player_x_ + 28.0f) {
                // Hit player!
                game_over_ = true;
                lv_obj_remove_flag(game_over_label_, LV_OBJ_FLAG_HIDDEN);
                return;
            }
        }
    }

    Render();
}

void InvadersGame::Render() {
    // Render Aliens
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            if (alien_alive_[r][c]) {
                int ax = static_cast<int>(fleet_x_ + c * 36.0f);
                int ay = static_cast<int>(fleet_y_ + r * 22.0f);
                lv_obj_set_pos(alien_objs_[r][c], ax, ay);
            }
        }
    }

    // Render Player Bullets
    for (int i = 0; i < kMaxPlayerBullets; ++i) {
        if (player_bullets_[i].active) {
            lv_obj_set_pos(player_bullet_objs_[i], static_cast<int32_t>(player_bullets_[i].x), static_cast<int32_t>(player_bullets_[i].y));
        }
    }

    // Render Alien Bullets
    for (int i = 0; i < kMaxAlienBullets; ++i) {
        if (alien_bullets_[i].active) {
            lv_obj_set_pos(alien_bullet_objs_[i], static_cast<int32_t>(alien_bullets_[i].x), static_cast<int32_t>(alien_bullets_[i].y));
        }
    }
}

void InvadersGame::TimerCallback(lv_timer_t* t) {
    auto self = static_cast<InvadersGame*>(lv_timer_get_user_data(t));
    if (self) self->UpdateGame();
}
