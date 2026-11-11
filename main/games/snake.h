#ifndef SNAKE_H
#define SNAKE_H

#include <lvgl.h>
#include <stdint.h>
#include <deque>

struct Point {
    int x;
    int y;
    bool operator==(const Point& o) const { return x == o.x && y == o.y; }
};

class SnakeGame {
public:
    static SnakeGame* instance_;
    static lv_obj_t* CreateScreen();
    static void Destroy();

    void InitUI(lv_obj_t* parent);
    void Start();
    void Stop();

    void ChangeDirection(int dx, int dy);

private:
    static const int kGridWidth = 18;
    static const int kGridHeight = 18;
    static const int kCellSize = 12; // 216 x 216 px

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* board_ = nullptr;
    lv_obj_t* score_label_ = nullptr;
    lv_obj_t* game_over_label_ = nullptr;
    lv_timer_t* timer_ = nullptr;

    lv_obj_t* cells_[kGridHeight][kGridWidth] = {};

    std::deque<Point> snake_;
    Point food_;
    int dir_x_ = 1;
    int dir_y_ = 0;
    int next_dir_x_ = 1;
    int next_dir_y_ = 0;
    int score_ = 0;
    bool game_over_ = false;

    void SpawnFood();
    void Step();
    void Render();
    void ResetGame();

    static void TimerCallback(lv_timer_t* t);
};

#endif // SNAKE_H
