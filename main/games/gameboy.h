#ifndef GAMEBOY_H
#define GAMEBOY_H

#include <lvgl.h>
#include <stdint.h>
#include <string>
#include <vector>

class GameBoyApp {
public:
    static GameBoyApp* instance_;
    static lv_obj_t* CreateScreen();
    static void Destroy();

    void InitUI(lv_obj_t* parent);
    void ScanSDCard();
    void LoadROM(const std::string& path);
    void HandleButton(uint8_t gb_btn, bool pressed);

private:
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* header_ = nullptr;
    lv_obj_t* status_label_ = nullptr;
    lv_obj_t* rom_list_ = nullptr;
    lv_obj_t* game_canvas_ = nullptr;
    lv_timer_t* loop_timer_ = nullptr;

    std::vector<std::string> rom_files_;
    bool rom_loaded_ = false;
    uint8_t* rom_buffer_ = nullptr;
    size_t rom_size_ = 0;

    static void LoopCallback(lv_timer_t* t);
};

#endif // GAMEBOY_H
