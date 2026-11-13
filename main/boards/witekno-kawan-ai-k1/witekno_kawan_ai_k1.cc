#include "application.h"
#include "assets/lang_config.h"
#include "button.h"
#include "codecs/box_audio_codec.h"
#include "config.h"
#include "display/lcd_display.h"
#include "games/game_input.h"
#include "games/game_manager.h"
#include "games/screen_sleep_manager.h"
#include "games/snake.h"
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

#if CONFIG_BT_ENABLED
#include <esp_bt.h>
#endif

#define TAG "WiteknoKawanAiK1"

// Stock 78/esp_lcd_nv3023 init clocks a 16-byte gamma burst straight out of
// flash and then command 0x2C (RAMWR) with no pixels. On this panel the SPI
// master never finishes that transfer: polling waits forever, the task
// watchdog is not set to panic, and nothing after the "3Ah overwritten"
// warning is printed. Bytes match the component's 0.85" sequence, sent one
// register at a time from internal RAM, with no RAMWR. 0x3A stays 0x05,
// which is the value the phase-1 image brought the panel up with.
namespace {
uint8_t kA5[] = {0xa5};
uint8_t k09[] = {0x09};
uint8_t k05[] = {0x05};
uint8_t k00[] = {0x00};
uint8_t k03[] = {0x03};
uint8_t k04[] = {0x04};
uint8_t k0f[] = {0x0f};
uint8_t k34[] = {0x34};
uint8_t k30[] = {0x30};
uint8_t k16[] = {0x16};
uint8_t k0a[] = {0x0a};
uint8_t k27[] = {0x27};
uint8_t k63[] = {0x63};
uint8_t k81[] = {0x81};
uint8_t k10[] = {0x10};
uint8_t k01[] = {0x01};
uint8_t kGammaPos[16] = {0x09, 0x1d, 0x14, 0x0a, 0x11, 0x16, 0x38, 0x0b,
                         0x08, 0x3e, 0x07, 0x0d, 0x16, 0x0f, 0x14, 0x05};
uint8_t kGammaNeg[16] = {0x04, 0x28, 0x0c, 0x11, 0x0b, 0x23, 0x45, 0x07,
                         0x0a, 0x3b, 0x0d, 0x18, 0x14, 0x0f, 0x19, 0x08};

const nv3023_lcd_init_cmd_t kNv3023Init[] = {
    {0xff, kA5, 1, 0},
    {0x3e, k09, 1, 0},
    {0x3a, k05, 1, 0},
    {0x82, k00, 1, 0},
    {0x98, k00, 1, 0},
    {0x63, k0f, 1, 0},
    {0x64, k0f, 1, 0},
    {0xb4, k34, 1, 0},
    {0xb5, k30, 1, 0},
    {0x83, k03, 1, 0},
    {0x86, k04, 1, 0},
    {0x87, k16, 1, 0},
    {0x88, k0a, 1, 0},
    {0x89, k27, 1, 0},
    {0x93, k63, 1, 0},
    {0x96, k81, 1, 0},
    {0xc3, k10, 1, 0},
    {0xe6, k00, 1, 0},
    {0x99, k01, 1, 0},
    {0x70, &kGammaPos[0], 1, 0},
    {0x71, &kGammaPos[1], 1, 0},
    {0x72, &kGammaPos[2], 1, 0},
    {0x73, &kGammaPos[3], 1, 0},
    {0x74, &kGammaPos[4], 1, 0},
    {0x75, &kGammaPos[5], 1, 0},
    {0x76, &kGammaPos[6], 1, 0},
    {0x77, &kGammaPos[7], 1, 0},
    {0x78, &kGammaPos[8], 1, 0},
    {0x79, &kGammaPos[9], 1, 0},
    {0x7a, &kGammaPos[10], 1, 0},
    {0x7b, &kGammaPos[11], 1, 0},
    {0x7c, &kGammaPos[12], 1, 0},
    {0x7d, &kGammaPos[13], 1, 0},
    {0x7e, &kGammaPos[14], 1, 0},
    {0x7f, &kGammaPos[15], 1, 0},
    {0xa0, &kGammaNeg[0], 1, 0},
    {0xa1, &kGammaNeg[1], 1, 0},
    {0xa2, &kGammaNeg[2], 1, 0},
    {0xa3, &kGammaNeg[3], 1, 0},
    {0xa4, &kGammaNeg[4], 1, 0},
    {0xa5, &kGammaNeg[5], 1, 0},
    {0xa6, &kGammaNeg[6], 1, 0},
    {0xa7, &kGammaNeg[7], 1, 0},
    {0xa8, &kGammaNeg[8], 1, 0},
    {0xa9, &kGammaNeg[9], 1, 0},
    {0xaa, &kGammaNeg[10], 1, 0},
    {0xab, &kGammaNeg[11], 1, 0},
    {0xac, &kGammaNeg[12], 1, 0},
    {0xad, &kGammaNeg[13], 1, 0},
    {0xae, &kGammaNeg[14], 1, 0},
    {0xaf, &kGammaNeg[15], 1, 0},
    {0xff, k00, 1, 0},
    {0x11, k00, 0, 120},
    {0x29, k00, 0, 10},
};
}  // namespace

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
        // LVGL refreshes a 20-row stripe. A full-frame max (284*240*2) reserves
        // a large internal DMA descriptor chain before the BLE heap is freed.
        buscfg.max_transfer_sz = DISPLAY_WIDTH * 20 * sizeof(uint16_t);
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
        nv3023_vendor_config_t vendor_config = {
            .init_cmds = kNv3023Init,
            .init_cmds_size = sizeof(kNv3023Init) / sizeof(kNv3023Init[0]),
        };
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config = &vendor_config;
        ESP_ERROR_CHECK(esp_lcd_new_panel_nv3023(panel_io, &panel_config, &panel));

        // RST is not connected, so the driver issues a software reset.
        esp_lcd_panel_reset(panel);
        vTaskDelay(pdMS_TO_TICKS(120));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_LOGI(TAG, "NV3023 init done");
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_set_gap(panel, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_disp_on_off(panel, true);
        // Light the panel before the LVGL object's row fill. A stalled SPI
        // transfer used to die here with the backlight still off.
        GetBacklight()->RestoreBrightness();

        // Gap is applied on the panel. Passing it again would shift LVGL twice.
        display_ = new SpiLcdDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT, 0, 0,
                                     DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
        ESP_LOGI(TAG, "LVGL display ready");
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

    bool WakeOrConsume() {
        auto& sleep = ScreenSleepManager::GetInstance();
        if (sleep.IsSleeping()) {
            sleep.WakeUp();
            return true;
        }
        sleep.WakeUp();
        return false;
    }

    void LeaveOverlays() {
        if (KawanMenu::IsOpen()) {
            KawanMenu::Close();
        }
        if (KawanVideo::Playing()) {
            KawanVideo::Stop();
        }
        auto& games = GameManager::GetInstance();
        if (games.IsInGame()) {
            games.ExitCurrentGame();
        }
        if (games.IsInLauncher()) {
            games.ExitToChatbot();
        }
        KawanUiClearFocus();
    }

    void InitializeButtons() {
        // GPIO 40 single = next, long = select. GPIO 39 single = previous, long = back.
        // BOOT single = talk/wake. BOOT double = menu. Games and BLE stay closed until opened.
        boot_button_.OnClick([this]() {
            if (WakeOrConsume()) {
                return;
            }
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            LeaveOverlays();
            app.ToggleChatState();
        });
        boot_button_.OnDoubleClick([this]() {
            if (WakeOrConsume()) {
                return;
            }
            LeaveOverlays();
            KawanMenu::Open();
        });

        volume_down_button_.OnClick([this]() {
            if (WakeOrConsume()) {
                return;
            }
            if (KawanMenu::IsOpen()) {
                KawanMenu::OnDown();
                return;
            }
            if (KawanUiUsesButtons()) {
                KawanUiMove(1);
                return;
            }
            if (!GameManager::GetInstance().IsInGame()) {
                return;
            }
            const GameType game = GameManager::GetInstance().CurrentGame();
            if (game == GameType::kSnake && SnakeGame::instance_ != nullptr) {
                SnakeGame::instance_->Turn(1);
            } else if (game == GameType::kFlappy) {
                ApplyGamePadAction(GamePadAction::kAction);
            } else if (game == GameType::kPong) {
                ApplyGamePadAction(GamePadAction::kDown);
            } else {
                ApplyGamePadAction(GamePadAction::kRight);
            }
        });
        volume_down_button_.OnLongPress([this]() {
            if (WakeOrConsume()) {
                return;
            }
            if (KawanMenu::IsOpen()) {
                KawanMenu::OnConfirm();
                return;
            }
            if (KawanVideo::Playing()) {
                KawanVideo::Stop();
                return;
            }
            if (KawanUiUsesButtons()) {
                KawanUiActivate();
                return;
            }
            if (!GameManager::GetInstance().IsInGame()) {
                return;
            }
            const GameType game = GameManager::GetInstance().CurrentGame();
            if (game == GameType::kTetris || game == GameType::kInvaders ||
                game == GameType::kFlappy) {
                ApplyGamePadAction(GamePadAction::kAction);
            } else if (game == GameType::kSnake || game == GameType::kPuzzle2048 ||
                       game == GameType::kGameBoy) {
                ApplyGamePadAction(GamePadAction::kUp);
            } else {
                ApplyGamePadAction(GamePadAction::kDrop);
            }
        });

        volume_up_button_.OnClick([this]() {
            if (WakeOrConsume()) {
                return;
            }
            if (KawanMenu::IsOpen()) {
                KawanMenu::OnUp();
                return;
            }
            if (KawanUiUsesButtons()) {
                KawanUiMove(-1);
                return;
            }
            if (!GameManager::GetInstance().IsInGame()) {
                return;
            }
            const GameType game = GameManager::GetInstance().CurrentGame();
            if (game == GameType::kSnake && SnakeGame::instance_ != nullptr) {
                SnakeGame::instance_->Turn(-1);
            } else if (game == GameType::kFlappy) {
                ApplyGamePadAction(GamePadAction::kAction);
            } else if (game == GameType::kPong) {
                ApplyGamePadAction(GamePadAction::kUp);
            } else {
                ApplyGamePadAction(GamePadAction::kLeft);
            }
        });
        volume_up_button_.OnLongPress([this]() {
            if (WakeOrConsume()) {
                return;
            }
            if (KawanMenu::IsOpen()) {
                KawanMenu::OnBack();
                return;
            }
            if (KawanVideo::Playing()) {
                KawanVideo::Stop();
                return;
            }
            auto& games = GameManager::GetInstance();
            if (games.IsInLauncher()) {
                games.ExitToChatbot();
                KawanUiClearFocus();
                return;
            }
            if (games.IsInGame()) {
                games.ExitCurrentGame();
                if (games.IsInLauncher()) {
                    games.ExitToChatbot();
                }
                KawanUiClearFocus();
            }
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
        mcp.AddTool(
            "self.games.launch",
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
        mcp.AddTool("self.settings.open", "Open the system settings screen.", PropertyList(),
                    [](const PropertyList& properties) -> ReturnValue {
                        GameManager::GetInstance().ShowSettings();
                        return "Settings screen opened";
                    });
        mcp.AddTool("self.video.start", "Start the MJPEG stream. Optional url is saved in NVS.",
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
        : boot_button_(BOOT_BUTTON_GPIO, false, 700),
          volume_up_button_(VOLUME_UP_BUTTON_GPIO, false, 700),
          volume_down_button_(VOLUME_DOWN_BUTTON_GPIO, false, 700) {
#if CONFIG_BT_ENABLED
        // Give the classic-BT controller block back before WiFi starts. The BLE
        // host itself stays uninitialised until the pairing screen is opened.
        esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
#endif
        InitializeI2c();
        InitializeSpi();
        InitializeNv3023Display();
        InitializeChargeDetect();
        InitializeButtons();
        InitializeTools();
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

    bool HasTouchScreen() const override { return false; }
};

DECLARE_BOARD(WiteknoKawanAiK1);
