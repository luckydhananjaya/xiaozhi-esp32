#include "screen_sleep_manager.h"
#include "boards/common/board.h"
#include "boards/common/backlight.h"
#include "application.h"
#include <esp_log.h>

#define TAG "ScreenSleep"

ScreenSleepManager& ScreenSleepManager::GetInstance() {
    static ScreenSleepManager instance;
    return instance;
}

ScreenSleepManager::ScreenSleepManager() {
    last_activity_time_ms_ = esp_timer_get_time() / 1000;
}

ScreenSleepManager::~ScreenSleepManager() {
    if (timer_) {
        esp_timer_stop(timer_);
        esp_timer_delete(timer_);
        timer_ = nullptr;
    }
}

void ScreenSleepManager::TimerCallback(void* arg) {
    auto self = static_cast<ScreenSleepManager*>(arg);
    self->PeriodicCheck();
}

void ScreenSleepManager::Init() {
    if (timer_) return;

    last_activity_time_ms_ = esp_timer_get_time() / 1000;

    esp_timer_create_args_t timer_args = {
        .callback = TimerCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "screen_sleep_timer",
        .skip_unhandled_events = true
    };
    if (esp_timer_create(&timer_args, &timer_) == ESP_OK) {
        esp_timer_start_periodic(timer_, 1000000); // 1 second interval
        ESP_LOGI(TAG, "ScreenSleepManager initialized (Timeout: %d s)", timeout_seconds_);
    }
}

void ScreenSleepManager::WakeUp() {
    last_activity_time_ms_ = esp_timer_get_time() / 1000;
    if (is_sleeping_) {
        is_sleeping_ = false;
        auto bl = Board::GetInstance().GetBacklight();
        if (bl) {
            bl->RestoreBrightness();
        }
        ESP_LOGI(TAG, "Screen woken up from standby");
    }
}

void ScreenSleepManager::Sleep() {
    if (!is_sleeping_) {
        is_sleeping_ = true;
        auto bl = Board::GetInstance().GetBacklight();
        if (bl) {
            bl->SetBrightness(0, false); // Turn off backlight without altering stored level
        }
        ESP_LOGI(TAG, "Screen entered standby / sleep mode");
    }
}

bool ScreenSleepManager::HandleTouch() {
    if (is_sleeping_) {
        WakeUp();
        return true; // Consume touch event so wake-up tap does not activate widgets
    }
    last_activity_time_ms_ = esp_timer_get_time() / 1000;
    return false;
}

void ScreenSleepManager::SetTimeout(int seconds) {
    timeout_seconds_ = seconds;
    last_activity_time_ms_ = esp_timer_get_time() / 1000;
    if (timeout_seconds_ <= 0 && is_sleeping_) {
        WakeUp();
    }
    ESP_LOGI(TAG, "Screen sleep timeout set to %d seconds", timeout_seconds_);
}

void ScreenSleepManager::OnVoiceOrNotification() {
    WakeUp();
}

void ScreenSleepManager::PeriodicCheck() {
    if (timeout_seconds_ <= 0) return; // Inactivity sleep disabled
    if (is_sleeping_) return;

    // Check if voice audio or active conversation is currently in progress
    auto state = Application::GetInstance().GetDeviceState();
    if (state == kDeviceStateListening || state == kDeviceStateSpeaking) {
        last_activity_time_ms_ = esp_timer_get_time() / 1000;
        return;
    }

    int64_t now_ms = esp_timer_get_time() / 1000;
    if (now_ms - last_activity_time_ms_ >= (static_cast<int64_t>(timeout_seconds_) * 1000)) {
        Sleep();
    }
}
