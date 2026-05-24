#pragma once
// Waveform declarations: 5 waveform generators and blend interpolation helper.

#include <Arduino.h>
#include "config.h"

// Waveform indices — must match NUM_WAVEFORMS (5)
#define WAVE_SINE     0
#define WAVE_SQUARE   1
#define WAVE_SAW      2
#define WAVE_TRIANGLE 3
#define WAVE_NOISE    4

void  initWaveforms();

// phase: 0.0 – 1.0, returns sample: -1.0 – 1.0
float waveSine    (float phase);
float waveSquare  (float phase);
float waveSaw     (float phase);
float waveTriangle(float phase);
float waveNoise   ();

// Returns sample for the given waveform index and phase
float getWaveformSample(uint8_t waveform, float phase);

// Interpolates between two waveforms by blend (0.0 = all a, 1.0 = all b)
float blendWaveforms(uint8_t waveformA, uint8_t waveformB, float phase, float blend);
