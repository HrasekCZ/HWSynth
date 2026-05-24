#pragma once
// Synth engine: SynthState snapshot struct, ADSR/LFO/arpeggiator, I2S DMA output.

#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>
#include "config.h"
#include "pins.h"
#include "waveforms.h"
#include "effects.h"

// Arpeggiator pattern indices
#define ARP_UP      0
#define ARP_DOWN    1
#define ARP_UP_DOWN 2
#define ARP_RANDOM  3

struct SynthState {
    float frequency;     // Hz  (base pitch, before LFO / arp)
    float volume;        // 0.0 – 1.0
    int   waveform_a;    // WAVE_* index
    int   waveform_b;    // WAVE_* index
    float blend;         // 0.0 = all A, 1.0 = all B
    float phase;         // 0.0 – 1.0, oscillator accumulator
    float lfo_rate;      // 0.0-1.0  →  0.1-10 Hz
    float lfo_depth;     // 0.0-1.0  →  ±0.5 octaves max
    float adsr_attack;   // seconds (0.0-2.0)
    float adsr_release;  // seconds (0.0-2.0)
    bool  arp_enabled;   // true = arpeggiator active
    float arp_rate;      // 0.0-1.0  →  0.1-10 notes/sec
    int   arp_pattern;   // ARP_UP / DOWN / UP_DOWN / RANDOM
};

class Synth {
public:
    Synth();
    void begin();
    void update(SynthState& state);
    void setParam(uint8_t pot, float value);

    EffectsChain chain;  // public — main loop configures enable/params

private:
    void  fillBuffer(SynthState& state);
    float tickAdsr(const SynthState& state);
    float tickLfo(const SynthState& state);
    float tickArp(const SynthState& state);

    int16_t _buffer[BUFFER_SIZE * 2];  // stereo interleaved L, R, L, R …

    // ---- ADSR state ----
    enum AdsrStage : uint8_t {
        ADSR_ATTACK,
        ADSR_DECAY,
        ADSR_HOLD,      // sustain hold (renamed to avoid clash with constant below)
        ADSR_RELEASE
    };
    AdsrStage _adsrStage    = ADSR_ATTACK;
    float     _env          = 0.0f;    // current envelope level, 0.0-1.0
    uint32_t  _sustainCount = 0;       // samples spent in sustain hold

    // Fixed ADSR shape (no pots available for decay/sustain)
    static constexpr float    ADSR_DECAY_S      = 0.15f;
    static constexpr float    ADSR_SUSTAIN_LVL  = 0.70f;
    static constexpr uint32_t ADSR_HOLD_SAMPLES =
        (uint32_t)(0.20f * SAMPLE_RATE);

    // ---- LFO state ----
    float _lfoPhase = 0.0f;

    // ---- Arpeggiator state ----
    uint32_t _arpSampleCount = 0;   // samples elapsed on current note
    uint8_t  _arpStep        = 0;   // current position in the sequence
    uint32_t _arpNoiseSeed   = 22695477UL;  // LCG seed for RANDOM pattern
};
