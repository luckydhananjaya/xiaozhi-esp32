#ifndef PUZZLE2048_H
#define PUZZLE2048_H

#include <lvgl.h>
#include <stdint.h>

class Puzzle2048Game {
public:
    static Puzzle2048Game* instance_;
    static lv_obj_t* CreateScreen();
    static void Destroy();

    void InitUI(lv_obj_t* parent);
    void Move(int dir); // 0=Up, 1=Down, 2=Left, 3=Right

private:
    static const int kSize = 4;
    static const int kTileSize = 42;

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* grid_container_ = nullptr;
    lv_obj_t* score_label_ = nullptr;
    lv_obj_t* game_over_label_ = nullptr;

    lv_obj_t* tile_objs_[kSize][kSize] = {};
    lv_obj_t* tile_labels_[kSize][kSize] = {};

    int board_[kSize][kSize] = {};
    int score_ = 0;
    bool game_over_ = false;

    void ResetGame();
    void SpawnRandomTile();
    bool CanMove();
    void Render();
    uint32_t GetTileColor(int val);
};

#endif // PUZZLE2048_H
