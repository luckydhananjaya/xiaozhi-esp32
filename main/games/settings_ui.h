#ifndef SETTINGS_UI_H
#define SETTINGS_UI_H

#include <lvgl.h>
#include <stdint.h>

class SettingsUI {
public:
    static SettingsUI* instance_;
    static lv_obj_t* CreateScreen();
    static void Destroy();

    void InitUI(lv_obj_t* parent);
    void SetBrightness(int val);
    void SetVolume(int val);

private:
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* brightness_slider_ = nullptr;
    lv_obj_t* brightness_label_ = nullptr;
    lv_obj_t* volume_slider_ = nullptr;
    lv_obj_t* volume_label_ = nullptr;

    int current_brightness_ = 100;
    int current_volume_ = 70;

    static void ExitBtnCb(lv_event_t* e);
    static void BrightnessSliderCb(lv_event_t* e);
    static void VolumeSliderCb(lv_event_t* e);
    static void BrightnessStepCb(lv_event_t* e);
    static void VolumeStepCb(lv_event_t* e);
};

#endif // SETTINGS_UI_H
