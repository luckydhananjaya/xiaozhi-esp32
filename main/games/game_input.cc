#include "game_input.h"

#include "breakout.h"
#include "flappy.h"
#include "game_manager.h"
#include "gameboy.h"
#include "invaders.h"
#include "pong.h"
#include "puzzle2048.h"
#include "snake.h"
#include "tetris.h"

namespace {
int breakout_x = 120;
int pong_y = 90;
}  // namespace

void ApplyGamePadAction(GamePadAction action) {
    switch (GameManager::GetInstance().CurrentGame()) {
        case GameType::kTetris:
            if (!TetrisGame::instance_) {
                return;
            }
            if (action == GamePadAction::kLeft) {
                TetrisGame::instance_->MoveLeft();
            } else if (action == GamePadAction::kRight) {
                TetrisGame::instance_->MoveRight();
            } else if (action == GamePadAction::kDown) {
                TetrisGame::instance_->MoveDown();
            } else if (action == GamePadAction::kAction) {
                TetrisGame::instance_->Rotate();
            } else if (action == GamePadAction::kDrop) {
                TetrisGame::instance_->Drop();
            }
            break;
        case GameType::kBreakout:
            if (!BreakoutGame::instance_) {
                return;
            }
            if (action == GamePadAction::kLeft) {
                breakout_x -= 18;
            } else if (action == GamePadAction::kRight) {
                breakout_x += 18;
            } else if (action == GamePadAction::kAction) {
                breakout_x = 120;
            }
            BreakoutGame::instance_->SetPaddleX(breakout_x);
            break;
        case GameType::kSnake:
            if (!SnakeGame::instance_) {
                return;
            }
            if (action == GamePadAction::kLeft) {
                SnakeGame::instance_->ChangeDirection(-1, 0);
            } else if (action == GamePadAction::kRight) {
                SnakeGame::instance_->ChangeDirection(1, 0);
            } else if (action == GamePadAction::kUp) {
                SnakeGame::instance_->ChangeDirection(0, -1);
            } else if (action == GamePadAction::kDown) {
                SnakeGame::instance_->ChangeDirection(0, 1);
            }
            break;
        case GameType::kInvaders:
            if (!InvadersGame::instance_) {
                return;
            }
            if (action == GamePadAction::kLeft) {
                InvadersGame::instance_->MoveLeft();
            } else if (action == GamePadAction::kRight) {
                InvadersGame::instance_->MoveRight();
            } else if (action == GamePadAction::kAction) {
                InvadersGame::instance_->Fire();
            }
            break;
        case GameType::kFlappy:
            if (FlappyGame::instance_ &&
                (action == GamePadAction::kAction || action == GamePadAction::kUp)) {
                FlappyGame::instance_->Flap();
            }
            break;
        case GameType::kPong:
            if (!PongGame::instance_) {
                return;
            }
            if (action == GamePadAction::kUp) {
                pong_y -= 14;
            } else if (action == GamePadAction::kDown) {
                pong_y += 14;
            }
            PongGame::instance_->SetPlayerPaddleY(pong_y);
            break;
        case GameType::kPuzzle2048:
            if (!Puzzle2048Game::instance_) {
                return;
            }
            if (action == GamePadAction::kUp) {
                Puzzle2048Game::instance_->Move(0);
            } else if (action == GamePadAction::kDown) {
                Puzzle2048Game::instance_->Move(1);
            } else if (action == GamePadAction::kLeft) {
                Puzzle2048Game::instance_->Move(2);
            } else if (action == GamePadAction::kRight) {
                Puzzle2048Game::instance_->Move(3);
            }
            break;
        case GameType::kGameBoy:
            if (!GameBoyApp::instance_) {
                return;
            }
            // Same bit values as peanut_gb.h JOYPAD_* without including that header.
            if (action == GamePadAction::kLeft) {
                GameBoyApp::instance_->HandleButton(0x20, true);
            } else if (action == GamePadAction::kRight) {
                GameBoyApp::instance_->HandleButton(0x10, true);
            } else if (action == GamePadAction::kUp) {
                GameBoyApp::instance_->HandleButton(0x40, true);
            } else if (action == GamePadAction::kDown) {
                GameBoyApp::instance_->HandleButton(0x80, true);
            } else if (action == GamePadAction::kAction) {
                GameBoyApp::instance_->HandleButton(0x01, true);
            } else if (action == GamePadAction::kDrop) {
                GameBoyApp::instance_->HandleButton(0x02, true);
            }
            break;
        default:
            break;
    }
}
