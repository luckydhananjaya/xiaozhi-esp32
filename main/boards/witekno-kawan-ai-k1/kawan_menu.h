#pragma once

#include "games/game_input.h"

// On-screen setup for a board with three buttons and no touch.
// Boot click confirms, volume up/down moves, boot long-press goes back.
// Double-click boot opens this menu from the chat screen.
namespace KawanMenu {
bool IsOpen();
void Open();
void Close();
void OnUp();
void OnDown();
void OnConfirm();
void OnBack();
void OnSave();
}  // namespace KawanMenu

// True when the arcade launcher or settings screen should be driven by clicking
// its existing buttons instead of the game-pad actions.
bool KawanUiUsesButtons();
void KawanUiMove(int delta);
void KawanUiActivate();
void KawanUiClearFocus();
