#pragma once
// ILI9341 display driver: 320×240 landscape UI, splash screen, dark/light palette.

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include "pins.h"
#include "config.h"

struct DisplayState {
    uint8_t     waveformIndex;  // WAVE_* (0-4)
    const char* pageName;       // short string, centered in top bar
    bool        fxActive[6];    // LPF, DLY, CHR, DST, BIT, LFO
    float       knobValues[6];  // 0.0 – 1.0
    const char* knobLabels[6];  // short uppercase strings
    uint8_t     activePage;     // 0=PREV 1=ON 2=MUTE 3=NEXT
};

class Display {
public:
    Display();
    ~Display();

    void begin(bool dark = true);
    void setMode(bool dark);   // toggle palette + full redraw
    void drawStatic();
    void showSplash();
    void updateSplashProgress(int step, int total);
    void updateOscilloscope(float* samples, int count);
    void updateValues(const DisplayState& state);
    void update() {}        // stub — main.cpp compatibility
    void showParam(uint8_t pot, float value) {}

private:
    Adafruit_ILI9341* tft;

    // ---- Layout constants (landscape 320×240) ----
    static constexpr int16_t W           = 320;
    static constexpr int16_t H           = 240;
    static constexpr int16_t Y_SCOPE_TOP = 23;
    static constexpr int16_t Y_SCOPE_BOT = 89;
    static constexpr int16_t Y_FX_TOP    = 89;
    static constexpr int16_t Y_FX_BOT    = 109;
    static constexpr int16_t Y_KNOB_TOP  = 109;
    static constexpr int16_t Y_KNOB_BOT  = 220;
    static constexpr int16_t Y_BTN_TOP   = 220;
    static constexpr int16_t KNOB_COLS   = 3;
    static constexpr int16_t KNOB_ROWS   = 2;

    void drawTopBar(uint8_t waveIdx, const char* page);
    void drawFxBadges(const bool* active);
    void drawKnobGrid(const float* values, const char* const* labels);
    void drawKnobCell(int16_t x, int16_t y, int16_t w, int16_t h,
                      const char* label, float value);
    void drawBottomBar(uint8_t activePage);
    void restoreDividers();

    // ---- Colour palette (swapped by setMode) ----
    bool     _dark = true;
    uint16_t _fg   = 0xFFFF;  // ILI9341_WHITE
    uint16_t _bg   = 0x0000;  // ILI9341_BLACK

    static const char* const WAVE_NAMES[5];
    static const char* const FX_NAMES[6];
    static const char* const BTN_NAMES[4];
};
