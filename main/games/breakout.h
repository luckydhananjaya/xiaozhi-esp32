#ifndef BREAKOUT_H
#define BREAKOUT_H

#include <lvgl.h>
#include <stdint.h>

class BreakoutGame {
public:
    static BreakoutGame* instance_;
    static lv_obj_t* CreateScreen();
    static void Destroy();

    void InitUI(lv_obj_t* parent);
    void Start();
    void Stop();

    void SetPaddleX(int x);

private:
    static const int kBrickRows = 5;
    static const int kBrickCols = 6;
    static const int kPaddleWidth = 50;
    static const int kPaddleHeight = 12;
    static const int kBallSize = 8;

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* paddle_obj_ = nullptr;
    lv_obj_t* ball_obj_ = nullptr;
    lv_obj_t* score_label_ = nullptr;
    lv_obj_t* lives_label_ = nullptr;
    lv_obj_t* message_label_ = nullptr;
    lv_obj_t* touch_area_ = nullptr;
    lv_timer_t* timer_ = nullptr;

    lv_obj_t* bricks_[kBrickRows][kBrickCols] = {};
    bool brick_alive_[kBrickRows][kBrickCols] = {};

    float ball_x_ = 160.0f;
    float ball_y_ = 150.0f;
    float ball_dx_ = 2.5f;
    float ball_dy_ = -2.5f;
    int paddle_x_ = 132;
    int score_ = 0;
    int lives_ = 3;
    bool game_over_ = false;

    void ResetBall();
    void UpdatePhysics();
    void ResetGame();

    static void TimerCallback(lv_timer_t* t);
    static void TouchEventCallback(lv_event_t* e);
};

#endif // BREAKOUT_H
