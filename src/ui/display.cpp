// Display rendering: static chrome, oscilloscope, knob grid, FX badges, bottom bar.
#include "display.h"

const char* const Display::WAVE_NAMES[5] = { "SINE", "SQR", "SAW", "TRI", "NOIS" };
const char* const Display::FX_NAMES[6]   = { "LPF", "DLY", "CHR", "DST", "BIT", "LFO" };
const char* const Display::BTN_NAMES[4]  = { "PREV", "ON", "MUTE", "NEXT" };

// ---------------------------------------------------------------------------
// Local helper: fill a bounding box then draw text centred inside it.
// ---------------------------------------------------------------------------
static void filledCenteredText(Adafruit_ILI9341* tft,
                               const char* str,
                               int16_t bx, int16_t by, int16_t bw, int16_t bh,
                               uint8_t sz, uint16_t fg, uint16_t bg) {
    tft->fillRect(bx, by, bw, bh, bg);
    int16_t tw = (int16_t)(strlen(str) * 6 * sz);
    int16_t th = (int16_t)(8 * sz);
    tft->setTextColor(fg, bg);
    tft->setTextSize(sz);
    tft->setCursor(bx + (bw - tw) / 2, by + (bh - th) / 2);
    tft->print(str);
}

// ---------------------------------------------------------------------------

Display::Display()
    : tft(new Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST))
{}

Display::~Display() {
    delete tft;
}

void Display::begin(bool dark) {
    _dark = dark;
    _fg   = dark ? ILI9341_WHITE : ILI9341_BLACK;
    _bg   = dark ? ILI9341_BLACK : ILI9341_WHITE;
    tft->begin();
    tft->setRotation(1);  // landscape: 320 wide × 240 tall
    tft->fillScreen(_bg);
}

// ---------------------------------------------------------------------------
// Splash screen — shown during boot init
// ---------------------------------------------------------------------------
void Display::showSplash() {
    tft->fillScreen(_bg);

    // "SYNTH" — size 4, centered (each char ~24 px wide × 32 px tall)
    const char* title = "SYNTH";
    int16_t tw = (int16_t)(strlen(title) * 6 * 4);
    tft->setTextColor(_fg, _bg);
    tft->setTextSize(4);
    tft->setCursor((W - tw) / 2, 80);
    tft->print(title);

    // "by Hrasek" — size 2, centered
    const char* sub = "by Hrasek";
    int16_t sw = (int16_t)(strlen(sub) * 6 * 2);
    tft->setTextSize(2);
    tft->setCursor((W - sw) / 2, 125);
    tft->print(sub);

    // Progress bar outline: x=60, y=170, w=200, h=6
    tft->drawRect(60, 170, 200, 6, _fg);
}

void Display::updateSplashProgress(int step, int total) {
    if (total <= 0) return;
    // Fill interior: x=61, y=171, h=4; width proportional to step/total
    int16_t fillW = (int16_t)((int32_t)step * 198 / total);
    if (fillW > 198) fillW = 198;
    if (fillW > 0)
        tft->fillRect(61, 171, fillW, 4, _fg);
}

// ---------------------------------------------------------------------------
// Mode switch — swap palette, fill screen, redraw static chrome
// ---------------------------------------------------------------------------
void Display::setMode(bool dark) {
    _dark = dark;
    _fg   = dark ? ILI9341_WHITE : ILI9341_BLACK;
    _bg   = dark ? ILI9341_BLACK : ILI9341_WHITE;
    tft->fillScreen(_bg);
    drawStatic();
}

// ---------------------------------------------------------------------------
// Horizontal section dividers — redrawn after any fillRect that may erase them
// ---------------------------------------------------------------------------
void Display::restoreDividers() {
    tft->drawFastHLine(0, Y_SCOPE_TOP - 1, W, _fg);
    tft->drawFastHLine(0, Y_SCOPE_BOT,     W, _fg);
    tft->drawFastHLine(0, Y_FX_BOT,        W, _fg);
    tft->drawFastHLine(0, Y_KNOB_BOT,      W, _fg);
}

