// DSP effect implementations. Each effect is self-contained; EffectsChain runs them in order.
#include "effects.h"

// ============================================================================
// LowPass — biquad low-pass, direct form II transposed
// ============================================================================

// Recomputes biquad coefficients from cutoff frequency and Q; call on parameter change only.
void LowPass::calcCoeffs(float cutoffHz, float Q) {
    // Clamp to safe range
    if (cutoffHz < 20.f)     cutoffHz = 20.f;
    if (cutoffHz > 20000.f)  cutoffHz = 20000.f;
    if (Q < 0.5f)            Q = 0.5f;

    const float w0    = 2.f * (float)M_PI * cutoffHz / (float)SAMPLE_RATE;
    const float cosw0 = cosf(w0);
    const float sinw0 = sinf(w0);
    const float alpha = sinw0 / (2.f * Q);

    const float a0inv = 1.f / (1.f + alpha);

    _b0 =  (1.f - cosw0) * 0.5f * a0inv;
    _b1 =   (1.f - cosw0) * a0inv;
    _b2 =  _b0;
    _a1 = -2.f * cosw0 * a0inv;
    _a2 =  (1.f - alpha) * a0inv;
}

void LowPass::begin() {
    _s1 = _s2 = 0.f;
    calcCoeffs(1000.f, 1.f);
}

void LowPass::setParams(float cutoff, float resonance, float /*unused*/) {
    // cutoff:    0.0-1.0 → 20-20000 Hz (log)
    float hz = 20.f * powf(1000.f, cutoff);
    // resonance: 0.0-1.0 → Q 0.5-10
    float Q  = 0.5f + resonance * 9.5f;
    calcCoeffs(hz, Q);
}

float LowPass::process(float in) {
    if (!enabled) return in;
    // Direct form II transposed
    float out = _b0 * in + _s1;
    _s1 = _b1 * in - _a1 * out + _s2;
    _s2 = _b2 * in - _a2 * out;
    return out;
}

// ============================================================================
// Delay
// ============================================================================

void Delay::begin() {
    memset(_buf, 0, sizeof(_buf));
    _writeIdx     = 0;
    _delaySamples = BUF_LEN / 2;
    _feedback     = 0.5f;
    _mix          = 0.5f;
}

void Delay::setParams(float time, float feedback, float mix) {
    // time: 0.0-1.0 → 0-200 ms → 0-(BUF_LEN-1) samples
    _delaySamples = (int)(time * (float)(BUF_LEN - 1));
    if (_delaySamples < 1) _delaySamples = 1;

    _feedback = feedback < 0.f ? 0.f : (feedback > 0.95f ? 0.95f : feedback);
    _mix      = mix < 0.f ? 0.f : (mix > 1.f ? 1.f : mix);
}

float Delay::process(float in) {
    if (!enabled) return in;

    int readIdx = _writeIdx - _delaySamples;
    if (readIdx < 0) readIdx += BUF_LEN;

    float wet = _buf[readIdx];
    _buf[_writeIdx] = in + wet * _feedback;

    if (++_writeIdx >= BUF_LEN) _writeIdx = 0;

    return in * (1.f - _mix) + wet * _mix;
}

// ============================================================================
// Chorus
// ============================================================================

// Linear interpolation into the circular delay buffer
float Chorus::readInterp(float delaySamples) const {
    int   idx0 = _writeIdx - 1 - (int)delaySamples;
    float frac = delaySamples - floorf(delaySamples);

    // Wrap indices
    idx0 = ((idx0 % BUF_LEN) + BUF_LEN) % BUF_LEN;
    int idx1 = (idx0 + 1) % BUF_LEN;

    return _buf[idx0] * (1.f - frac) + _buf[idx1] * frac;
}

void Chorus::begin() {
    memset(_buf, 0, sizeof(_buf));
    _writeIdx   = 0;
    _lfoPhase0  = 0.f;
    _lfoPhase1  = 0.25f;
    setParams(0.2f, 0.5f, 0.5f);
}

void Chorus::setParams(float rate, float depth, float mix) {
    // rate:  0.0-1.0 → 0.1-5 Hz
    float rateHz = 0.1f + rate * 4.9f;
    _lfoInc   = rateHz / (float)SAMPLE_RATE;

    // depth: 0.0-1.0 → 0-10 ms → 0-441 samples
    // Capped so centre(441) + depth(441) = 882 < BUF_LEN(1024) — no wrap hazard
    _depthSmp = depth * 441.f;

    _mix = mix < 0.f ? 0.f : (mix > 1.f ? 1.f : mix);
}

float Chorus::process(float in) {
    _buf[_writeIdx] = in;
    if (++_writeIdx >= BUF_LEN) _writeIdx = 0;

    if (!enabled) return in;

    // Base delay centre: 10 ms = 441 samples
    const float centre = 441.f;

    float d0 = centre + _depthSmp * sinf(2.f * (float)M_PI * _lfoPhase0);
    float d1 = centre + _depthSmp * sinf(2.f * (float)M_PI * _lfoPhase1);

    _lfoPhase0 += _lfoInc;
    if (_lfoPhase0 >= 1.f) _lfoPhase0 -= 1.f;
    _lfoPhase1 += _lfoInc;
    if (_lfoPhase1 >= 1.f) _lfoPhase1 -= 1.f;

    float wet = (readInterp(d0) + readInterp(d1)) * 0.5f;
    return in * (1.f - _mix) + wet * _mix;
}

