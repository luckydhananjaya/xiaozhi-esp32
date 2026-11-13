# Witekno Kawan AI K1 (2-mic)

Phase-1 board support for the Witekno Kawan AI K1, recovered from the closed
factory fork `witekno-kawan-ai-k1-2mic` (78/xiaozhi-esp32 v2.5.0).

- Chip: ESP32-S3
- Flash: 16 MB (project default `partitions/v2/16m.csv`)
- PSRAM: 8 MB, octal, from the ESP32-S3 sdkconfig defaults
- Audio: ES8311 + ES7210 (`BoxAudioCodec`), 24 kHz, playback reference on
- Display: SPI NV3023, 284x240, backlight on GPIO13 at 25 kHz

The panel driver is the existing project dependency `78/esp_lcd_nv3023` ==1.0.1.
No ST7789 fallback is compiled in.

## Build

```sh
python scripts/build.py witekno-kawan-ai-k1 --language en-US
```

The English artifact workflow is `.github/workflows/build-witekno-kawan-ai-k1.yml`.
It uploads `build/merged-binary.bin`.

## Flash

`merged-binary.bin` is a full image (bootloader, partition table, and app).
Write it at offset `0x0`:

```sh
esptool.py --chip esp32s3 -b 460800 --before default_reset --after hard_reset \
  write_flash --flash_mode qio --flash_freq 80m --flash_size 16MB \
  0x0 merged-binary.bin
```

Flash mode and frequency follow `sdkconfig.defaults.esp32s3` (`QIO`, 80 MHz).

## Still open

- GPIO39 / GPIO40 button roles and polarity are guessed (volume up / volume down, active low).
- Battery percentage is disabled. ADC1 channel 6 is GPIO7, the I2S data-in pin.
  `WITEKNO_KAWAN_AI_K1_BATTERY_ADC` is the reserved switch and does not claim GPIO7.
- Charge detect is GPIO47, input with pull-up. Active level is unknown, and
  `GetBatteryLevel()` is not implemented.
- NV3023 uses the component's default init sequence. A factory command list was
  not recovered. Color order is BGR until checked on hardware.
- No touch controller was recovered, so standby wakes from buttons and voice.
- The settings screen added for the Hosyond board is still laid out at 320x240.
