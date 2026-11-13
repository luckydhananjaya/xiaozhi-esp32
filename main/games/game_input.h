#ifndef GAME_INPUT_H
#define GAME_INPUT_H

// Physical buttons and a BLE gamepad share this. The arcade games themselves
// are unchanged; this only calls the controls they already expose.
enum class GamePadAction {
    kLeft,
    kRight,
    kUp,
    kDown,
    kAction,
    kDrop,
};

void ApplyGamePadAction(GamePadAction action);

#endif  // GAME_INPUT_H
