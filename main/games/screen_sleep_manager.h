#ifndef SCREEN_SLEEP_MANAGER_H
#define SCREEN_SLEEP_MANAGER_H

#include <cstdint>
#include <esp_timer.h>

class ScreenSleepManager {
public:
    static ScreenSleepManager& GetInstance();

    void Init();
    void WakeUp();
    void Sleep();

    // Returns true if screen was asleep and woke up (touch should be consumed)
    bool HandleTouch();

    void SetTimeout(int seconds);
    int GetTimeout() const { return timeout_seconds_; }
    bool IsSleeping() const { return is_sleeping_; }

    void OnVoiceOrNotification();

private:
    ScreenSleepManager();
    ~ScreenSleepManager();

    static void TimerCallback(void* arg);
    void PeriodicCheck();

    bool is_sleeping_ = false;
    int timeout_seconds_ = 30; // Default 30 seconds
    int64_t last_activity_time_ms_ = 0;
    esp_timer_handle_t timer_ = nullptr;
};

#endif // SCREEN_SLEEP_MANAGER_H