// ---------------------------------------------------------------------------
// Static chrome — call once after begin() / setMode()
// ---------------------------------------------------------------------------
void Display::drawStatic() {
    restoreDividers();

    // FX row: 5 vertical separators
    const int16_t badgeW = W / 6;
    for (int i = 1; i < 6; ++i)
        tft->drawFastVLine(i * badgeW, Y_FX_TOP, Y_FX_BOT - Y_FX_TOP, _fg);

    // Knob grid: 2 vertical + 1 horizontal
    const int16_t kw = W / KNOB_COLS;
    const int16_t kh = (Y_KNOB_BOT - Y_KNOB_TOP) / KNOB_ROWS;
    for (int c = 1; c < KNOB_COLS; ++c)
        tft->drawFastVLine(c * kw, Y_KNOB_TOP, Y_KNOB_BOT - Y_KNOB_TOP, _fg);
    tft->drawFastHLine(0, Y_KNOB_TOP + kh, W, _fg);

    // Bottom bar: 3 vertical separators
    const int16_t btnW = W / 4;
    for (int i = 1; i < 4; ++i)
        tft->drawFastVLine(i * btnW, Y_BTN_TOP, H - Y_BTN_TOP, _fg);
}

// ---------------------------------------------------------------------------
// Oscilloscope — erase and redraw every frame
// ---------------------------------------------------------------------------
void Display::updateOscilloscope(float* samples, int count) {
    tft->fillRect(0, Y_SCOPE_TOP, W, Y_SCOPE_BOT - Y_SCOPE_TOP, _bg);

    if (!samples || count < 2) {
        restoreDividers();
        return;
    }

    const int16_t midY = Y_SCOPE_TOP + (Y_SCOPE_BOT - Y_SCOPE_TOP) / 2;
    const float   amp  = (float)((Y_SCOPE_BOT - Y_SCOPE_TOP) / 2 - 1);

    for (int i = 0; i < count - 1; ++i) {
        int16_t x0 = (int16_t)((float)i       / (float)(count - 1) * (float)(W - 1));
        int16_t x1 = (int16_t)((float)(i + 1) / (float)(count - 1) * (float)(W - 1));

        float s0 = samples[i]     < -1.0f ? -1.0f : (samples[i]     > 1.0f ? 1.0f : samples[i]);
        float s1 = samples[i + 1] < -1.0f ? -1.0f : (samples[i + 1] > 1.0f ? 1.0f : samples[i + 1]);

        int16_t y0 = midY - (int16_t)(s0 * amp);
        int16_t y1 = midY - (int16_t)(s1 * amp);

        tft->drawLine(x0, y0, x1, y1, _fg);
    }

    restoreDividers();
}

// ---------------------------------------------------------------------------
// Top bar (y 0–22)
// ---------------------------------------------------------------------------
void Display::drawTopBar(uint8_t waveIdx, const char* page) {
    const int16_t barH = Y_SCOPE_TOP;  // 23px

    tft->fillRect(0, 0, W, barH, _bg);

    // Wave name: inverted pill (foreground fill, background text)
    const char* wn    = (waveIdx < 5) ? WAVE_NAMES[waveIdx] : "----";
    int16_t     nameW = (int16_t)(strlen(wn) * 6) + 8;
    tft->fillRect(2, 2, nameW, barH - 4, _fg);
    tft->setTextColor(_bg, _fg);
    tft->setTextSize(1);
    tft->setCursor(6, (barH - 8) / 2);
    tft->print(wn);

    // Page name: normal (foreground text, background fill)
    if (page && *page) {
        int16_t tw = (int16_t)(strlen(page) * 6);
        tft->setTextColor(_fg, _bg);
        tft->setTextSize(1);
        tft->setCursor((W - tw) / 2, (barH - 8) / 2);
        tft->print(page);
    }

    // Battery placeholder: outline rect + nub, right-aligned
    tft->drawRect(W - 24, 3, 18, barH - 6, _fg);
    tft->drawFastVLine(W - 6, 7, barH - 14, _fg);
}

