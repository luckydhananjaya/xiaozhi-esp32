#include "kawan_menu.h"

#include "kawan_hid.h"
#include "kawan_video.h"

#include "display.h"
#include "games/game_manager.h"
#include "settings.h"
#include "wifi_board.h"

#include <esp_log.h>
#include <esp_wifi.h>
#include <ssid_manager.h>
#include <wifi_manager.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#define TAG "KawanMenu"

namespace {
enum class Page { kClosed, kHome, kWifi, kEdit, kGamepad };

Page page = Page::kClosed;
lv_obj_t* root = nullptr;
int row = 0;
std::vector<lv_obj_t*> focusables;
int focus = 0;

enum class EditTarget { kPassword, kServer, kGemini };
EditTarget edit_target = EditTarget::kServer;
std::string edit_text;
int edit_char = 0;
std::string pending_ssid;

const char kAlphabet[] = " abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._:/?&=%";

struct Ap {
    std::string ssid;
    int rssi;
    bool secure;
};
std::vector<Ap> aps;

const char* kHome[] = {
    "Games",          "Settings", "WiFi scan", "Server URL",
    "Gemini API key", "Gamepad",  "Video",     "Captive portal",
};

void Redraw();

void CollectButtons(lv_obj_t* obj) {
    if (obj == nullptr) {
        return;
    }
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }
    if (lv_obj_check_type(obj, &lv_button_class)) {
        focusables.push_back(obj);
    }
    const uint32_t count = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < count; ++i) {
        CollectButtons(lv_obj_get_child(obj, i));
    }
}

void PaintFocus() {
    for (size_t i = 0; i < focusables.size(); ++i) {
        lv_obj_set_style_outline_width(focusables[i], i == static_cast<size_t>(focus) ? 3 : 0, 0);
        lv_obj_set_style_outline_color(focusables[i], lv_color_hex(0xFACC15), 0);
    }
}