// ============================================================================
// Tremolo
// ============================================================================

void Tremolo::begin() {
    _lfoPhase = 0.f;
    setParams(0.2f, 0.5f, 0.f);
}

void Tremolo::setParams(float rate, float depth, float /*unused*/) {
    // rate:  0.0-1.0 → 0.1-10 Hz
    float rateHz = 0.1f + rate * 9.9f;
    _lfoInc = rateHz / (float)SAMPLE_RATE;
    _depth  = depth < 0.f ? 0.f : (depth > 1.f ? 1.f : depth);
}

float Tremolo::process(float in) {
    float lfo = 0.5f + 0.5f * sinf(2.f * (float)M_PI * _lfoPhase);

    _lfoPhase += _lfoInc;
    if (_lfoPhase >= 1.f) _lfoPhase -= 1.f;

    if (!enabled) return in;

    // Gain oscillates between (1-depth) and 1.0
    float gain = 1.f - _depth * (1.f - lfo);
    return in * gain;
}

// ============================================================================
// Bitcrusher
// ============================================================================

void Bitcrusher::begin() {
    _levels = 65536.f;  // 16-bit
}

void Bitcrusher::setParams(float bits, float /*unused*/, float /*unused*/) {
    // bits: 0.0-1.0 → 16 bits down to 1 bit
    float b = 1.f + (1.f - bits) * 15.f;  // 0→16, 1→1
    _levels = powf(2.f, b);
}

float Bitcrusher::process(float in) {
    if (!enabled) return in;
    // Quantise: round to nearest step, keep in -1..1
    float q = floorf(in * _levels * 0.5f + 0.5f) / (_levels * 0.5f);
    // Clamp
    return q < -1.f ? -1.f : (q > 1.f ? 1.f : q);
}

// ============================================================================
// Distortion
// ============================================================================

void Distortion::begin() {
    _lpState = 0.f;
    setParams(0.5f, 0.8f, 0.f);
}

void Distortion::setParams(float drive, float tone, float /*unused*/) {
    // drive: 0.0-1.0 → 1.0-20.0
    _drive = 1.f + drive * 19.f;

    // tone: 0.0-1.0 → one-pole LP coefficient 0.05-1.0 (higher = brighter)
    _toneCoeff = 0.05f + tone * 0.95f;
}

float Distortion::process(float in) {
    if (!enabled) return in;

    // Apply drive then soft-clip via tanh
    float driven = in * _drive;
    float clipped = tanhf(driven);

    // One-pole LP tone filter: y[n] = c*x[n] + (1-c)*y[n-1]
    _lpState = _toneCoeff * clipped + (1.f - _toneCoeff) * _lpState;

    // Compensate for drive gain
    return _lpState / tanhf(_drive);
}

// ============================================================================
// EffectsChain
// ============================================================================

void EffectsChain::begin() {
    lowpass.begin();
    delay.begin();
    chorus.begin();
    tremolo.begin();
    bitcrusher.begin();
    distortion.begin();
}

float EffectsChain::process(float sample) {
    sample = lowpass.process(sample);
    sample = delay.process(sample);
    sample = chorus.process(sample);
    sample = tremolo.process(sample);
    sample = bitcrusher.process(sample);
    sample = distortion.process(sample);
    return sample;
}

void EffectsChain::setEnabled(uint8_t idx, bool on) {
    switch (idx) {
        case FX_LOWPASS:    lowpass.enabled    = on; break;
        case FX_DELAY:      delay.enabled      = on; break;
        case FX_CHORUS:     chorus.enabled     = on; break;
        case FX_TREMOLO:    tremolo.enabled     = on; break;
        case FX_BITCRUSH:   bitcrusher.enabled  = on; break;
        case FX_DISTORTION: distortion.enabled  = on; break;
        default: break;
    }
}

bool EffectsChain::getEnabled(uint8_t idx) const {
    switch (idx) {
        case FX_LOWPASS:    return lowpass.enabled;
        case FX_DELAY:      return delay.enabled;
        case FX_CHORUS:     return chorus.enabled;
        case FX_TREMOLO:    return tremolo.enabled;
        case FX_BITCRUSH:   return bitcrusher.enabled;
        case FX_DISTORTION: return distortion.enabled;
        default:            return false;
    }
}

void EffectsChain::setParams(uint8_t idx, float p1, float p2, float p3) {
    switch (idx) {
        case FX_LOWPASS:    lowpass.setParams(p1, p2, p3);    break;
        case FX_DELAY:      delay.setParams(p1, p2, p3);      break;
        case FX_CHORUS:     chorus.setParams(p1, p2, p3);     break;
        case FX_TREMOLO:    tremolo.setParams(p1, p2, p3);    break;
        case FX_BITCRUSH:   bitcrusher.setParams(p1, p2, p3); break;
        case FX_DISTORTION: distortion.setParams(p1, p2, p3); break;
        default: break;
    }
}
