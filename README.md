# HWSynth

A hardware synthesizer built on the ESP32-WROOM-32U, featuring real-time DSP audio synthesis, a full UI on an ILI9341 TFT display, and analogue control via ADS1115 ADC chips.

## Hardware

| Component | Part |
|---|---|
| MCU | ESP32-WROOM-32U |
| DAC | GY-PCM5102 (I2S) |
| Display | ILI9341 320×240 TFT (SPI) |
| ADC | 2× ADS1115 (I2C, 0x48 + 0x49) |
| Potentiometers | 8× (volume, pitch, LFO rate/depth, blend, cutoff, resonance, contextual) |
| Buttons | 4× push button (PREV, NEXT, ON/OFF, MUTE) |
| Switch | 1× output selector (line out / repro) |

## Features

- **5 waveforms:** sine, square, saw, triangle, noise
- **Waveform blending** between any two waveforms via pot
- **6 DSP effects:** low-pass filter, delay, chorus, tremolo, bitcrusher, distortion
- **ADSR envelope** with configurable attack and release
- **LFO pitch modulation** — 0.1–10 Hz, ±0.5 octaves
- **Arpeggiator** — UP, DOWN, UP/DOWN, and RANDOM patterns
- **Dark / light UI modes** with NVS persistence across reboots
- **Real-time oscilloscope** display of the current blended waveform
- **Dual-core FreeRTOS** — audio engine pinned to Core 0, UI and controls on Core 1
- **Boot splash screen** with progressive initialisation bar

## Pin Mapping

| Function | GPIO |
|---|---|
| TFT MOSI | 23 |
| TFT MISO | 19 |
| TFT CLK | 18 |
| TFT CS | 5 |
| TFT DC | 2 |
| TFT RST | 4 |
| I2S BCLK | 26 |
| I2S LRCK | 25 |
| I2S DOUT | 22 |
| I2C SDA | 21 |
| I2C SCL | 17 |
| BTN PREV | 32 |
| BTN NEXT | 33 |
| BTN ON/OFF | 13 |
| BTN MUTE | 14 |
| SW JACK | 16 |

## Build

### Requirements

- [PlatformIO](https://platformio.org/) CLI or VS Code extension
- ESP32 Arduino framework (installed automatically by PlatformIO)

### Dependencies

Declared in `platformio.ini` — fetched automatically on first build:

- `adafruit/Adafruit ILI9341`
- `adafruit/Adafruit GFX Library`
- `robtillaart/ADS1X15`

### Compile and flash

```bash
pio run --target upload
```

### Serial monitor

```bash
pio device monitor --baud 115200
```

## Project Structure

```
src/
  main.cpp              — setup(), loop(), FreeRTOS task wiring
  audio/
    synth.h / .cpp      — I2S output, SynthState, ADSR, LFO, arpeggiator
    waveforms.h / .cpp  — 5 waveform generators + blend interpolation
    effects.h / .cpp    — 6-slot DSP effects chain
  ui/
    display.h / .cpp    — ILI9341 UI layout and rendering
  input/
    controls.h / .cpp   — ADS1115 pot reading, button debounce, switch
include/
  pins.h                — GPIO assignments
  config.h              — sample rate, buffer size, pot index mapping
```

## License

MIT
