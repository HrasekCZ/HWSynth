// HWSynth — top-level application: FreeRTOS task wiring, 100 Hz control loop, 30 Hz display loop.
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <Preferences.h>
#include <math.h>
#include <string.h>

#include "pins.h"
#include "config.h"
#include "audio/synth.h"
#include "audio/waveforms.h"
#include "ui/display.h"
#include "input/controls.h"

// ---------------------------------------------------------------------------
// Global objects
// ---------------------------------------------------------------------------
static Synth    synth;
static Display  display;
static Controls controls;

// ---------------------------------------------------------------------------
// Shared state — protected by synthMutex
// Core 0 reads and writes .phase; Core 1 writes everything else.
// ---------------------------------------------------------------------------
static SynthState        synthState;
static SemaphoreHandle_t synthMutex;

// ---------------------------------------------------------------------------
// Core-1-only state — no mutex needed
// ---------------------------------------------------------------------------
static ControlState ctrlState   = {};
static DisplayState dispState   = {};
static float        scopeBuf[300];

// Mute
static float prevVolume = 0.8f;
static bool  muted      = false;

// Dark / light mode
static bool        darkMode = true;
static Preferences prefs;

// FX — authoritative state lives in synth.chain; activeFxIdx tracks next toggle
static uint8_t activeFxIdx = 0;

// Bottom-bar highlight (last intentional button press)
static uint8_t activePageIdx = 1;  // 0=PREV 1=ON 2=MUTE 3=NEXT

// ---------------------------------------------------------------------------
// Knob labels shown in the 3×2 grid (pots 0-5)
// ---------------------------------------------------------------------------
static const char* const KNOB_LABELS[6] = {
    "VOL", "PITCH", "LFO-R", "LFO-D", "BLEND", "CUT"
};

// Forward declaration (defined later, called from handleButtons combo handler)
static void buildDisplayState();

// ---------------------------------------------------------------------------
// NVS helpers
// ---------------------------------------------------------------------------
static void nvsLoad() {
    prefs.begin("synth", /*readOnly=*/true);
    darkMode = prefs.getBool("darkmode", true);
    prefs.end();
}

static void nvsSave() {
    prefs.begin("synth", /*readOnly=*/false);
    prefs.putBool("darkmode", darkMode);
    prefs.end();
}

