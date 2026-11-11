#include "tetris.h"
#include "game_manager.h"
#include <esp_random.h>
#include <esp_log.h>
#include <stdio.h>

#define TAG "Tetris"

TetrisGame* TetrisGame::instance_ = nullptr;

static const uint16_t kTetrominoColors[8] = {
    0x0F17, // 0: Empty (Dark)
    0x05B5, // 1: I (Cyan)
    0xEF60, // 2: O (Yellow)
    0xA2B5, // 3: T (Purple)
    0x260C, // 4: S (Green)
    0xEC88, // 5: Z (Red)
    0x3C1D, // 6: J (Blue)
    0xFBA2, // 7: L (Orange)
};

static const uint32_t kTetrominoHexColors[8] = {
    0x0F172A, // 0: Empty
    0x06B6D4, // 1: I
    0xEAB308, // 2: O
    0xA855F7, // 3: T
    0x22C55E, // 4: S
    0xEF4444, // 5: Z
    0x3B82F6, // 6: J
    0xF97316  // 7: L
};

static const uint16_t kPieces[7][4] = {
    // I
    {0x0F00, 0x2222, 0x00F0, 0x4444},
    // O
    {0x6600, 0x6600, 0x6600, 0x6600},
    // T
    {0x2700, 0x2320, 0x0720, 0x2620},
    // S
    {0x3600, 0x2310, 0x0360, 0x4620},
    // Z
    {0x6300, 0x1320, 0x0630, 0x2640},
    // J
    {0x4700, 0x2230, 0x0710, 0x3110},
    // L
    {0x1700, 0x3220, 0x0740, 0x2260}
};

static void OnExitClicked(lv_event_t* e) {
    if (TetrisGame::instance_) {
        TetrisGame::instance_->Stop();
    }
    GameManager::GetInstance().ExitCurrentGame();
}

static void OnLeftClicked(lv_event_t* e) {
    if (TetrisGame::instance_) TetrisGame::instance_->MoveLeft();
}

static void OnRightClicked(lv_event_t* e) {
    if (TetrisGame::instance_) TetrisGame::instance_->MoveRight();
}

static void OnDownClicked(lv_event_t* e) {
    if (TetrisGame::instance_) TetrisGame::instance_->MoveDown();
}

static void OnRotateClicked(lv_event_t* e) {
    if (TetrisGame::instance_) TetrisGame::instance_->Rotate();
}

static void OnDropClicked(lv_event_t* e) {
    if (TetrisGame::instance_) TetrisGame::instance_->Drop();
}

lv_obj_t* TetrisGame::CreateScreen() {
    Destroy();
    instance_ = new TetrisGame();

    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_set_size(scr, 240, 320);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x050814), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

    instance_->screen_ = scr;
    instance_->InitUI(scr);
    instance_->Start();
    return scr;
}

void TetrisGame::Destroy() {
    if (instance_) {
        instance_->Stop();
        delete instance_;
        instance_ = nullptr;
    }
}

