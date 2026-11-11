#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

#include <wifi_station.h>
#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "adc_battery_monitor.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>

#include "led/single_led.h"
#include "system_reset.h"
#include "esp_lcd_ili9341.h"
#include "games/game_manager.h"

#define TAG "FreenoveESP32S3Display"

class TouchDriver {
public:
    TouchDriver() : dev_(nullptr) {}

    bool Init(i2c_master_bus_handle_t bus, uint8_t addr) {
        i2c_device_config_t cfg = {
            .device_address = addr,
            .scl_speed_hz = 400000,
            .scl_wait_us = 0,
        };
        if (i2c_master_bus_add_device(bus, &cfg, &dev_) != ESP_OK) {
            return false;
        }

        // Boost touch sensitivity (threshold 8 for ultra-responsive, light touch)
        uint8_t th_cmd[] = { 0x80, 0x08 };
        i2c_master_transmit(dev_, th_cmd, 2, 50);

        // Boost active scan rate to 100Hz+ for zero lag
        uint8_t period_cmd[] = { 0x88, 0x05 };
        i2c_master_transmit(dev_, period_cmd, 2, 50);
        return true;
    }

    bool Read(bool &touched, uint16_t &x, uint16_t &y) {
        touched = false;
        x = y = 0;
        if (!dev_) return false;

        uint8_t reg = 0x02;
        uint8_t buf[5];
        if (i2c_master_transmit_receive(dev_, &reg, 1, buf, 5, 50) != ESP_OK) return false;

        uint8_t points = buf[0] & 0x0F;
        if (points == 0) {
            touched = false;
            return true;
        }

        touched = true;
        uint16_t raw_x = ((buf[1] & 0x0F) << 8) | buf[2];
        uint16_t raw_y = ((buf[3] & 0x0F) << 8) | buf[4];

        // 100% accurate native Landscape 320x240 mapping for FT6336 on Hosyond 2.8" TFT:
        x = raw_y;
        y = (raw_x < 240) ? (239 - raw_x) : 0;
        return true;
    }

private:
    i2c_master_dev_handle_t dev_;
};

static TouchDriver* g_touch_driver = nullptr;

