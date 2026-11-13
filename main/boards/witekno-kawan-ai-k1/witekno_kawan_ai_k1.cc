#include "application.h"
#include "assets/lang_config.h"
#include "button.h"
#include "codecs/box_audio_codec.h"
#include "config.h"
#include "display/lcd_display.h"
#include "games/game_input.h"
#include "games/game_manager.h"
#include "games/screen_sleep_manager.h"
#include "kawan_hid.h"
#include "kawan_menu.h"
#include "kawan_video.h"
#include "mcp_server.h"
#include "wifi_board.h"

#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_lcd_nv3023.h>
#include <esp_lcd_panel_ops.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "WiteknoKawanAiK1"

class KawanDisplay : public SpiLcdDisplay {
public:
    using SpiLcdDisplay::SpiLcdDisplay;

    void SetupUI() override {
        SpiLcdDisplay::SetupUI();
        GameManager::GetInstance().Init();
    }
};

// ESP-Box style BoxAudioCodec (ES8311 + ES7210) plus an SPI LCD.
// Panel geometry follows espressif/esp-sensairshuttle (284x240, gap x=36).
// The controller here is NV3023 via 78/esp_lcd_nv3023, which is already in
// main/idf_component.yml. TODO: factory init commands were not recovered, so
// this uses the component's default sequence.
class WiteknoKawanAiK1 : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    Display* display_;

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = AUDIO_CODEC_I2C_NUM,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags =
                {
                    .enable_internal_pullup = 1,
                },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = DISPLAY_MISO_PIN;
        buscfg.sclk_io_num = DISPLAY_SCK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeNv3023Display() {
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

        ESP_LOGD(TAG, "Install NV3023 LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_nv3023(panel_io, &panel_config, &panel));

        // RST is not connected, so the driver issues a software reset.
        esp_lcd_panel_reset(panel);
        vTaskDelay(pdMS_TO_TICKS(120));
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_set_gap(panel, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_disp_on_off(panel, true);

        // Gap is applied on the panel. Passing it again would shift LVGL twice.
        display_ = new KawanDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT, 0, 0,
                                    DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeChargeDetect() {
        gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = 1ULL << CHARGE_DETECT_PIN;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.intr_type = GPIO_INTR_DISABLE;
        ESP_ERROR_CHECK(gpio_config(&io_conf));
        ESP_LOGI(TAG, "Charge detect GPIO%d level=%d", CHARGE_DETECT_PIN,
                 gpio_get_level(CHARGE_DETECT_PIN));

#if CONFIG_WITEKNO_KAWAN_AI_K1_BATTERY_ADC
        // TODO: ADC1_CHANNEL_6 is GPIO7, the I2S DIN pin. Leave the mic connected
        // and keep the percentage gauge disabled until a free sense pin is known.
        ESP_LOGW(TAG, "Battery ADC is selected but GPIO7 is I2S DIN; gauge stays off");
#endif
    }

    bool RouteArcade(GamePadAction action) {
        auto& games = GameManager::GetInstance();
        if (KawanUiUsesButtons()) {
            if (action == GamePadAction::kLeft || action == GamePadAction::kUp) {
                KawanUiMove(-1);
            } else if (action == GamePadAction::kRight || action == GamePadAction::kDown) {
                KawanUiMove(1);
            } else if (action == GamePadAction::kAction) {
                KawanUiActivate();
            } else if (action == GamePadAction::kDrop) {
                games.ExitToChatbot();
                KawanUiClearFocus();
            }
            return true;
        }
        if (games.IsInGame()) {
            if (action == GamePadAction::kDrop) {
                games.ExitCurrentGame();
                KawanUiClearFocus();
            } else {
                ApplyGamePadAction(action);
            }
            return true;
        }
        return false;
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            ScreenSleepManager::GetInstance().WakeUp();
            if (KawanMenu::IsOpen()) {
                KawanMenu::OnConfirm();
                return;
            }
            if (RouteArcade(GamePadAction::kAction)) {
                return;
            }
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
        boot_button_.OnDoubleClick([this]() {
            ScreenSleepManager::GetInstance().WakeUp();
            if (KawanMenu::IsOpen()) {
                KawanMenu::OnSave();
                return;
            }
            if (GameManager::GetInstance().IsInGame() || GameManager::GetInstance().IsInLauncher()) {
                return;
            }
            KawanMenu::Open();
        });
        boot_button_.OnLongPress([this]() {
            ScreenSleepManager::GetInstance().WakeUp();
            if (KawanMenu::IsOpen()) {
                KawanMenu::OnBack();
                return;
            }
            if (KawanVideo::Playing()) {
                KawanVideo::Stop();
                return;
            }
            RouteArcade(GamePadAction::kDrop);
        });

        volume_up_button_.OnClick([this]() {
            ScreenSleepManager::GetInstance().WakeUp();
            if (KawanMenu::IsOpen()) {
                KawanMenu::OnUp();
                return;
            }
            if (KawanUiUsesButtons()) {
                KawanUiMove(-1);
                return;
            }
            if (GameManager::GetInstance().IsInGame()) {
                const GameType game = GameManager::GetInstance().CurrentGame();
                const bool vertical = game == GameType::kSnake || game == GameType::kPong ||
                                      game == GameType::kPuzzle2048 || game == GameType::kGameBoy;
                ApplyGamePadAction(vertical ? GamePadAction::kUp : GamePadAction::kRight);
                return;
            }
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });
        volume_down_button_.OnClick([this]() {
            ScreenSleepManager::GetInstance().WakeUp();
            if (KawanMenu::IsOpen()) {
                KawanMenu::OnDown();
                return;
            }
            if (KawanUiUsesButtons()) {
                KawanUiMove(1);
                return;
            }
            if (GameManager::GetInstance().IsInGame()) {
                const GameType game = GameManager::GetInstance().CurrentGame();
                const bool vertical = game == GameType::kSnake || game == GameType::kPong ||
                                      game == GameType::kPuzzle2048 || game == GameType::kGameBoy;
                ApplyGamePadAction(vertical ? GamePadAction::kDown : GamePadAction::kLeft);
                return;
            }
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });
    }

    void InitializeTools() {
        auto& mcp = McpServer::GetInstance();
        mcp.AddTool("self.screen.sleep",
                    "Turn off screen backlight to enter low-power standby mode.", PropertyList(),
                    [](const PropertyList& properties) -> ReturnValue {
                        ScreenSleepManager::GetInstance().Sleep();
                        return "Screen entered standby sleep mode";
                    });
        mcp.AddTool("self.screen.wake", "Turn on / wake screen from sleep mode.", PropertyList(),
                    [](const PropertyList& properties) -> ReturnValue {
                        ScreenSleepManager::GetInstance().WakeUp();
                        return "Screen woken up";
                    });
        mcp.AddTool("self.games.launch",
                    "Launch retro arcade game or games menu. Parameter `game`: 'launcher', 'tetris', "
                    "'breakout', 'snake', 'flappy', 'invaders', 'pong', '2048', or 'gameboy'.",
                    PropertyList({Property("game", kPropertyTypeString, std::string("launcher"))}),
                    [](const PropertyList& properties) -> ReturnValue {
                        std::string game = properties["game"].value<std::string>();
                        if (game == "tetris") {
                            GameManager::GetInstance().StartGame(GameType::kTetris);
                        } else if (game == "breakout" || game == "bricks") {
                            GameManager::GetInstance().StartGame(GameType::kBreakout);
                        } else if (game == "snake") {
                            GameManager::GetInstance().StartGame(GameType::kSnake);
                        } else if (game == "flappy") {
                            GameManager::GetInstance().StartGame(GameType::kFlappy);
                        } else if (game == "invaders") {
                            GameManager::GetInstance().StartGame(GameType::kInvaders);
                        } else if (game == "pong") {
                            GameManager::GetInstance().StartGame(GameType::kPong);
                        } else if (game == "2048") {
                            GameManager::GetInstance().StartGame(GameType::kPuzzle2048);
                        } else if (game == "gameboy" || game == "gb") {
                            GameManager::GetInstance().StartGame(GameType::kGameBoy);
                        } else if (game == "settings") {
                            GameManager::GetInstance().ShowSettings();
                        } else {
                            GameManager::GetInstance().ShowLauncher();
                        }
                        return "Game screen opened";
                    });
        mcp.AddTool("self.settings.open",
                    "Open the system settings screen.", PropertyList(),
                    [](const PropertyList& properties) -> ReturnValue {
                        GameManager::GetInstance().ShowSettings();
                        return "Settings screen opened";
                    });
        mcp.AddTool("self.video.start",
                    "Start the MJPEG stream. Optional url is saved in NVS.",
                    PropertyList({Property("url", kPropertyTypeString, std::string(""))}),
                    [](const PropertyList& properties) -> ReturnValue {
                        KawanVideo::Start(properties["url"].value<std::string>());
                        return KawanVideo::Playing() ? "Video started" : "Video URL missing";
                    });
        mcp.AddTool("self.video.stop", "Stop the MJPEG stream.", PropertyList(),
                    [](const PropertyList& properties) -> ReturnValue {
                        KawanVideo::Stop();
                        return "Video stopped";
                    });
        mcp.AddTool("self.screen.set_timeout",
                    "Set screen sleep timeout in seconds (0 for never, 15, 30, 60).",
                    PropertyList({Property("seconds", kPropertyTypeInteger, 30)}),
                    [](const PropertyList& properties) -> ReturnValue {
                        int sec = properties["seconds"].value<int>();
                        ScreenSleepManager::GetInstance().SetTimeout(sec);
                        return "Screen sleep timeout updated";
                    });
    }

public:
    WiteknoKawanAiK1()
        : boot_button_(BOOT_BUTTON_GPIO),
          volume_up_button_(VOLUME_UP_BUTTON_GPIO),
          volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
        InitializeI2c();
        InitializeSpi();
        InitializeNv3023Display();
        InitializeChargeDetect();
        InitializeButtons();
        InitializeTools();
        GetBacklight()->RestoreBrightness();
        // Same standby path as the Hosyond board: backlight off on idle, wake from
        // these buttons and from Application::SetDeviceState() on voice activity.
        // This board has no touch controller, so there is no touch wake.
        ScreenSleepManager::GetInstance().Init();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(
            i2c_bus_, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR, AUDIO_CODEC_ES7210_ADDR,
            AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override { return display_; }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT,
                                      DISPLAY_BACKLIGHT_FREQ_HZ);
        return &backlight;
    }
};

DECLARE_BOARD(WiteknoKawanAiK1);
