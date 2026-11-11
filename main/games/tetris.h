#ifndef TETRIS_H
#define TETRIS_H

#include <lvgl.h>
#include <stdint.h>
#include <vector>

class TetrisGame {
public:
    static TetrisGame* instance_;
    static lv_obj_t* CreateScreen();
    static void Destroy();

    void InitUI(lv_obj_t* parent);
    void Start();
    void Stop();

    void MoveLeft();
    void MoveRight();
    void MoveDown();
    void Drop();
    void Rotate();

private:
    static const int kBoardWidth = 10;
    static const int kBoardHeight = 20;
    static const int kBlockSize = 10;

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* board_container_ = nullptr;
    lv_obj_t* score_label_ = nullptr;
    lv_obj_t* lines_label_ = nullptr;
    lv_obj_t* game_over_label_ = nullptr;
    lv_timer_t* timer_ = nullptr;

    lv_obj_t* blocks_[kBoardHeight][kBoardWidth] = {};

    uint8_t board_[kBoardHeight][kBoardWidth] = {};
    int current_piece_ = 0;
    int current_rotation_ = 0;
    int current_x_ = 0;
    int current_y_ = 0;
    int score_ = 0;
    int lines_ = 0;
    bool is_game_over_ = false;

    void SpawnPiece();
    bool CheckCollision(int piece, int rotation, int x, int y);
    void LockPiece();
    void ClearLines();
    void UpdateDisplay();
    void Reset();

    static void TimerCallback(lv_timer_t* t);
};

#endif // TETRIS_H
