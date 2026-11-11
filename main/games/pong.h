#ifndef PONG_H
#define PONG_H

#include <lvgl.h>
#include <stdint.h>

class PongGame {
public:
    static PongGame* instance_;
    static lv_obj_t* CreateScreen();
    static void Destroy();

    void InitUI(lv_obj_t* parent);
    void Start();
    void Stop();

    void SetPlayerPaddleY(int y);

private:
    static const int kPaddleW = 10;
    static const int kPaddleH = 46;
    static const int kBallSize = 8;

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* play_area_ = nullptr;
    lv_obj_t* player_paddle_ = nullptr;
    lv_obj_t* ai_paddle_ = nullptr;
    lv_obj_t* ball_obj_ = nullptr;
    lv_obj_t* score_label_ = nullptr;
    lv_obj_t* message_label_ = nullptr;
    lv_timer_t* timer_ = nullptr;

    float ball_x_ = 160.0f;
    float ball_y_ = 110.0f;
    float ball_dx_ = 3.2f;
    float ball_dy_ = 1.8f;

    float player_y_ = 90.0f;
    float ai_y_ = 90.0f;

    int player_score_ = 0;
    int ai_score_ = 0;
    bool game_over_ = false;

    void ResetBall();
    void ResetGame();
    void UpdatePhysics();
    void Render();

    static void TimerCallback(lv_timer_t* t);
    static void TouchEventCallback(lv_event_t* e);
};

#endif // PONG_H