// ---------------------------------------------------------------------------
// FX badges row (y 89–109)
// ---------------------------------------------------------------------------
void Display::drawFxBadges(const bool* active) {
    const int16_t badgeW = W / 6;
    const int16_t bh     = Y_FX_BOT - Y_FX_TOP;

    for (int i = 0; i < 6; ++i) {
        int16_t bx = i * badgeW;
        // Active = inverted (fg fill, bg text); inactive = normal (bg fill, fg text)
        uint16_t textCol = active[i] ? _bg : _fg;
        uint16_t fillCol = active[i] ? _fg : _bg;
        filledCenteredText(tft, FX_NAMES[i], bx, Y_FX_TOP, badgeW, bh,
                           1, textCol, fillCol);
    }

    // Restore vertical separators erased by fillRect
    for (int i = 1; i < 6; ++i)
        tft->drawFastVLine(i * badgeW, Y_FX_TOP, bh, _fg);
}

// ---------------------------------------------------------------------------
// Knob grid (y 109–220) — 3 columns × 2 rows
// ---------------------------------------------------------------------------
void Display::drawKnobCell(int16_t x, int16_t y, int16_t w, int16_t h,
                           const char* label, float value) {
    const int16_t pad = 3;

    // Clear interior (preserve 1px border drawn by drawStatic)
    tft->fillRect(x + 1, y + 1, w - 2, h - 2, _bg);

    // Label: size 1
    tft->setTextColor(_fg, _bg);
    tft->setTextSize(1);
    tft->setCursor(x + pad, y + pad);
    if (label) tft->print(label);

    // Value: size 2
    char buf[8];
    snprintf(buf, sizeof(buf), "%.2f", value);
    tft->setTextSize(2);
    tft->setCursor(x + pad, y + pad + 10);
    tft->print(buf);

    // Progress bar: 2px thick, 4px from bottom
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;
    int16_t barY   = y + h - 5;
    int16_t barLen = (int16_t)(value * (float)(w - 2));
    tft->drawFastHLine(x + 1, barY,     w - 2, _bg);   // erase
    tft->drawFastHLine(x + 1, barY + 1, w - 2, _bg);
    if (barLen > 0) {
        tft->drawFastHLine(x + 1, barY,     barLen, _fg);  // fill
        tft->drawFastHLine(x + 1, barY + 1, barLen, _fg);
    }
}

// Iterates all 6 knob cells then restores grid lines erased by their fillRects.
void Display::drawKnobGrid(const float* values, const char* const* labels) {
    const int16_t kw = W / KNOB_COLS;
    const int16_t kh = (Y_KNOB_BOT - Y_KNOB_TOP) / KNOB_ROWS;

    for (int row = 0; row < KNOB_ROWS; ++row)
        for (int col = 0; col < KNOB_COLS; ++col)
            drawKnobCell(col * kw, Y_KNOB_TOP + row * kh, kw, kh,
                         labels[row * KNOB_COLS + col],
                         values[row * KNOB_COLS + col]);

    // Restore grid lines erased by fillRect in drawKnobCell
    for (int c = 1; c < KNOB_COLS; ++c)
        tft->drawFastVLine(c * kw, Y_KNOB_TOP, Y_KNOB_BOT - Y_KNOB_TOP, _fg);
    tft->drawFastHLine(0, Y_KNOB_TOP + kh, W, _fg);
}

// ---------------------------------------------------------------------------
// Bottom bar (y 220–240)
// ---------------------------------------------------------------------------
void Display::drawBottomBar(uint8_t activePage) {
    const int16_t btnW = W / 4;
    const int16_t bh   = H - Y_BTN_TOP;

    for (int i = 0; i < 4; ++i) {
        int16_t  bx      = i * btnW;
        uint16_t textCol = (i == activePage) ? _bg : _fg;
        uint16_t fillCol = (i == activePage) ? _fg : _bg;
        filledCenteredText(tft, BTN_NAMES[i], bx, Y_BTN_TOP, btnW, bh,
                           1, textCol, fillCol);
    }

    // Restore vertical separators
    for (int i = 1; i < 4; ++i)
        tft->drawFastVLine(i * btnW, Y_BTN_TOP, bh, _fg);
}

// ---------------------------------------------------------------------------

// Full dynamic redraw: top bar, FX badges, knob grid, bottom bar, then fix any stray erased lines.
void Display::updateValues(const DisplayState& state) {
    drawTopBar(state.waveformIndex, state.pageName);
    drawFxBadges(state.fxActive);
    drawKnobGrid(state.knobValues, state.knobLabels);
    drawBottomBar(state.activePage);
    restoreDividers();  // final pass — any missed fills are corrected
}
