#include "kawan_hid.h"

#include "games/game_input.h"
#include "games/game_manager.h"
#include "kawan_menu.h"
#include "settings.h"

#include <esp_log.h>

#include <cstring>
#include <mutex>

#if CONFIG_BT_BLE_ENABLED
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <esp_hidh.h>
#include <esp_hidh_bluedroid.h>
#endif

#define TAG "KawanHid"

namespace {
std::mutex mu;
bool started = false;
bool scanning = false;
bool connected = false;
std::string status = "BLE idle";
std::vector<KawanHidDevice> devices;
#if CONFIG_BT_BLE_ENABLED
esp_hidh_dev_t* hid = nullptr;

void DispatchPad(GamePadAction action) {
    if (KawanMenu::IsOpen()) {
        if (action == GamePadAction::kUp || action == GamePadAction::kLeft) {
            KawanMenu::OnUp();
        } else if (action == GamePadAction::kDown || action == GamePadAction::kRight) {
            KawanMenu::OnDown();
        } else if (action == GamePadAction::kAction) {
            KawanMenu::OnConfirm();
        } else if (action == GamePadAction::kDrop) {
            KawanMenu::OnBack();
        }
        return;
    }
    if (KawanUiUsesButtons()) {
        if (action == GamePadAction::kUp || action == GamePadAction::kLeft) {
            KawanUiMove(-1);
        } else if (action == GamePadAction::kDown || action == GamePadAction::kRight) {
            KawanUiMove(1);
        } else if (action == GamePadAction::kAction) {
            KawanUiActivate();
        } else if (action == GamePadAction::kDrop) {
            GameManager::GetInstance().ExitToChatbot();
            KawanUiClearFocus();
        }
        return;
    }
    if (GameManager::GetInstance().IsInGame() && action == GamePadAction::kDrop) {
        GameManager::GetInstance().ExitCurrentGame();
        return;
    }
    ApplyGamePadAction(action);
}

void MapReport(const uint8_t* data, uint16_t length) {
    if (data == nullptr || length == 0) {
        return;
    }
    static uint16_t last_buttons = 0;
    uint16_t buttons = data[0];
    if (length > 1) {
        buttons |= static_cast<uint16_t>(data[1]) << 8;
    }
    const uint16_t pressed = buttons & ~last_buttons;
    last_buttons = buttons;
    if (pressed & 0x01) {
        DispatchPad(GamePadAction::kAction);
    }
    if (pressed & 0x02) {
        DispatchPad(GamePadAction::kDrop);
    }
    GamePadAction axis = GamePadAction::kAction;
    bool have_axis = false;
    if (length >= 3) {
        const uint8_t hat = data[2] & 0x0f;
        if (hat == 0) {
            axis = GamePadAction::kUp;
            have_axis = true;
        } else if (hat == 2) {
            axis = GamePadAction::kRight;
            have_axis = true;
        } else if (hat == 4) {
            axis = GamePadAction::kDown;
            have_axis = true;
        } else if (hat == 6) {
            axis = GamePadAction::kLeft;
            have_axis = true;
        }
    }
    if (!have_axis && length >= 5) {
        if (data[3] < 64) {
            axis = GamePadAction::kLeft;
            have_axis = true;
        } else if (data[3] > 192) {
            axis = GamePadAction::kRight;
            have_axis = true;
        } else if (data[4] < 64) {
            axis = GamePadAction::kUp;
            have_axis = true;
        } else if (data[4] > 192) {
            axis = GamePadAction::kDown;
            have_axis = true;
        }
    }
    static int last_axis = -1;
    if (!have_axis) {
        last_axis = -1;
    } else if (static_cast<int>(axis) != last_axis) {
        last_axis = static_cast<int>(axis);
        DispatchPad(axis);
    }
}

void HidEvent(void* handler_args, esp_event_base_t base, int32_t id, void* event_data) {
    (void)handler_args;
    (void)base;
    auto* param = static_cast<esp_hidh_event_data_t*>(event_data);
    const auto event = static_cast<esp_hidh_event_t>(id);
    uint8_t report[16];
    uint16_t report_len = 0;
    {
        // MapReport walks the menu and takes this same mutex. Copy the report
        // first so a gamepad packet cannot deadlock the BLE callback.
        std::lock_guard<std::mutex> lock(mu);
        if (event == ESP_HIDH_OPEN_EVENT) {
            connected = param->open.status == ESP_OK;
            status = connected ? "Paired" : "Pair failed";
            if (connected && param->open.dev != nullptr) {
                const uint8_t* bda = esp_hidh_dev_bda_get(param->open.dev);
                if (bda != nullptr) {
                    char text[18];
                    snprintf(text, sizeof(text), "%02x:%02x:%02x:%02x:%02x:%02x", bda[0], bda[1],
                             bda[2], bda[3], bda[4], bda[5]);
                    Settings settings("gamepad", true);
                    settings.SetString("bda", text);
                }
            }
        } else if (event == ESP_HIDH_CLOSE_EVENT) {
            connected = false;
            status = "Disconnected";
            if (param->close.dev != nullptr) {
                esp_hidh_dev_free(param->close.dev);
            }
            hid = nullptr;
        } else if (event == ESP_HIDH_INPUT_EVENT && param->input.data != nullptr) {
            report_len = param->input.length;
            if (report_len > sizeof(report)) {
                report_len = sizeof(report);
            }
            memcpy(report, param->input.data, report_len);
        }
    }
    if (event == ESP_HIDH_INPUT_EVENT && report_len > 0) {
        MapReport(report, report_len);
    }
}

void GapEvent(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
    if (event != ESP_GAP_BLE_SCAN_RESULT_EVT || param == nullptr) {
        if (event == ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT) {
            std::lock_guard<std::mutex> lock(mu);
            scanning = false;
            status = devices.empty() ? "No devices" : "Scan done";
        }
        return;
    }
    if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_CMPL_EVT) {
        std::lock_guard<std::mutex> lock(mu);
        scanning = false;
        status = devices.empty() ? "No devices" : "Scan done";
        return;
    }
    if (param->scan_rst.search_evt != ESP_GAP_SEARCH_INQ_RES_EVT) {
        return;
    }
    uint8_t len = 0;
    uint8_t* name =
        esp_ble_resolve_adv_data(param->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, &len);
    if (name == nullptr || len == 0) {
        name = esp_ble_resolve_adv_data(param->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_SHORT, &len);
    }
    KawanHidDevice device;
    if (name != nullptr && len > 0) {
        device.name.assign(reinterpret_cast<char*>(name), len);
    } else {
        device.name = "HID device";
    }
    memcpy(device.bda, param->scan_rst.bda, 6);
    device.addr_type = param->scan_rst.ble_addr_type;
    device.rssi = param->scan_rst.rssi;
    char text[18];
    snprintf(text, sizeof(text), "%02x:%02x:%02x:%02x:%02x:%02x", device.bda[0], device.bda[1],
             device.bda[2], device.bda[3], device.bda[4], device.bda[5]);
    device.address = text;
    std::lock_guard<std::mutex> lock(mu);
    for (const auto& existing : devices) {
        if (existing.address == device.address) {
            return;
        }
    }
    if (devices.size() < 8) {
        devices.push_back(device);
    }
}

