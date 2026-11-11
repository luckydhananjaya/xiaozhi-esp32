#include "puzzle2048.h"
#include "game_manager.h"
#include <esp_log.h>
#include <esp_random.h>

#define TAG "Puzzle2048"

Puzzle2048Game* Puzzle2048Game::instance_ = nullptr;

static void PuzzleExitBtnCb(lv_event_t* e) {
    GameManager::GetInstance().ExitCurrentGame();
}

static void PuzzleDirBtnCb(lv_event_t* e) {
    if (!Puzzle2048Game::instance_) return;
    int dir = reinterpret_cast<intptr_t>(lv_event_get_user_data(e));
    Puzzle2048Game::instance_->Move(dir);
}

lv_obj_t* Puzzle2048Game::CreateScreen() {
    Destroy();
    instance_ = new Puzzle2048Game();
    instance_->screen_ = lv_obj_create(nullptr);
    lv_obj_set_size(instance_->screen_, 320, 240);
    lv_obj_set_style_bg_color(instance_->screen_, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_pad_all(instance_->screen_, 0, 0);
    lv_obj_set_scrollbar_mode(instance_->screen_, LV_SCROLLBAR_MODE_OFF);

    instance_->InitUI(instance_->screen_);
    instance_->ResetGame();
    return instance_->screen_;
}

void Puzzle2048Game::Destroy() {
    if (instance_) {
        delete instance_;
        instance_ = nullptr;
    }
}

uint32_t Puzzle2048Game::GetTileColor(int val) {
    switch (val) {
        case 2:    return 0x334155; // Slate
        case 4:    return 0x475569;
        case 8:    return 0xF59E0B; // Amber
        case 16:   return 0xF97316; // Orange
        case 32:   return 0xEA580C;
        case 64:   return 0xEF4444; // Red
        case 128:  return 0xEAB308; // Gold
        case 256:  return 0xFACC15;
        case 512:  return 0x10B981; // Emerald
        case 1024: return 0x06B6D4; // Cyan
        case 2048: return 0x8B5CF6; // Purple
        default:   return 0x1E293B; // Empty
    }
}

void Puzzle2048Game::InitUI(lv_obj_t* parent) {
    // Header (320 x 28)
    lv_obj_t* header = lv_obj_create(parent);
    lv_obj_set_size(header, 320, 28);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);

    score_label_ = lv_label_create(header);
    lv_label_set_text(score_label_, "2048 | SCORE: 0");
    lv_obj_set_style_text_color(score_label_, lv_color_hex(0xFACC15), 0);
    lv_obj_align(score_label_, LV_ALIGN_LEFT_MID, 12, 0);

    lv_obj_t* exit_btn = lv_button_create(header);
    lv_obj_set_size(exit_btn, 60, 22);
    lv_obj_align(exit_btn, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_set_style_bg_color(exit_btn, lv_color_hex(0xDC2626), 0);
    lv_obj_set_style_radius(exit_btn, 4, 0);
    lv_obj_add_event_cb(exit_btn, PuzzleExitBtnCb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* exit_lbl = lv_label_create(exit_btn);
    lv_label_set_text(exit_lbl, "EXIT");
    lv_obj_set_style_text_color(exit_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(exit_lbl);

    // 4x4 Grid Container (194 x 194) at x=8, y=36
    grid_container_ = lv_obj_create(parent);
    lv_obj_set_pos(grid_container_, 10, 36);
    lv_obj_set_size(grid_container_, 196, 196);
    lv_obj_set_style_bg_color(grid_container_, lv_color_hex(0x020617), 0);
    lv_obj_set_style_border_width(grid_container_, 2, 0);
    lv_obj_set_style_border_color(grid_container_, lv_color_hex(0x334155), 0);
    lv_obj_set_style_radius(grid_container_, 6, 0);
    lv_obj_set_style_pad_all(grid_container_, 4, 0);

    for (int r = 0; r < kSize; ++r) {
        for (int c = 0; c < kSize; ++c) {
            lv_obj_t* tile = lv_obj_create(grid_container_);
            lv_obj_set_pos(tile, 4 + c * 46, 4 + r * 46);
            lv_obj_set_size(tile, 42, 42);
            lv_obj_set_style_bg_color(tile, lv_color_hex(0x1E293B), 0);
            lv_obj_set_style_border_width(tile, 0, 0);
            lv_obj_set_style_radius(tile, 4, 0);
            lv_obj_set_style_pad_all(tile, 0, 0);

            lv_obj_t* lbl = lv_label_create(tile);
            lv_label_set_text(lbl, "");
            lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
            lv_obj_center(lbl);

            tile_objs_[r][c] = tile;
            tile_labels_[r][c] = lbl;
        }
    }

    // Controls D-Pad on right (x=215..315)
    auto create_dir_btn = [parent](const char* label, int x, int y, int dir) {
        lv_obj_t* btn = lv_button_create(parent);
        lv_obj_set_pos(btn, x, y);
        lv_obj_set_size(btn, 44, 44);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x334155), 0);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_add_event_cb(btn, PuzzleDirBtnCb, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<intptr_t>(dir)));

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, label);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(lbl);
    };

    create_dir_btn("^", 248, 52, 0);   // UP
    create_dir_btn("<", 218, 102, 2);  // LEFT
    create_dir_btn(">", 268, 102, 3);  // RIGHT
    create_dir_btn("v", 248, 152, 1);  // DOWN

    game_over_label_ = lv_label_create(grid_container_);
    lv_label_set_text(game_over_label_, "GAME OVER!\nTAP ANY DIR");
    lv_obj_set_style_text_color(game_over_label_, lv_color_hex(0xEF4444), 0);
    lv_obj_set_style_text_align(game_over_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(game_over_label_);
    lv_obj_add_flag(game_over_label_, LV_OBJ_FLAG_HIDDEN);
}

