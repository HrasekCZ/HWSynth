// Controls implementation: I2C ADC init with fallback, pot normalisation, debounce, switch.
#include "controls.h"

// constexpr static arrays need a definition in one TU for older compilers
constexpr uint8_t  Controls::BTN_PINS[];
constexpr uint32_t Controls::DEBOUNCE_MS;
constexpr float    Controls::ADS_SCALE;

// ---------------------------------------------------------------------------

Controls::Controls()
    : _ads1(ADS1_ADDR), _ads2(ADS2_ADDR), _btn{}
{}

void Controls::begin() {
    Wire.begin(I2C_SDA, I2C_SCL);

    _ads1Ok = _ads1.begin();
    if (!_ads1Ok) {
        Serial.println("[Controls] WARNING: ADS1115 #1 (0x48) not found — pots 0-3 will return 0.5");
    } else {
        _ads1.setGain(0);      // ±6.144 V — widest range, maps full pot swing
        _ads1.setDataRate(7);  // 860 SPS — fastest, minimises I2C blocking time
    }

    _ads2Ok = _ads2.begin();
    if (!_ads2Ok) {
        Serial.println("[Controls] WARNING: ADS1115 #2 (0x49) not found — pots 4-7 will return 0.5");
    } else {
        _ads2.setGain(0);
        _ads2.setDataRate(7);
    }

    pinMode(BTN_PREV,  INPUT_PULLUP);
    pinMode(BTN_NEXT,  INPUT_PULLUP);
    pinMode(BTN_ONOFF, INPUT_PULLUP);
    pinMode(BTN_MUTE,  INPUT_PULLUP);
    pinMode(SW_JACK, INPUT_PULLUP);

    // Seed debounce state from current pin levels so first update is clean
    for (uint8_t i = 0; i < 4; ++i) {
        bool level        = digitalRead(BTN_PINS[i]);
        _btn[i].lastRaw    = level;
        _btn[i].lastStable = level;
        _btn[i].lastChangeMs = 0;
    }
}

// ---------------------------------------------------------------------------

// Reads one ADC channel and normalises to 0.0–1.0; returns 0.5 if the chip is absent.
float Controls::readPot(ADS1115& ads, uint8_t channel) {
    bool ok = (&ads == &_ads1) ? _ads1Ok : _ads2Ok;
    if (!ok) return 0.5f;
    int16_t raw = ads.readADC(channel);
    if (raw < 0)     raw = 0;
    if (raw > 32767) raw = 32767;
    return (float)raw * ADS_SCALE;
}

// Returns true only on the rising-edge (LOW→HIGH, i.e. button released after
// a confirmed press — but since INPUT_PULLUP logic is inverted we treat the
// falling edge of the pin as the press event and fire on the stable LOW).
bool Controls::debounce(uint8_t idx) {
    bool raw = digitalRead(BTN_PINS[idx]);
    uint32_t now = millis();

    if (raw != _btn[idx].lastRaw) {
        // Pin changed — reset timer
        _btn[idx].lastRaw      = raw;
        _btn[idx].lastChangeMs = now;
        return false;
    }

    // Pin has been stable long enough
    if ((now - _btn[idx].lastChangeMs) >= DEBOUNCE_MS) {
        bool prev          = _btn[idx].lastStable;
        _btn[idx].lastStable = raw;

        // With INPUT_PULLUP: pressed = LOW (false), released = HIGH (true).
        // Fire on falling edge: was HIGH, now stable LOW → press event.
        if (prev == true && raw == false)
            return true;
    }

    return false;
}

// ---------------------------------------------------------------------------

void Controls::update(ControlState& state) {
    // --- Pots: read all 8 channels across the two ADCs ---
    state.pots[POT_VOLUME]     = readPot(_ads1, 0);
    state.pots[POT_PITCH]      = readPot(_ads1, 1);
    state.pots[POT_LFO_RATE]   = readPot(_ads1, 2);
    state.pots[POT_LFO_DEPTH]  = readPot(_ads1, 3);
    state.pots[POT_BLEND]      = readPot(_ads2, 0);
    state.pots[POT_CUTOFF]     = readPot(_ads2, 1);
    state.pots[POT_RESONANCE]  = readPot(_ads2, 2);
    state.pots[POT_CONTEXTUAL] = readPot(_ads2, 3);

    // --- Buttons: debounced, true for one cycle on press ---
    state.btn_prev  = debounce(0);
    state.btn_next  = debounce(1);
    state.btn_onoff = debounce(2);
    state.btn_mute  = debounce(3);

    // --- Switch: HIGH = line out (jack inserted), LOW = repro ---
    state.sw = (digitalRead(SW_JACK) == HIGH) ? SwitchPos::LINEOUT : SwitchPos::REPRO;
}