esp_ble_scan_params_t scan_params = {
    .scan_type = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval = 0x50,
    .scan_window = 0x30,
    .scan_duplicate = BLE_SCAN_DUPLICATE_ENABLE,
};
#endif
}  // namespace

namespace KawanHid {
bool Start() {
#if !CONFIG_BT_BLE_ENABLED
    status = "BLE is off in sdkconfig";
    return false;
#else
    std::lock_guard<std::mutex> lock(mu);
    if (started) {
        return true;
    }
    // Classic BT memory may already have been returned to the heap at boot.
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if (esp_bt_controller_init(&cfg) != ESP_OK ||
        esp_bt_controller_enable(ESP_BT_MODE_BLE) != ESP_OK) {
        status = "Controller failed";
        return false;
    }
    if (esp_bluedroid_init() != ESP_OK || esp_bluedroid_enable() != ESP_OK) {
        status = "Bluedroid failed";
        return false;
    }
    esp_ble_gap_register_callback(GapEvent);
    esp_hidh_config_t hid_cfg = {};
    hid_cfg.callback = HidEvent;
    hid_cfg.event_stack_size = 4096;
    if (esp_hidh_init(&hid_cfg) != ESP_OK) {
        status = "HID host failed";
        return false;
    }
    started = true;
    status = "Ready";
    return true;
#endif
}

void Stop() {}

bool Scanning() {
    std::lock_guard<std::mutex> lock(mu);
    return scanning;
}

bool Connected() {
    std::lock_guard<std::mutex> lock(mu);
    return connected;
}

std::string Status() {
    std::lock_guard<std::mutex> lock(mu);
    return status;
}

std::vector<KawanHidDevice> Devices() {
    std::lock_guard<std::mutex> lock(mu);
    return devices;
}

void Scan() {
#if CONFIG_BT_BLE_ENABLED
    if (!Start()) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mu);
        devices.clear();
        scanning = true;
        status = "Scanning";
    }
    esp_ble_gap_set_scan_params(&scan_params);
    esp_ble_gap_start_scanning(5);
#else
    status = "BLE is off in sdkconfig";
#endif
}

bool Connect(int index) {
#if !CONFIG_BT_BLE_ENABLED
    (void)index;
    return false;
#else
    KawanHidDevice device;
    {
        std::lock_guard<std::mutex> lock(mu);
        if (index < 0 || index >= static_cast<int>(devices.size())) {
            return false;
        }
        device = devices[index];
        status = "Pairing " + device.name;
    }
    if (hid != nullptr) {
        esp_hidh_dev_close(hid);
        hid = nullptr;
    }
    hid = esp_hidh_dev_open(device.bda, ESP_HID_TRANSPORT_BLE, device.addr_type);
    return hid != nullptr;
#endif
}

void Forget() {
    Settings settings("gamepad", true);
    settings.EraseKey("bda");
#if CONFIG_BT_BLE_ENABLED
    if (hid != nullptr) {
        esp_hidh_dev_close(hid);
        hid = nullptr;
    }
#endif
    std::lock_guard<std::mutex> lock(mu);
    connected = false;
    status = "Forgotten";
}
}  // namespace KawanHid
