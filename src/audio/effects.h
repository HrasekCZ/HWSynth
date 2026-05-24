#pragma once
// DSP effects chain: 6 effect classes (LPF, delay, chorus, tremolo, bitcrusher, distortion)
// and EffectsChain which processes them in series.

#include <Arduino.h>
#include <math.h>
#include "config.h"

// ---------------------------------------------------------------------------
// Effect index constants — match EffectsChain::_fx[] order and fxActive[]
// ---------------------------------------------------------------------------
#define FX_LOWPASS    0
#define FX_DELAY      1
#define FX_CHORUS     2
#define FX_TREMOLO    3
#define FX_BITCRUSH   4
#define FX_DISTORTION 5
#define FX_COUNT      6

// ---------------------------------------------------------------------------
// LowPass — biquad, direct form II transposed
// p1 = cutoff  (0.0-1.0 → 20-20000 Hz)
// p2 = resonance (0.0-1.0 → Q 0.5-10)
// p3 = unused
// ---------------------------------------------------------------------------
class LowPass {
public:
    bool enabled = false;
    void  begin();
    float process(float in);
    void  setParams(float cutoff, float resonance, float /*unused*/);

private:
    // Biquad coefficients
    float _b0 = 1.f, _b1 = 0.f, _b2 = 0.f;
    float _a1 = 0.f, _a2 = 0.f;
    // Direct form II transposed state
    float _s1 = 0.f, _s2 = 0.f;

    void calcCoeffs(float cutoffHz, float Q);
};

// ---------------------------------------------------------------------------
// Delay — circular buffer, 0.5 s max
// p1 = time      (0.0-1.0 → 0-500 ms)
// p2 = feedback  (0.0-0.95)
// p3 = mix       (0.0-1.0, dry/wet)
// ---------------------------------------------------------------------------
class Delay {
public:
    bool enabled = false;
    void  begin();
    float process(float in);
    void  setParams(float time, float feedback, float mix);

private:
    static constexpr int BUF_LEN = 8820;   // 200 ms at 44100 Hz (35 KB)
    float    _buf[BUF_LEN] = {};
    int      _writeIdx  = 0;
    int      _delaySamples = 0;
    float    _feedback  = 0.5f;
    float    _mix       = 0.5f;
};

// ---------------------------------------------------------------------------
// Chorus — two LFO-modulated delay lines
// p1 = rate  (0.0-1.0 → 0.1-5 Hz)
// p2 = depth (0.0-1.0)
// p3 = mix   (0.0-1.0)
// ---------------------------------------------------------------------------
class Chorus {
public:
    bool enabled = false;
    void  begin();
    float process(float in);
    void  setParams(float rate, float depth, float mix);

private:
    static constexpr int BUF_LEN = 1024;  // ~23 ms — enough for chorus depth (1 KB)
    float _buf[BUF_LEN] = {};
    int   _writeIdx = 0;

    // Two LFO voices, offset by 90°
    float _lfoPhase0 = 0.f;
    float _lfoPhase1 = 0.25f;  // quarter-cycle offset

    float _lfoInc   = 0.f;   // phase increment per sample
    float _depthSmp = 0.f;   // depth in samples
    float _mix      = 0.5f;

    float readInterp(float delaySamples) const;
};

// ---------------------------------------------------------------------------
// Tremolo — AM via sine LFO
// p1 = rate  (0.0-1.0 → 0.1-10 Hz)
// p2 = depth (0.0-1.0)
// p3 = unused
// ---------------------------------------------------------------------------
class Tremolo {
public:
    bool enabled = false;
    void  begin();
    float process(float in);
    void  setParams(float rate, float depth, float /*unused*/);

private:
    float _lfoPhase = 0.f;
    float _lfoInc   = 0.f;
    float _depth    = 0.5f;
};

// ---------------------------------------------------------------------------
// Bitcrusher — reduce bit depth
// p1 = bits (0.0-1.0 → 16 down to 1 bit)
// p2 = unused
// p3 = unused
// ---------------------------------------------------------------------------
class Bitcrusher {
public:
    bool enabled = false;
    void  begin();
    float process(float in);
    void  setParams(float bits, float /*unused*/, float /*unused*/);

private:
    float _levels = 65536.f;  // 2^16
};

// ---------------------------------------------------------------------------
// Distortion — soft clip (tanh) + one-pole LP tone filter
// p1 = drive (0.0-1.0 → 1.0-20.0)
// p2 = tone  (0.0-1.0, LP cutoff)
// p3 = unused
// ---------------------------------------------------------------------------
class Distortion {
public:
    bool enabled = false;
    void  begin();
    float process(float in);
    void  setParams(float drive, float tone, float /*unused*/);

private:
    float _drive     = 1.f;
    float _toneCoeff = 0.5f;  // one-pole LP: y = coeff*x + (1-coeff)*y_prev
    float _lpState   = 0.f;
};

// ---------------------------------------------------------------------------
// EffectsChain — holds all 6 effects, processes in order when enabled
// ---------------------------------------------------------------------------
class EffectsChain {
public:
    EffectsChain() = default;
    void  begin();
    float process(float sample);

    // Access individual effects by FX_* index
    void  setEnabled(uint8_t idx, bool on);
    bool  getEnabled(uint8_t idx) const;
    void  setParams(uint8_t idx, float p1, float p2, float p3);

    // Direct references for convenient main-loop access
    LowPass     lowpass;
    Delay       delay;
    Chorus      chorus;
    Tremolo     tremolo;
    Bitcrusher  bitcrusher;
    Distortion  distortion;
};
