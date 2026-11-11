#ifndef GAME_MANAGER_H
#define GAME_MANAGER_H

#include <lvgl.h>
#include <string>
#include <functional>

enum class GameType {
    kNone,
    kTetris,
    kBreakout,
    kSnake,
    kInvaders,
    kFlappy,
    kPuzzle2048,
    kPong,
    kGameBoy,
    kSettings
};

class GameManager {
public:
    static GameManager& GetInstance();

    void Init();
    void ShowLauncher();
    void ShowSettings();
    void StartGame(GameType game);
    void ExitCurrentGame();
    void ExitToChatbot();

    bool IsInGame() const { return current_game_ != GameType::kNone; }
    bool IsInLauncher() const { return in_launcher_; }

    void SetOrientationCallback(std::function<void(bool portrait)> cb) { orientation_callback_ = cb; }

private:
    GameManager();
    ~GameManager();

    std::function<void(bool portrait)> orientation_callback_ = nullptr;

    lv_obj_t* chatbot_screen_ = nullptr;
    lv_obj_t* launcher_screen_ = nullptr;
    lv_obj_t* game_screen_ = nullptr;
    lv_obj_t* games_btn_ = nullptr;

    GameType current_game_ = GameType::kNone;
    bool in_launcher_ = false;

    void CreateFloatingGamesButton();
};

#endif // GAME_MANAGER_H
