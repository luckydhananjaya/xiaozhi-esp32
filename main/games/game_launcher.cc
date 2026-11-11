#include "game_launcher.h"
#include "game_manager.h"
#include <esp_log.h>

#define TAG "GameLauncher"

struct GameItem {
    GameType type;
    const char* title;
    const char* icon;
    const char* badge;
    uint32_t color_hex;
};

static const GameItem kGames[] = {
    { GameType::kTetris,    "TETRIS",   "[]", "PLAY",  0x0284C7 }, // Blue
    { GameType::kBreakout,  "BRICKS",   "==", "PLAY",  0x16A34A }, // Green
    { GameType::kSnake,     "SNAKE",    "~~", "PLAY",  0xCA8A04 }, // Yellow
    { GameType::kInvaders,  "INVADERS", "><", "PLAY",  0x9333EA }, // Purple
    { GameType::kFlappy,    "FLAPPY",   "^v", "PLAY",  0xEA580C }, // Orange
    { GameType::kPuzzle2048,"2048",     "##", "PLAY",  0xDC2626 }, // Red
    { GameType::kPong,      "PONG",     "||", "PLAY",  0x0D9488 }, // Teal
    { GameType::kGameBoy,   "GAMEBOY",  "GB", "CARD",  0x4F46E5 }, // Indigo
};

static void GameCardEventCb(lv_event_t* e) {
    auto type = static_cast<GameType>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    if (type != GameType::kNone) {
        GameManager::GetInstance().StartGame(type);
    }
}

static void ExitBtnEventCb(lv_event_t* e) {
    GameManager::GetInstance().ExitToChatbot();
}

lv_obj_t* GameLauncher::CreateScreen() {
    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_set_size(scr, 320, 240);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0A0F1D), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

    // Top Header
    lv_obj_t* header = lv_obj_create(scr);
    lv_obj_set_size(header, 320, 36);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_style_pad_left(header, 12, 0);
    lv_obj_set_style_pad_right(header, 8, 0);
    lv_obj_set_scrollbar_mode(header, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "RETRO ARCADE (8 GAMES)");
    lv_obj_set_style_text_color(title, lv_color_hex(0xF8FAFC), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 4, 0);

    // EXIT Button
    lv_obj_t* exit_btn = lv_button_create(header);
    lv_obj_set_size(exit_btn, 65, 26);
    lv_obj_align(exit_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(exit_btn, lv_color_hex(0xDC2626), 0); // Red
    lv_obj_set_style_radius(exit_btn, 4, 0);
    lv_obj_add_event_cb(exit_btn, ExitBtnEventCb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* exit_label = lv_label_create(exit_btn);
    lv_label_set_text(exit_label, "EXIT [X]");
    lv_obj_set_style_text_color(exit_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(exit_label);

    // Grid Container for 8 cards
    lv_obj_t* grid = lv_obj_create(scr);
    lv_obj_set_size(grid, 320, 204);
    lv_obj_align(grid, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 6, 0);
    lv_obj_set_scrollbar_mode(grid, LV_SCROLLBAR_MODE_OFF);

    // 2 rows of 4 columns
    for (size_t i = 0; i < 8; ++i) {
        int col = i % 4;
        int row = i / 4;
        int x = 8 + col * 76;
        int y = 4 + row * 96;

        lv_obj_t* card = lv_button_create(grid);
        lv_obj_set_pos(card, x, y);
        lv_obj_set_size(card, 70, 90);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x1E293B), 0);
        lv_obj_set_style_border_width(card, 2, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(kGames[i].color_hex), 0);
        lv_obj_set_style_radius(card, 6, 0);
        lv_obj_set_style_pad_all(card, 2, 0);

        // Icon Box
        lv_obj_t* icon_lbl = lv_label_create(card);
        lv_label_set_text(icon_lbl, kGames[i].icon);
        lv_obj_set_style_text_color(icon_lbl, lv_color_hex(kGames[i].color_hex), 0);
        lv_obj_align(icon_lbl, LV_ALIGN_TOP_MID, 0, 8);

        // Title
        lv_obj_t* title_lbl = lv_label_create(card);
        lv_label_set_text(title_lbl, kGames[i].title);
        lv_obj_set_style_text_color(title_lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(title_lbl, LV_ALIGN_CENTER, 0, 4);

        // Badge
        lv_obj_t* badge = lv_obj_create(card);
        lv_obj_set_size(badge, 48, 16);
        lv_obj_align(badge, LV_ALIGN_BOTTOM_MID, 0, -4);
        lv_obj_set_style_bg_color(badge, lv_color_hex(kGames[i].color_hex), 0);
        lv_obj_set_style_border_width(badge, 0, 0);
        lv_obj_set_style_radius(badge, 3, 0);
        lv_obj_set_style_pad_all(badge, 0, 0);

        lv_obj_t* badge_lbl = lv_label_create(badge);
        lv_label_set_text(badge_lbl, kGames[i].badge);
        lv_obj_set_style_text_color(badge_lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(badge_lbl);

        lv_obj_add_event_cb(card, GameCardEventCb, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<intptr_t>(kGames[i].type)));
    }

    return scr;
}
