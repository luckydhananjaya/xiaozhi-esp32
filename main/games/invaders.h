#ifndef INVADERS_H
#define INVADERS_H

#include <lvgl.h>
#include <stdint.h>

struct Bullet {
    float x;
    float y;
    bool active;
};

class InvadersGame {
public:
    static InvadersGame* instance_;
    static lv_obj_t* CreateScreen();
    static void Destroy();

    void InitUI(lv_obj_t* parent);
    void Start();
    void Stop();

    void MoveLeft();
    void MoveRight();
    void Fire();

private:
    static const int kRows = 3;
    static const int kCols = 6;
    static const int kAlienW = 20;
    static const int kAlienH = 14;

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* play_area_ = nullptr;
    lv_obj_t* player_obj_ = nullptr;
    lv_obj_t* score_label_ = nullptr;
    lv_obj_t* game_over_label_ = nullptr;
    lv_timer_t* timer_ = nullptr;

    lv_obj_t* alien_objs_[kRows][kCols] = {};
    bool alien_alive_[kRows][kCols] = {};

    static const int kMaxPlayerBullets = 3;
    lv_obj_t* player_bullet_objs_[kMaxPlayerBullets] = {};
    Bullet player_bullets_[kMaxPlayerBullets] = {};

    static const int kMaxAlienBullets = 2;
    lv_obj_t* alien_bullet_objs_[kMaxAlienBullets] = {};
    Bullet alien_bullets_[kMaxAlienBullets] = {};

    float player_x_ = 140.0f;
    float fleet_x_ = 20.0f;
    float fleet_y_ = 25.0f;
    float fleet_dx_ = 1.4f;
    int score_ = 0;
    bool game_over_ = false;

    void ResetGame();
    void UpdateGame();
    void Render();

    static void TimerCallback(lv_timer_t* t);
};

#endif // INVADERS_H
