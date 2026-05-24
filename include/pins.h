#pragma once
// GPIO pin assignments for the ESP32-WROOM-32U.

// TFT SPI (ILI9341)
#define TFT_MOSI  23
#define TFT_MISO  19
#define TFT_CLK   18
#define TFT_CS     5
#define TFT_DC     2
#define TFT_RST    4

// SD Card
#define SD_CS     15

// I2S DAC (GY-PCM5102)
#define I2S_BCLK  26
#define I2S_LRCK  25
#define I2S_DOUT  22

// I2C (ADS1115 x2)
#define I2C_SDA   21
#define I2C_SCL   17

// Buttons
#define BTN_PREV  32
#define BTN_NEXT  33
#define BTN_ONOFF 13
#define BTN_MUTE  14

// Output selector switch (single pin): HIGH = line out, LOW = repro
#define SW_JACK   16