static void TouchpadReadCallback(lv_indev_t* indev, lv_indev_data_t* data) {
    if (!g_touch_driver) return;
    bool touched = false;
    uint16_t x = 0, y = 0;
    if (g_touch_driver->Read(touched, x, y) && touched) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

class HosyondDisplay : public SpiLcdDisplay {
public:
    using SpiLcdDisplay::SpiLcdDisplay;

    virtual void SetupUI() override {
        SpiLcdDisplay::SetupUI();

        DisplayLockGuard lock(this);

        auto toggle_chat_cb = [](lv_event_t* e) {
            if (!GameManager::GetInstance().IsInLauncher() && !GameManager::GetInstance().IsInGame()) {
                Application::GetInstance().ToggleChatState();
            }
        };

        // Make screen, container, and content all clickable for reliable touch-to-talk
        lv_obj_t* scr = lv_screen_active();
        if (scr) {
            lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(scr, toggle_chat_cb, LV_EVENT_CLICKED, nullptr);
        }
        if (container_) {
            lv_obj_add_flag(container_, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(container_, toggle_chat_cb, LV_EVENT_CLICKED, nullptr);
        }
        if (content_) {
            lv_obj_add_flag(content_, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(content_, toggle_chat_cb, LV_EVENT_CLICKED, nullptr);
        }

        GameManager::GetInstance().Init();
    }
};

class FreenoveESP32S3Display : public WifiBoard {
private:
    Button boot_button_;
    LcdDisplay *display_;
    i2c_master_bus_handle_t codec_i2c_bus_;
    TouchDriver touch_;
    AdcBatteryMonitor* adc_battery_monitor_;
    lv_indev_t* touch_indev_ = nullptr;

    void InitializeBatteryMonitor() {
        adc_battery_monitor_ = new AdcBatteryMonitor(ADC_UNIT_1, ADC_CHANNEL_8, 200000, 200000, GPIO_NUM_NC);
    }

    void InitializeTouch() {
        // Reset capacitive touch controller (FT6336G / CST816 on GPIO 18)
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = (1ULL << GPIO_NUM_18);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);
        gpio_set_level(GPIO_NUM_18, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(GPIO_NUM_18, 1);
        vTaskDelay(pdMS_TO_TICKS(50));

        if (!touch_.Init(codec_i2c_bus_, 0x38)) {
            ESP_LOGE(TAG, "Touch initialization failed at 0x38");
            return;
        }
        ESP_LOGI(TAG, "Touch initialized successfully at 0x38");

        g_touch_driver = &touch_;
        touch_indev_ = lv_indev_create();
        lv_indev_set_type(touch_indev_, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(touch_indev_, TouchpadReadCallback);
        lv_indev_set_disp(touch_indev_, lv_display_get_default());
        ESP_LOGI(TAG, "LVGL pointer indev registered for Touch");
    }

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = AUDIO_CODEC_I2C_NUM,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = DISPLAY_MIS0_PIN;
        buscfg.sclk_io_num = DISPLAY_SCK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto &app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
    }

    void InitializeLcdDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = DISPLAY_SPI_SCLK_HZ;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_SPI_HOST, &io_config, &panel_io));

        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));
        ESP_LOGI(TAG, "Install LCD driver ILI9341");
        esp_lcd_panel_reset(panel);

        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        display_ = new HosyondDisplay(panel_io, panel,
            DISPLAY_WIDTH, DISPLAY_HEIGHT,
            DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
            DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeTools() {
        auto& mcp = McpServer::GetInstance();
        mcp.AddTool("self.games.launch", "Launch retro arcade game or games menu. Parameter `game`: 'launcher', 'tetris', 'breakout', 'snake', 'flappy', 'invaders', 'pong', or '2048'.",
            PropertyList({
                Property("game", kPropertyTypeString, std::string("launcher"))
            }),
            [](const PropertyList& properties) -> ReturnValue {
                std::string game = properties["game"].value<std::string>();
                if (game == "tetris") {
                    GameManager::GetInstance().StartGame(GameType::kTetris);
                    return "Tetris launched";
                } else if (game == "breakout" || game == "bricks") {
                    GameManager::GetInstance().StartGame(GameType::kBreakout);
                    return "Breakout launched";
                } else if (game == "snake") {
                    GameManager::GetInstance().StartGame(GameType::kSnake);
                    return "Snake launched";
                } else if (game == "flappy" || game == "flappybird") {
                    GameManager::GetInstance().StartGame(GameType::kFlappy);
                    return "Flappy Bird launched";
                } else if (game == "invaders" || game == "spaceinvaders") {
                    GameManager::GetInstance().StartGame(GameType::kInvaders);
                    return "Space Invaders launched";
                } else if (game == "pong") {
                    GameManager::GetInstance().StartGame(GameType::kPong);
                    return "Pong launched";
                } else if (game == "2048" || game == "puzzle2048") {
                    GameManager::GetInstance().StartGame(GameType::kPuzzle2048);
                    return "2048 launched";
                } else if (game == "settings" || game == "setting" || game == "preferences") {
                    GameManager::GetInstance().ShowSettings();
                    return "Settings screen opened";
                } else {
                    GameManager::GetInstance().ShowLauncher();
                    return "Game launcher opened";
                }
            });

        mcp.AddTool("self.settings.open", "Open the system settings screen on display to adjust brightness and volume.",
            PropertyList(),
            [](const PropertyList& properties) -> ReturnValue {
                GameManager::GetInstance().ShowSettings();
                return "Settings screen opened";
            });
    }

public:
    FreenoveESP32S3Display(): boot_button_(BOOT_BUTTON_GPIO)
    {
        InitializeI2c();
        InitializeBatteryMonitor();
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeTouch();
        InitializeButtons();
        InitializeTools();
        GetBacklight()->SetBrightness(100);
    }

    virtual Led *GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, AUDIO_CODEC_I2C_NUM,
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN, AUDIO_CODEC_PA_PIN,
            AUDIO_CODEC_ES8311_ADDR, true, true);
        return &audio_codec;
    }

    virtual Display *GetDisplay() override { return display_; }

    virtual Backlight *GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual bool GetBatteryLevel(int &level, bool& charging, bool& discharging) override {
        charging = adc_battery_monitor_->IsCharging();
        discharging = adc_battery_monitor_->IsDischarging();
        level = adc_battery_monitor_->GetBatteryLevel();
        return true;
    }
};

DECLARE_BOARD(FreenoveESP32S3Display);