lv_obj_t* Label(lv_obj_t* parent, const char* text, int y) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_width(label, lv_pct(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(label, lv_color_hex(0xE2E8F0), 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 6, y);
    return label;
}

void EnsureRoot() {
    if (root != nullptr) {
        return;
    }
    root = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(root, lv_color_hex(0x020617), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
}

void SaveEdit() {
    if (edit_target == EditTarget::kPassword) {
        SsidManager::GetInstance().AddSsid(pending_ssid, edit_text);
        WifiManager::GetInstance().StartStation();
        ESP_LOGI(TAG, "Saved WiFi %s", pending_ssid.c_str());
    } else if (edit_target == EditTarget::kServer) {
        Settings wifi("wifi", true);
        wifi.SetString("ota_url", edit_text);
        Settings ws("websocket", true);
        if (edit_text.rfind("ws", 0) == 0) {
            ws.SetString("url", edit_text);
        }
        ESP_LOGI(TAG, "Saved server URL (%u chars)", static_cast<unsigned>(edit_text.size()));
    } else {
        Settings gemini("gemini", true);
        gemini.SetString("api_key", edit_text);
        ESP_LOGI(TAG, "Saved Gemini API key (%u chars)", static_cast<unsigned>(edit_text.size()));
    }
}

void DrawHome() {
    lv_obj_clean(root);
    Label(root, "40 next  40-hold open  39-hold back", 2);
    const int count = static_cast<int>(sizeof(kHome) / sizeof(kHome[0]));
    if (row < 0) {
        row = count - 1;
    }
    if (row >= count) {
        row = 0;
    }
    for (int i = 0; i < count; ++i) {
        lv_obj_t* label = Label(root, kHome[i], 22 + i * 22);
        if (i == row) {
            lv_obj_set_style_text_color(label, lv_color_hex(0x38BDF8), 0);
        }
    }
}

void DrawWifi() {
    lv_obj_clean(root);
    Label(root, "WiFi  40-hold join  39-hold back", 2);
    if (aps.empty()) {
        Label(root, "No networks. Hold boot, then scan again.", 28);
        return;
    }
    if (row < 0) {
        row = static_cast<int>(aps.size()) - 1;
    }
    if (row >= static_cast<int>(aps.size())) {
        row = 0;
    }
    const int window = 8;
    int start = std::max(0, row - 3);
    if (start + window > static_cast<int>(aps.size())) {
        start = std::max(0, static_cast<int>(aps.size()) - window);
    }
    for (int line_no = 0; line_no < window && start + line_no < static_cast<int>(aps.size());
         ++line_no) {
        const Ap& ap = aps[start + line_no];
        std::string line = ap.ssid + " " + std::to_string(ap.rssi);
        lv_obj_t* label = Label(root, line.c_str(), 24 + line_no * 22);
        if (start + line_no == row) {
            lv_obj_set_style_text_color(label, lv_color_hex(0x38BDF8), 0);
        }
    }
}

void DrawEdit() {
    lv_obj_clean(root);
    const char* title = "Edit";
    if (edit_target == EditTarget::kPassword) {
        title = "WiFi password";
    } else if (edit_target == EditTarget::kServer) {
        title = "Server URL";
    } else {
        title = "Gemini API key";
    }
    Label(root, title, 2);
    Label(root, "40:next  40-hold:type  39-hold:delete", 22);
    std::string shown = edit_text;
    if (edit_target == EditTarget::kGemini && !shown.empty()) {
        shown.assign(shown.size(), '*');
    }
    const int alphabet = static_cast<int>(std::strlen(kAlphabet));
    const char* key = "OK";
    if (edit_char >= 0 && edit_char < alphabet) {
        key = nullptr;
    }
    if (key == nullptr) {
        const char ch = kAlphabet[edit_char];
        shown.push_back('[');
        shown.push_back(ch == ' ' ? '_' : ch);
        shown.push_back(']');
    } else if (edit_char == alphabet) {
        shown += "  [OK]";
    } else {
        shown += "  [DEL]";
    }
    Label(root, shown.c_str(), 56);
}

void DrawGamepad() {
    lv_obj_clean(root);
    Label(root, "Gamepad  40:next  40-hold:pair  39-hold:back", 2);
    Label(root, KawanHid::Status().c_str(), 22);
    auto devices = KawanHid::Devices();
    const int count = static_cast<int>(devices.size()) + 1;
    if (row < 0) {
        row = count - 1;
    }
    if (row >= count) {
        row = 0;
    }
    if (devices.empty()) {
        Label(root, KawanHid::Scanning() ? "Scanning..." : "No devices yet", 48);
    }
    const int shown = std::min(static_cast<int>(devices.size()), 7);
    for (int i = 0; i < shown; ++i) {
        std::string line = devices[i].name + " " + std::to_string(devices[i].rssi);
        lv_obj_t* label = Label(root, line.c_str(), 46 + i * 20);
        if (i == row) {
            lv_obj_set_style_text_color(label, lv_color_hex(0x38BDF8), 0);
        }
    }
    lv_obj_t* forget = Label(root, "Forget paired device", 46 + shown * 20);
    if (row >= static_cast<int>(devices.size())) {
        lv_obj_set_style_text_color(forget, lv_color_hex(0xF87171), 0);
    }
}

void Redraw() {
    DisplayLockGuard lock(Board::GetInstance().GetDisplay());
    EnsureRoot();
    if (page == Page::kHome) {
        DrawHome();
    } else if (page == Page::kWifi) {
        DrawWifi();
    } else if (page == Page::kEdit) {
        DrawEdit();
    } else if (page == Page::kGamepad) {
        DrawGamepad();
    }
}

void ScanWifi() {
    aps.clear();
    wifi_scan_config_t scan = {};
    scan.show_hidden = false;
    scan.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    scan.scan_time.active.min = 80;
    scan.scan_time.active.max = 200;
    if (esp_wifi_scan_start(&scan, true) != ESP_OK) {
        return;
    }
    uint16_t count = 0;
    esp_wifi_scan_get_ap_num(&count);
    if (count == 0) {
        return;
    }
    std::vector<wifi_ap_record_t> records(count);
    esp_wifi_scan_get_ap_records(&count, records.data());
    for (uint16_t i = 0; i < count && aps.size() < 16; ++i) {
        Ap ap;
        ap.ssid.assign(reinterpret_cast<char*>(records[i].ssid));
        if (ap.ssid.empty()) {
            continue;
        }
        ap.rssi = records[i].rssi;
        ap.secure = records[i].authmode != WIFI_AUTH_OPEN;
        aps.push_back(ap);
    }
    std::sort(aps.begin(), aps.end(), [](const Ap& a, const Ap& b) { return a.rssi > b.rssi; });
}
}  // namespace

namespace KawanMenu {
bool IsOpen() { return page != Page::kClosed; }

void Open() {
    page = Page::kHome;
    row = 0;
    Redraw();
}

void Close() {
    page = Page::kClosed;
    DisplayLockGuard lock(Board::GetInstance().GetDisplay());
    if (root != nullptr) {
        lv_obj_delete(root);
        root = nullptr;
    }
}

int EditKeyCount() { return static_cast<int>(std::strlen(kAlphabet)) + 2; }

void OnUp() {
    if (page == Page::kEdit) {
        const int n = EditKeyCount();
        edit_char = (edit_char + n - 1) % n;
        Redraw();
        return;
    }
    row -= 1;
    Redraw();
}

void OnDown() {
    if (page == Page::kEdit) {
        const int n = EditKeyCount();
        edit_char = (edit_char + 1) % n;
        Redraw();
        return;
    }
    row += 1;
    Redraw();
}

void OnConfirm() {
    if (page == Page::kHome) {
        const int count = static_cast<int>(sizeof(kHome) / sizeof(kHome[0]));
        if (row < 0 || row >= count) {
            return;
        }
        const std::string item = kHome[row];
        if (item == "Games") {
            Close();
            GameManager::GetInstance().ShowLauncher();
        } else if (item == "Settings") {
            Close();
            GameManager::GetInstance().ShowSettings();
        } else if (item == "WiFi scan") {
            ScanWifi();
            page = Page::kWifi;
            row = 0;
            Redraw();
        } else if (item == "Server URL") {
            Settings wifi("wifi", false);
            edit_text = wifi.GetString("ota_url");
            edit_target = EditTarget::kServer;
            edit_char = 0;
            row = -1;
            page = Page::kEdit;
            Redraw();
        } else if (item == "Gemini API key") {
            Settings gemini("gemini", false);
            edit_text = gemini.GetString("api_key");
            edit_target = EditTarget::kGemini;
            edit_char = 0;
            row = -1;
            page = Page::kEdit;
            Redraw();
        } else if (item == "Gamepad") {
            KawanHid::Start();
            KawanHid::Scan();
            page = Page::kGamepad;
            row = 0;
            Redraw();
        } else if (item == "Video") {
            Close();
            KawanVideo::Start("");
        } else if (item == "Captive portal") {
            Close();
            static_cast<WifiBoard&>(Board::GetInstance()).EnterWifiConfigMode();
        }
        return;
    }
    if (page == Page::kWifi) {
        if (aps.empty() || row < 0 || row >= static_cast<int>(aps.size())) {
            return;
        }
        pending_ssid = aps[row].ssid;
        if (!aps[row].secure) {
            SsidManager::GetInstance().AddSsid(pending_ssid, "");
            WifiManager::GetInstance().StartStation();
            page = Page::kHome;
            row = 0;
            Redraw();
            return;
        }
        edit_text.clear();
        edit_target = EditTarget::kPassword;
        edit_char = 0;
        row = -1;
        page = Page::kEdit;
        Redraw();
        return;
    }
    if (page == Page::kEdit) {
        const int alphabet = static_cast<int>(std::strlen(kAlphabet));
        if (edit_char == alphabet) {
            SaveEdit();
            page = Page::kHome;
            row = 0;
        } else if (edit_char > alphabet) {
            if (!edit_text.empty()) {
                edit_text.pop_back();
            }
        } else {
            edit_text.push_back(kAlphabet[edit_char]);
        }
        Redraw();
        return;
    }
    if (page == Page::kGamepad) {
        if (row >= static_cast<int>(KawanHid::Devices().size())) {
            KawanHid::Forget();
        } else {
            KawanHid::Connect(row);
        }
        Redraw();
    }
}

void OnSave() {
    if (page != Page::kEdit) {
        return;
    }
    SaveEdit();
    page = Page::kHome;
    row = 0;
    Redraw();
}

void OnBack() {
    if (page == Page::kEdit && !edit_text.empty()) {
        edit_text.pop_back();
        Redraw();
        return;
    }
    if (page == Page::kHome) {
        Close();
        return;
    }
    page = Page::kHome;
    row = 0;
    Redraw();
}
}  // namespace KawanMenu

bool KawanUiUsesButtons() {
    auto& games = GameManager::GetInstance();
    return games.IsInLauncher() || games.CurrentGame() == GameType::kSettings;
}

void KawanUiMove(int delta) {
    DisplayLockGuard lock(Board::GetInstance().GetDisplay());
    focusables.clear();
    CollectButtons(lv_screen_active());
    if (focusables.empty()) {
        return;
    }
    if (focus < 0) {
        focus = 0;
    }
    focus = (focus + delta) % static_cast<int>(focusables.size());
    if (focus < 0) {
        focus += static_cast<int>(focusables.size());
    }
    PaintFocus();
}

void KawanUiActivate() {
    DisplayLockGuard lock(Board::GetInstance().GetDisplay());
    if (focusables.empty()) {
        CollectButtons(lv_screen_active());
        focus = 0;
        PaintFocus();
        return;
    }
    if (focus < 0 || focus >= static_cast<int>(focusables.size())) {
        focus = 0;
    }
    lv_obj_send_event(focusables[focus], LV_EVENT_CLICKED, nullptr);
    focusables.clear();
    focus = 0;
}

void KawanUiClearFocus() {
    focusables.clear();
    focus = 0;
}
