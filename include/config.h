#pragma once
// Project-wide compile-time constants: audio parameters and pot index mapping.

#define SAMPLE_RATE    44100
#define BUFFER_SIZE      256  // I2S DMA buffer in samples; ~5.8 ms at 44100 Hz
#define NUM_WAVEFORMS      5
#define NUM_EFFECTS        7
#define NUM_POTS           8

// ADS1115 I2C addresses
#define ADS1_ADDR       0x48  // ADDR pin to GND
#define ADS2_ADDR       0x49  // ADDR pin to VCC

// Pot index mapping (0-7)
// ADS1115 #1 (0x48): channels 0-3
#define POT_VOLUME      0
#define POT_PITCH       1
#define POT_LFO_RATE    2
#define POT_LFO_DEPTH   3
// ADS1115 #2 (0x49): channels 0-3 mapped to indices 4-7
#define POT_BLEND       4
#define POT_CUTOFF      5
#define POT_RESONANCE   6
#define POT_CONTEXTUAL  7
