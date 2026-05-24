// Synth engine implementation: I2S config, per-sample ADSR/LFO/arp ticks, DMA buffer fill.
#include "synth.h"

// TWO_PI is already defined by Arduino.h (6.2831853...)

static const i2s_config_t I2S_CFG = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate          = SAMPLE_RATE,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count        = 4,
    .dma_buf_len          = BUFFER_SIZE,
    .use_apll             = true,
    .tx_desc_auto_clear   = true,
    .fixed_mclk           = 0,
};

static const i2s_pin_config_t I2S_PINS = {
    .bck_io_num   = I2S_BCLK,
    .ws_io_num    = I2S_LRCK,
    .data_out_num = I2S_DOUT,
    .data_in_num  = I2S_PIN_NO_CHANGE,
};

// ---------------------------------------------------------------------------

Synth::Synth() : _buffer{} {}

void Synth::begin() {
    chain.begin();
    _adsrStage       = ADSR_ATTACK;
    _env             = 0.0f;
    _sustainCount    = 0;
    _lfoPhase        = 0.0f;
    _arpSampleCount  = 0;
    _arpStep         = 0;
    _arpNoiseSeed    = 22695477UL;
    i2s_driver_install(I2S_NUM_0, &I2S_CFG, 0, nullptr);
    i2s_set_pin(I2S_NUM_0, &I2S_PINS);
    i2s_zero_dma_buffer(I2S_NUM_0);
}

// ---------------------------------------------------------------------------
// ADSR — looping: when release ends, immediately retrigger attack
// ---------------------------------------------------------------------------
float Synth::tickAdsr(const SynthState& s) {
    // Clamp incoming params to safe ranges
    float attack  = s.adsr_attack  < 0.001f ? 0.001f : s.adsr_attack;
    float release = s.adsr_release < 0.001f ? 0.001f : s.adsr_release;

    switch (_adsrStage) {

        case ADSR_ATTACK:
            _env += 1.0f / (attack * (float)SAMPLE_RATE);
            if (_env >= 1.0f) {
                _env       = 1.0f;
                _adsrStage = ADSR_DECAY;
            }
            break;

        case ADSR_DECAY: {
            float step = (1.0f - ADSR_SUSTAIN_LVL)
                         / (ADSR_DECAY_S * (float)SAMPLE_RATE);
            _env -= step;
            if (_env <= ADSR_SUSTAIN_LVL) {
                _env          = ADSR_SUSTAIN_LVL;
                _sustainCount = 0;
                _adsrStage    = ADSR_HOLD;
            }
            break;
        }

        case ADSR_HOLD:
            ++_sustainCount;
            if (_sustainCount >= ADSR_HOLD_SAMPLES)
                _adsrStage = ADSR_RELEASE;
            break;

        case ADSR_RELEASE: {
            float step = ADSR_SUSTAIN_LVL
                         / (release * (float)SAMPLE_RATE);
            _env -= step;
            if (_env <= 0.0f) {
                _env       = 0.0f;
                _adsrStage = ADSR_ATTACK;  // loop
            }
            break;
        }
    }

    return _env;
}

// ---------------------------------------------------------------------------
// LFO — sine, modulates pitch ±depth*0.5 octaves
// Returns the octave offset (multiply base frequency by pow(2, result))
// ---------------------------------------------------------------------------
float Synth::tickLfo(const SynthState& s) {
    float rateHz = 0.1f + s.lfo_rate * 9.9f;   // 0.1–10 Hz
    float out    = sinf((float)TWO_PI * _lfoPhase) * s.lfo_depth * 0.5f;

    _lfoPhase += rateHz / (float)SAMPLE_RATE;
    if (_lfoPhase >= 1.0f) _lfoPhase -= 1.0f;

    return out;  // octaves: ±(depth * 0.5)
}

