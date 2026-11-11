#ifndef FLAPPY_H
#define FLAPPY_H

#include <lvgl.h>
#include <stdint.h>
#include <vector>

struct Pipe {
    float x;
    int gap_y;
    int gap_height;
    bool scored;
};

class FlappyGame {
public:
    static FlappyGame* instance_;
    static lv_obj_t* CreateScreen();
    static void Destroy();

    void InitUI(lv_obj_t* parent);
    void Start();
    void Stop();

    void Flap();

private:
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* play_area_ = nullptr;
    lv_obj_t* bird_obj_ = nullptr;
    lv_obj_t* score_label_ = nullptr;
    lv_obj_t* game_over_label_ = nullptr;
    lv_timer_t* timer_ = nullptr;

    // Fixed pool of 3 pipes (each has top and bottom visual rect)
    static const int kMaxPipes = 3;
    lv_obj_t* pipe_top_objs_[kMaxPipes] = {};
    lv_obj_t* pipe_bot_objs_[kMaxPipes] = {};
    Pipe pipes_[kMaxPipes] = {};

    float bird_y_ = 100.0f;
    float bird_vy_ = 0.0f;
    int score_ = 0;
    bool game_over_ = false;

    void ResetGame();
    void UpdatePhysics();
    void Render();

    static void TimerCallback(lv_timer_t* t);
    static void TouchEventCallback(lv_event_t* e);
};

#endif // FLAPPY_H
