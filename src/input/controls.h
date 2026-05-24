#pragma once
// Input subsystem: ADS1115 pot reading (with fallback), button debounce, output switch.

#include <Arduino.h>
#include <ADS1X15.h>
#include "pins.h"
#include "config.h"

enum class SwitchPos : uint8_t {
    REPRO   = 0,  // SW_JACK LOW
    LINEOUT = 1,  // SW_JACK HIGH
};

struct ControlState {
    float     pots[NUM_POTS];  // 0.0 – 1.0, indices match POT_* defines
    bool      btn_prev;        // true for one update cycle on press
    bool      btn_next;
    bool      btn_onoff;
    bool      btn_mute;
    SwitchPos sw;
};

class Controls {
public:
    Controls();
    void begin();
    void update(ControlState& state);

private:
    ADS1115 _ads1;  // 0x48 — pots 0-3
    ADS1115 _ads2;  // 0x49 — pots 4-7

    // Debounce state per button: last stable pin state + timestamp
    struct BtnState {
        bool     lastStable;
        bool     lastRaw;
        uint32_t lastChangeMs;
    };
    BtnState _btn[4];  // PREV, NEXT, ONOFF, MUTE

    static constexpr uint8_t  BTN_PINS[4]    = { BTN_PREV, BTN_NEXT, BTN_ONOFF, BTN_MUTE };
    static constexpr uint32_t DEBOUNCE_MS     = 50;
    static constexpr float    ADS_SCALE       = 1.0f / 32767.0f;

    bool _ads1Ok = false;  // false → hardware absent, readPot returns 0.5
    bool _ads2Ok = false;

    // Returns true if this update the button just transitioned to pressed
    bool debounce(uint8_t idx);

    float readPot(ADS1115& ads, uint8_t channel);
};