void TetrisGame::InitUI(lv_obj_t* scr) {
    // Top Bar Header
    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, "TETRIS");
    lv_obj_set_style_text_color(title, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_pos(title, 12, 6);

    // EXIT Button top-right
    lv_obj_t* exit_btn = lv_button_create(scr);
    lv_obj_set_pos(exit_btn, 172, 4);
    lv_obj_set_size(exit_btn, 60, 24);
    lv_obj_set_style_bg_color(exit_btn, lv_color_hex(0xDC2626), 0); // Red
    lv_obj_set_style_radius(exit_btn, 4, 0);
    lv_obj_add_event_cb(exit_btn, OnExitClicked, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* exit_lbl = lv_label_create(exit_btn);
    lv_label_set_text(exit_lbl, "EXIT");
    lv_obj_set_style_text_color(exit_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(exit_lbl);

    // Left Board Container (100 x 200 px)
    board_container_ = lv_obj_create(scr);
    lv_obj_set_pos(board_container_, 8, 34);
    lv_obj_set_size(board_container_, 104, 204);
    lv_obj_set_style_bg_color(board_container_, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_border_width(board_container_, 2, 0);
    lv_obj_set_style_border_color(board_container_, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_pad_all(board_container_, 0, 0);
    lv_obj_set_scrollbar_mode(board_container_, LV_SCROLLBAR_MODE_OFF);

    // Create block UI elements
    for (int y = 0; y < kBoardHeight; ++y) {
        for (int x = 0; x < kBoardWidth; ++x) {
            lv_obj_t* blk = lv_obj_create(board_container_);
            lv_obj_set_pos(blk, x * kBlockSize, y * kBlockSize);
            lv_obj_set_size(blk, kBlockSize - 1, kBlockSize - 1);
            lv_obj_set_style_radius(blk, 1, 0);
            lv_obj_set_style_border_width(blk, 0, 0);
            lv_obj_set_style_pad_all(blk, 0, 0);
            lv_obj_set_style_bg_color(blk, lv_color_hex(kTetrominoHexColors[0]), 0);
            blocks_[y][x] = blk;
        }
    }

    // Right Stats Panel
    score_label_ = lv_label_create(scr);
    lv_label_set_text(score_label_, "SCORE\n0");
    lv_obj_set_style_text_color(score_label_, lv_color_hex(0xF8FAFC), 0);
    lv_obj_set_pos(score_label_, 124, 44);

    lines_label_ = lv_label_create(scr);
    lv_label_set_text(lines_label_, "LINES\n0");
    lv_obj_set_style_text_color(lines_label_, lv_color_hex(0xF8FAFC), 0);
    lv_obj_set_pos(lines_label_, 124, 94);

    game_over_label_ = lv_label_create(scr);
    lv_label_set_text(game_over_label_, "");
    lv_obj_set_style_text_color(game_over_label_, lv_color_hex(0xEF4444), 0);
    lv_obj_set_pos(game_over_label_, 124, 150);

    // Bottom Thumb Controls (y=246..314)
    // 1. LEFT [<]
    lv_obj_t* left_btn = lv_button_create(scr);
    lv_obj_set_pos(left_btn, 6, 248);
    lv_obj_set_size(left_btn, 52, 62);
    lv_obj_set_style_bg_color(left_btn, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(left_btn, 2, 0);
    lv_obj_set_style_border_color(left_btn, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_radius(left_btn, 8, 0);
    lv_obj_add_event_cb(left_btn, OnLeftClicked, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* l_lbl = lv_label_create(left_btn);
    lv_label_set_text(l_lbl, "<");
    lv_obj_set_style_text_color(l_lbl, lv_color_hex(0x38BDF8), 0);
    lv_obj_center(l_lbl);

    // 2. RIGHT [>]
    lv_obj_t* right_btn = lv_button_create(scr);
    lv_obj_set_pos(right_btn, 64, 248);
    lv_obj_set_size(right_btn, 52, 62);
    lv_obj_set_style_bg_color(right_btn, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(right_btn, 2, 0);
    lv_obj_set_style_border_color(right_btn, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_radius(right_btn, 8, 0);
    lv_obj_add_event_cb(right_btn, OnRightClicked, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* r_lbl = lv_label_create(right_btn);
    lv_label_set_text(r_lbl, ">");
    lv_obj_set_style_text_color(r_lbl, lv_color_hex(0x38BDF8), 0);
    lv_obj_center(r_lbl);

    // 3. ROTATE [ROT]
    lv_obj_t* rot_btn = lv_button_create(scr);
    lv_obj_set_pos(rot_btn, 122, 248);
    lv_obj_set_size(rot_btn, 56, 62);
    lv_obj_set_style_bg_color(rot_btn, lv_color_hex(0x8B5CF6), 0); // Purple
    lv_obj_set_style_radius(rot_btn, 8, 0);
    lv_obj_add_event_cb(rot_btn, OnRotateClicked, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* rot_lbl = lv_label_create(rot_btn);
    lv_label_set_text(rot_lbl, "ROT");
    lv_obj_set_style_text_color(rot_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(rot_lbl);

    // 4. DROP [DROP]
    lv_obj_t* drop_btn = lv_button_create(scr);
    lv_obj_set_pos(drop_btn, 182, 248);
    lv_obj_set_size(drop_btn, 52, 62);
    lv_obj_set_style_bg_color(drop_btn, lv_color_hex(0x0284C7), 0); // Cyan
    lv_obj_set_style_radius(drop_btn, 8, 0);
    lv_obj_add_event_cb(drop_btn, OnDropClicked, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* drp_lbl = lv_label_create(drop_btn);
    lv_label_set_text(drp_lbl, "DROP");
    lv_obj_set_style_text_color(drp_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(drp_lbl);
}

void TetrisGame::Reset() {
    for (int y = 0; y < kBoardHeight; ++y) {
        for (int x = 0; x < kBoardWidth; ++x) {
            board_[y][x] = 0;
        }
    }
    score_ = 0;
    lines_ = 0;
    is_game_over_ = false;
    lv_label_set_text(game_over_label_, "");
    SpawnPiece();
    UpdateDisplay();
}

void TetrisGame::Start() {
    Reset();
    if (!timer_) {
        timer_ = lv_timer_create(TimerCallback, 500, this);
    }
}

void TetrisGame::Stop() {
    if (timer_) {
        lv_timer_delete(timer_);
        timer_ = nullptr;
    }
}

void TetrisGame::TimerCallback(lv_timer_t* t) {
    auto* game = static_cast<TetrisGame*>(lv_timer_get_user_data(t));
    if (game && !game->is_game_over_) {
        game->MoveDown();
    }
}

void TetrisGame::SpawnPiece() {
    current_piece_ = esp_random() % 7;
    current_rotation_ = 0;
    current_x_ = 3;
    current_y_ = 0;

    if (CheckCollision(current_piece_, current_rotation_, current_x_, current_y_)) {
        is_game_over_ = true;
        lv_label_set_text(game_over_label_, "GAME\nOVER");
    }
}

bool TetrisGame::CheckCollision(int piece, int rotation, int px, int py) {
    uint16_t shape = kPieces[piece][rotation];
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            if (shape & (0x8000 >> (y * 4 + x))) {
                int bx = px + x;
                int by = py + y;
                if (bx < 0 || bx >= kBoardWidth || by >= kBoardHeight) return true;
                if (by >= 0 && board_[by][bx] != 0) return true;
            }
        }
    }
    return false;
}

void TetrisGame::LockPiece() {
    uint16_t shape = kPieces[current_piece_][current_rotation_];
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            if (shape & (0x8000 >> (y * 4 + x))) {
                int bx = current_x_ + x;
                int by = current_y_ + y;
                if (by >= 0 && by < kBoardHeight && bx >= 0 && bx < kBoardWidth) {
                    board_[by][bx] = current_piece_ + 1;
                }
            }
        }
    }
    ClearLines();
    SpawnPiece();
}

void TetrisGame::ClearLines() {
    int lines_cleared = 0;
    for (int y = kBoardHeight - 1; y >= 0; --y) {
        bool full = true;
        for (int x = 0; x < kBoardWidth; ++x) {
            if (board_[y][x] == 0) {
                full = false;
                break;
            }
        }
        if (full) {
            lines_cleared++;
            for (int ty = y; ty > 0; --ty) {
                for (int x = 0; x < kBoardWidth; ++x) {
                    board_[ty][x] = board_[ty - 1][x];
                }
            }
            for (int x = 0; x < kBoardWidth; ++x) {
                board_[0][x] = 0;
            }
            y++; // Check same row again
        }
    }

    if (lines_cleared > 0) {
        lines_ += lines_cleared;
        score_ += (lines_cleared == 1 ? 100 : lines_cleared == 2 ? 300 : lines_cleared == 3 ? 500 : 800);
        char buf[32];
        snprintf(buf, sizeof(buf), "SCORE\n%d", score_);
        lv_label_set_text(score_label_, buf);
        snprintf(buf, sizeof(buf), "LINES\n%d", lines_);
        lv_label_set_text(lines_label_, buf);
    }
}

void TetrisGame::MoveLeft() {
    if (is_game_over_) return;
    if (!CheckCollision(current_piece_, current_rotation_, current_x_ - 1, current_y_)) {
        current_x_--;
        UpdateDisplay();
    }
}

void TetrisGame::MoveRight() {
    if (is_game_over_) return;
    if (!CheckCollision(current_piece_, current_rotation_, current_x_ + 1, current_y_)) {
        current_x_++;
        UpdateDisplay();
    }
}

void TetrisGame::MoveDown() {
    if (is_game_over_) return;
    if (!CheckCollision(current_piece_, current_rotation_, current_x_, current_y_ + 1)) {
        current_y_++;
        UpdateDisplay();
    } else {
        LockPiece();
        UpdateDisplay();
    }
}

void TetrisGame::Drop() {
    if (is_game_over_) return;
    while (!CheckCollision(current_piece_, current_rotation_, current_x_, current_y_ + 1)) {
        current_y_++;
    }
    LockPiece();
    UpdateDisplay();
}

void TetrisGame::Rotate() {
    if (is_game_over_) return;
    int next_rot = (current_rotation_ + 1) % 4;
    if (!CheckCollision(current_piece_, next_rot, current_x_, current_y_)) {
        current_rotation_ = next_rot;
        UpdateDisplay();
    }
}

void TetrisGame::UpdateDisplay() {
    // Render static board
    for (int y = 0; y < kBoardHeight; ++y) {
        for (int x = 0; x < kBoardWidth; ++x) {
            uint8_t color_idx = board_[y][x];
            lv_obj_set_style_bg_color(blocks_[y][x], lv_color_hex(kTetrominoHexColors[color_idx]), 0);
        }
    }

    // Render active falling piece
    if (!is_game_over_) {
        uint16_t shape = kPieces[current_piece_][current_rotation_];
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                if (shape & (0x8000 >> (y * 4 + x))) {
                    int bx = current_x_ + x;
                    int by = current_y_ + y;
                    if (by >= 0 && by < kBoardHeight && bx >= 0 && bx < kBoardWidth) {
                        lv_obj_set_style_bg_color(blocks_[by][bx], lv_color_hex(kTetrominoHexColors[current_piece_ + 1]), 0);
                    }
                }
            }
        }
    }
}
