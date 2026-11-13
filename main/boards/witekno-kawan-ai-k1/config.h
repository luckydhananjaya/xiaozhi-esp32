#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE 24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// Playback reference is mixed into the ES7210 capture path for on-device AEC.
#define AUDIO_INPUT_REFERENCE true

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_5
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_15
#define AUDIO_I2S_GPIO_WS GPIO_NUM_16
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_6
#define AUDIO_I2S_GPIO_DIN GPIO_NUM_7

#define AUDIO_CODEC_PA_PIN GPIO_NUM_4
#define AUDIO_CODEC_I2C_SDA_PIN GPIO_NUM_12
#define AUDIO_CODEC_I2C_SCL_PIN GPIO_NUM_11
// BoxAudioCodec talks to this bus as I2C_NUM_1. Internal pull-ups are enabled.
#define AUDIO_CODEC_I2C_NUM I2C_NUM_1
// 8-bit addresses, the form upstream boards pass into BoxAudioCodec
// (ES8311_CODEC_DEFAULT_ADDR / ES7210_CODEC_DEFAULT_ADDR). The codec driver
// shifts them to 7-bit itself.
#define AUDIO_CODEC_ES8311_ADDR 0x30
#define AUDIO_CODEC_ES7210_ADDR 0x80

#define BOOT_BUTTON_GPIO GPIO_NUM_0
// TODO: Factory firmware roles for GPIO39 and GPIO40 were not recovered.
// GPIO0 is the boot/chat button. 39 and 40 are wired as volume up/down.
#define VOLUME_UP_BUTTON_GPIO GPIO_NUM_39
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_40

#define LCD_SPI_HOST SPI3_HOST
#define DISPLAY_SCK_PIN GPIO_NUM_9
#define DISPLAY_MOSI_PIN GPIO_NUM_10
#define DISPLAY_CS_PIN GPIO_NUM_14
#define DISPLAY_DC_PIN GPIO_NUM_8
#define DISPLAY_RST_PIN GPIO_NUM_NC
#define DISPLAY_MISO_PIN GPIO_NUM_NC
#define DISPLAY_SPI_MODE 0
#define DISPLAY_SPI_SCLK_HZ (40 * 1000 * 1000)

// 284x240 window into the panel GRAM. offset_x 36 matches the same geometry
// used by espressif/esp-sensairshuttle and is applied with esp_lcd_panel_set_gap.
#define DISPLAY_WIDTH 284
#define DISPLAY_HEIGHT 240
#define DISPLAY_OFFSET_X 36
#define DISPLAY_OFFSET_Y 0
#define DISPLAY_SWAP_XY true
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y true
#define DISPLAY_INVERT_COLOR true
// TODO: Confirm on hardware. Other NV3023 boards in this tree use BGR.
#define DISPLAY_RGB_ORDER LCD_RGB_ELEMENT_ORDER_BGR

#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_13
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_BACKLIGHT_FREQ_HZ 25000

// Active level is unknown. The pin is an input with a pull-up; the gauge is off.
#define CHARGE_DETECT_PIN GPIO_NUM_47

// TODO: Do not assign the battery ADC. ADC1 channel 6 is GPIO7, which is
// I2S DIN. The microphone keeps that pin. See Kconfig
// WITEKNO_KAWAN_AI_K1_BATTERY_ADC.

#endif  // _BOARD_CONFIG_H_