// ---------------------------------------------------------------------------
// Mode defaults — applied to SynthState on dark/light toggle
// ---------------------------------------------------------------------------
static void applyModeDefaults() {
    xSemaphoreTake(synthMutex, portMAX_DELAY);
    if (darkMode) {
        // Dark: slow, brooding
        synthState.arp_rate     = 1.0f;   // Hz
        synthState.adsr_attack  = 0.3f;   // seconds
        synthState.adsr_release = 0.8f;
        synthState.lfo_depth    = 0.4f;   // 0-1
    } else {
        // Light: fast, bright
        synthState.arp_rate     = 4.0f;   // Hz
        synthState.adsr_attack  = 0.05f;
        synthState.adsr_release = 0.2f;
        synthState.lfo_depth    = 0.1f;
    }
    xSemaphoreGive(synthMutex);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Logarithmic mapping: pot 0.0→1.0  ⟹  20Hz→20000Hz
static inline float pitchFromPot(float pot) {
    return 20.0f * powf(1000.0f, pot);
}

// Fill scopeBuf with one full period of the blended waveform
static void fillScopeBuffer(int waveA, int waveB, float blend) {
    for (int i = 0; i < 300; ++i)
        scopeBuf[i] = blendWaveforms(waveA, waveB, (float)i / 300.0f, blend);
}

// ---------------------------------------------------------------------------
// Audio task — Core 0, priority 2
//
// Pattern: snapshot state (mutex held briefly) → generate audio (mutex free,
// i2s_write blocks ~5.8 ms) → write phase back (mutex held briefly).
// The mutex is never held across the blocking i2s_write call.
// ---------------------------------------------------------------------------
static void audioTask(void* /*pvParameters*/) {
    for (;;) {
        // 1. Snapshot current parameters
        SynthState local;
        xSemaphoreTake(synthMutex, portMAX_DELAY);
        local = synthState;
        xSemaphoreGive(synthMutex);

        // 2. Fill DMA buffer + block until I2S consumes it (~5.8 ms)
        synth.update(local);

        // 3. Write back only phase — main task may have updated everything
        //    else during the i2s_write window, which is intentional.
        xSemaphoreTake(synthMutex, portMAX_DELAY);
        synthState.phase = local.phase;
        xSemaphoreGive(synthMutex);
    }
}

// ---------------------------------------------------------------------------
// Apply pot values → SynthState  (called from Core 1 at 100 Hz)
// ---------------------------------------------------------------------------
static void applyControls() {
    float vol   = muted ? 0.0f : ctrlState.pots[POT_VOLUME];
    float freq  = pitchFromPot(ctrlState.pots[POT_PITCH]);
    float blend = ctrlState.pots[POT_BLEND];

    xSemaphoreTake(synthMutex, portMAX_DELAY);
    synthState.volume    = vol;
    synthState.frequency = freq;
    synthState.blend     = blend;
    synthState.lfo_rate  = ctrlState.pots[POT_LFO_RATE];
    synthState.lfo_depth = ctrlState.pots[POT_LFO_DEPTH];
    xSemaphoreGive(synthMutex);

    // LPF: cutoff and resonance pots drive the biquad directly
    synth.chain.setParams(FX_LOWPASS,
                          ctrlState.pots[POT_CUTOFF],
                          ctrlState.pots[POT_RESONANCE],
                          0.f);
}

// ---------------------------------------------------------------------------
// Handle button presses  (called from Core 1 at 100 Hz)
// ---------------------------------------------------------------------------
static void handleButtons() {
    // ---- Combo: PREV + ONOFF held simultaneously for 1000 ms ----
    // Raw pin read (INPUT_PULLUP → LOW = pressed)
    bool prevRaw  = (digitalRead(BTN_PREV)  == LOW);
    bool onoffRaw = (digitalRead(BTN_ONOFF) == LOW);
    bool comboHeld = prevRaw && onoffRaw;

    static uint32_t comboMs        = 0;
    static bool     comboTriggered = false;

    if (comboHeld) {
        if (comboMs == 0) {
            comboMs        = millis();
            if (comboMs == 0) comboMs = 1;  // guard against millis()==0 edge case
            comboTriggered = false;
        } else if (!comboTriggered && (millis() - comboMs >= 1000)) {
            comboTriggered = true;
            darkMode = !darkMode;
            applyModeDefaults();
            nvsSave();
            // Redraw entire UI in new palette
            buildDisplayState();
            display.setMode(darkMode);
            display.updateValues(dispState);
        }
        // Suppress individual PREV and ONOFF actions while combo is held
        return;
    } else {
        comboMs        = 0;
        comboTriggered = false;
    }

    // ---- PREV / NEXT — cycle waveform pair ----
    if (ctrlState.btn_prev || ctrlState.btn_next) {
        int delta = ctrlState.btn_next ? 1 : -1;
        activePageIdx = ctrlState.btn_next ? 3 : 0;

        xSemaphoreTake(synthMutex, portMAX_DELAY);
        synthState.waveform_a =
            (synthState.waveform_a + delta + NUM_WAVEFORMS) % NUM_WAVEFORMS;
        synthState.waveform_b =
            (synthState.waveform_a + 1) % NUM_WAVEFORMS;
        xSemaphoreGive(synthMutex);
    }

    // ---- ONOFF — toggle current FX slot, advance selector ----
    if (ctrlState.btn_onoff) {
        bool cur = synth.chain.getEnabled(activeFxIdx);
        synth.chain.setEnabled(activeFxIdx, !cur);
        activeFxIdx   = (activeFxIdx + 1) % FX_COUNT;
        activePageIdx = 1;
    }

    // ---- MUTE — toggle volume ----
    if (ctrlState.btn_mute) {
        muted         = !muted;
        activePageIdx = 2;

        xSemaphoreTake(synthMutex, portMAX_DELAY);
        if (muted) {
            prevVolume        = (synthState.volume > 0.0f)
                                    ? synthState.volume : prevVolume;
            synthState.volume = 0.0f;
        } else {
            synthState.volume = prevVolume;
        }
        xSemaphoreGive(synthMutex);
    }
}

// ---------------------------------------------------------------------------
// Assemble DisplayState + scope buffer  (called from Core 1 at ~30 Hz)
// ---------------------------------------------------------------------------
static void buildDisplayState() {
    // Snapshot the fields we need for display (brief mutex window)
    int   waveA, waveB;
    float blend;
    xSemaphoreTake(synthMutex, portMAX_DELAY);
    waveA = synthState.waveform_a;
    waveB = synthState.waveform_b;
    blend = synthState.blend;
    xSemaphoreGive(synthMutex);

    // Scope: one static period of the blended waveform
    fillScopeBuffer(waveA, waveB, blend);

    // Top bar
    dispState.waveformIndex = (uint8_t)waveA;
    dispState.pageName      = "SYNTH";

    // FX badges — read live enabled state from chain
    for (int i = 0; i < FX_COUNT; ++i)
        dispState.fxActive[i] = synth.chain.getEnabled(i);

    // Knob grid (pots 0-5)
    for (int i = 0; i < 6; ++i) {
        dispState.knobValues[i] = ctrlState.pots[i];
        dispState.knobLabels[i] = KNOB_LABELS[i];
    }

    // Bottom bar highlight
    dispState.activePage = muted ? 2 : activePageIdx;
}

// ---------------------------------------------------------------------------
// setup
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);

    synthMutex = xSemaphoreCreateMutex();
    configASSERT(synthMutex);

    synthState = {
        .frequency    = 440.0f,
        .volume       = 0.8f,
        .waveform_a   = WAVE_SINE,
        .waveform_b   = WAVE_SQUARE,
        .blend        = 0.0f,
        .phase        = 0.0f,
        .lfo_rate     = 0.1f,   // ~1 Hz default
        .lfo_depth    = 0.0f,   // off until pot moved
        .adsr_attack  = 0.1f,   // 100 ms fixed for now
        .adsr_release = 0.3f,   // 300 ms fixed for now
        .arp_enabled  = false,
        .arp_rate     = 2.0f,   // 2 Hz default (stored as Hz directly)
        .arp_pattern  = ARP_UP,
    };

    // --- Splash: load NVS first so palette is correct from the start ---
    nvsLoad();
    display.begin(darkMode);    // apply palette, no chrome yet
    display.showSplash();       // draw title + empty progress bar

    // --- Init steps — each advances the progress bar ---
    initWaveforms();
    synth.begin();                          // I2S driver install
    display.updateSplashProgress(1, 4);

    controls.begin();                       // Wire + ADS1115 #1
    display.updateSplashProgress(2, 4);

    // ADS1115 #2 initialised inside controls.begin(); second tick here
    display.updateSplashProgress(3, 4);

    display.updateSplashProgress(4, 4);     // all done
    delay(300);                             // let user see the full bar

    // --- Transition to normal UI ---
    display.setMode(darkMode);  // fills screen + draws static chrome

    // Draw initial dynamic content before audio starts
    buildDisplayState();
    display.updateValues(dispState);
    display.updateOscilloscope(scopeBuf, 300);

    // Audio task pinned to Core 0, priority 2
    xTaskCreatePinnedToCore(
        audioTask,      // task function
        "AudioTask",    // name (debug)
        4096,           // stack in bytes
        nullptr,        // parameter
        2,              // priority (higher than loop's 1)
        nullptr,        // task handle (not needed)
        0               // Core 0
    );
}

// ---------------------------------------------------------------------------
// loop — runs on Core 1 at Arduino default priority (1)
// ---------------------------------------------------------------------------
void loop() {
    static uint32_t lastCtrlMs    = 0;
    static uint32_t lastDisplayMs = 0;

    const uint32_t now = millis();

    // Controls @ 100 Hz (every 10 ms)
    if (now - lastCtrlMs >= 10) {
        lastCtrlMs = now;
        controls.update(ctrlState);
        handleButtons();
        applyControls();
    }

    // Display @ ~30 Hz (every 33 ms)
    if (now - lastDisplayMs >= 33) {
        lastDisplayMs = now;
        buildDisplayState();
        display.updateOscilloscope(scopeBuf, 300);
        display.updateValues(dispState);
    }
}