// ---------------------------------------------------------------------------
// Arpeggiator — advances on its own sample timer, independent of ADSR
// Returns the frequency of the current arp note.
// ---------------------------------------------------------------------------
float Synth::tickArp(const SynthState& s) {
    // Precomputed frequency ratios: root, minor 3rd (+3 st), P5 (+7 st), octave (+12 st)
    static const float RATIOS[4] = {
        1.000000f,   // 2^(0/12)
        1.189207f,   // 2^(3/12)
        1.498307f,   // 2^(7/12)
        2.000000f,   // 2^(12/12)
    };
    // Sequence tables (indices into RATIOS[])
    static const uint8_t SEQ_UP[4]     = {0, 1, 2, 3};
    static const uint8_t SEQ_DOWN[4]   = {3, 2, 1, 0};
    static const uint8_t SEQ_UPDOWN[6] = {0, 1, 2, 3, 2, 1};
    // Sequence lengths for each pattern
    static const uint8_t SEQ_LEN[4]    = {4, 4, 6, 4};

    // arp_rate stored as Hz directly (0.1–10)
    float    rateHz  = s.arp_rate < 0.1f ? 0.1f : (s.arp_rate > 10.f ? 10.f : s.arp_rate);
    uint32_t holdLen = (uint32_t)((float)SAMPLE_RATE / rateHz);
    if (holdLen < 1) holdLen = 1;

    // Advance step when hold period expires
    if (++_arpSampleCount >= holdLen) {
        _arpSampleCount = 0;
        int pat = (s.arp_pattern >= 0 && s.arp_pattern <= 3) ? s.arp_pattern : 0;

        if (pat == ARP_RANDOM) {
            // LCG — pick note index 0-3 directly
            _arpNoiseSeed = _arpNoiseSeed * 1664525UL + 1013904223UL;
            _arpStep = (uint8_t)((_arpNoiseSeed >> 16) & 0x03);
        } else {
            _arpStep = (_arpStep + 1) % SEQ_LEN[pat];
        }
    }

    // Map current step → note index → frequency ratio
    int     pat     = (s.arp_pattern >= 0 && s.arp_pattern <= 3) ? s.arp_pattern : 0;
    uint8_t noteIdx = 0;
    switch (pat) {
        case ARP_UP:      noteIdx = SEQ_UP[_arpStep % 4];     break;
        case ARP_DOWN:    noteIdx = SEQ_DOWN[_arpStep % 4];   break;
        case ARP_UP_DOWN: noteIdx = SEQ_UPDOWN[_arpStep % 6]; break;
        case ARP_RANDOM:  noteIdx = _arpStep & 0x03;          break;
    }

    return s.frequency * RATIOS[noteIdx];
}

// ---------------------------------------------------------------------------
// Buffer fill
// ---------------------------------------------------------------------------
void Synth::fillBuffer(SynthState& state) {
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        // --- Envelope ---
        float envGain = tickAdsr(state);

        // --- Arpeggiator: overrides base frequency when enabled ---
        float baseFreq = state.arp_enabled ? tickArp(state) : state.frequency;

        // --- LFO: frequency modulation (applied on top of arp note) ---
        float lfoOct  = tickLfo(state);
        float modFreq = baseFreq * powf(2.0f, lfoOct);

        // --- Oscillator ---
        float sample = blendWaveforms(state.waveform_a, state.waveform_b,
                                      state.phase, state.blend);

        // --- Effects chain (pre-VCA, full signal level) ---
        sample = chain.process(sample);

        // --- VCA: envelope × volume ---
        sample *= envGain * state.volume;

        // --- Output ---
        int16_t s16 = (int16_t)(sample * 32767.0f);
        _buffer[i * 2]     = s16;  // L
        _buffer[i * 2 + 1] = s16;  // R

        // --- Phase accumulation (LFO-modulated pitch) ---
        state.phase += modFreq / (float)SAMPLE_RATE;
        if (state.phase >= 1.0f) state.phase -= 1.0f;
    }
}

void Synth::update(SynthState& state) {
    fillBuffer(state);

    size_t bytesWritten = 0;
    i2s_write(I2S_NUM_0, _buffer, sizeof(_buffer), &bytesWritten, portMAX_DELAY);
}

void Synth::setParam(uint8_t pot, float value) {}