void Puzzle2048Game::ResetGame() {
    for (int r = 0; r < kSize; ++r) {
        for (int c = 0; c < kSize; ++c) {
            board_[r][c] = 0;
        }
    }
    score_ = 0;
    game_over_ = false;

    if (score_label_) lv_label_set_text(score_label_, "2048 | SCORE: 0");
    if (game_over_label_) lv_obj_add_flag(game_over_label_, LV_OBJ_FLAG_HIDDEN);

    SpawnRandomTile();
    SpawnRandomTile();
    Render();
}

void Puzzle2048Game::SpawnRandomTile() {
    int empty_r[16], empty_c[16], count = 0;
    for (int r = 0; r < kSize; ++r) {
        for (int c = 0; c < kSize; ++c) {
            if (board_[r][c] == 0) {
                empty_r[count] = r;
                empty_c[count] = c;
                count++;
            }
        }
    }
    if (count > 0) {
        int idx = esp_random() % count;
        board_[empty_r[idx]][empty_c[idx]] = (esp_random() % 10 == 0) ? 4 : 2;
    }
}

bool Puzzle2048Game::CanMove() {
    for (int r = 0; r < kSize; ++r) {
        for (int c = 0; c < kSize; ++c) {
            if (board_[r][c] == 0) return true;
            if (c < kSize - 1 && board_[r][c] == board_[r][c + 1]) return true;
            if (r < kSize - 1 && board_[r][c] == board_[r + 1][c]) return true;
        }
    }
    return false;
}

void Puzzle2048Game::Move(int dir) {
    if (game_over_) {
        ResetGame();
        return;
    }

    bool moved = false;

    auto slide_line = [&](int line[4]) {
        int target = 0;
        for (int i = 0; i < 4; ++i) {
            if (line[i] != 0) {
                if (target > 0 && line[target - 1] == line[i]) {
                    line[target - 1] *= 2;
                    score_ += line[target - 1];
                    line[i] = 0;
                    moved = true;
                } else {
                    if (target != i) {
                        line[target] = line[i];
                        line[i] = 0;
                        moved = true;
                    }
                    target++;
                }
            }
        }
    };

    if (dir == 2) { // LEFT
        for (int r = 0; r < 4; ++r) {
            int line[4] = { board_[r][0], board_[r][1], board_[r][2], board_[r][3] };
            slide_line(line);
            for (int c = 0; c < 4; ++c) board_[r][c] = line[c];
        }
    } else if (dir == 3) { // RIGHT
        for (int r = 0; r < 4; ++r) {
            int line[4] = { board_[r][3], board_[r][2], board_[r][1], board_[r][0] };
            slide_line(line);
            for (int c = 0; c < 4; ++c) board_[r][3 - c] = line[c];
        }
    } else if (dir == 0) { // UP
        for (int c = 0; c < 4; ++c) {
            int line[4] = { board_[0][c], board_[1][c], board_[2][c], board_[3][c] };
            slide_line(line);
            for (int r = 0; r < 4; ++r) board_[r][c] = line[r];
        }
    } else if (dir == 1) { // DOWN
        for (int c = 0; c < 4; ++c) {
            int line[4] = { board_[3][c], board_[2][c], board_[1][c], board_[0][c] };
            slide_line(line);
            for (int r = 0; r < 4; ++r) board_[3 - r][c] = line[r];
        }
    }

    if (moved) {
        SpawnRandomTile();
        lv_label_set_text_fmt(score_label_, "2048 | SCORE: %d", score_);
        if (!CanMove()) {
            game_over_ = true;
            lv_obj_remove_flag(game_over_label_, LV_OBJ_FLAG_HIDDEN);
        }
        Render();
    }
}

void Puzzle2048Game::Render() {
    for (int r = 0; r < kSize; ++r) {
        for (int c = 0; c < kSize; ++c) {
            int val = board_[r][c];
            lv_obj_set_style_bg_color(tile_objs_[r][c], lv_color_hex(GetTileColor(val)), 0);
            if (val > 0) {
                lv_label_set_text_fmt(tile_labels_[r][c], "%d", val);
            } else {
                lv_label_set_text(tile_labels_[r][c], "");
            }
        }
    }
}
