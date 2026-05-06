# MONODUCK

A monophonic VST3 synthesizer instrument with a custom GDI+ GUI. Accepts MIDI input and generates audio from four oscillator waveforms with a Mandelbrot waveshaper, compressor, foldback distortion, and portamento glide.

## Features

**Oscillators** — mix of four waveforms, each with its own volume knob:
- Sine, Square, Triangle, Saw

**FX chain** (in order):
- **Spice** — Mandelbrot waveshaper; adds harmonic complexity
- **Squeeze** — feedforward peak compressor (threshold −18 dBFS, up to 20:1 ratio, auto makeup gain)
- **Glide** — portamento between notes (up to 2 s)
- **Distortion** — foldback distortion; above 50% adds subtle white noise (~4% max)

**MIDI:**
- Monophonic with note priority (last-note held)
- Pitch bend (±12 semitones)

**GUI:**
- Animated waveform display with scale-expansion feedback trail
- Rotary dial knobs with label + percentage readout
- Resizable window (520×auto minimum)

## Build

Requires the [Steinberg VST3 SDK](https://github.com/steinbergmedia/vst3sdk).

```powershell
mkdir build
cd build
cmake -DVST3_SDK_PATH="C:/Path/To/vst3sdk" ..
cmake --build . --config Release
```

The plugin is output as `MONODUCK.vst3` in `build/Release/`.

To install, copy `MONODUCK.vst3` to your DAW's VST3 folder (e.g. `C:\Program Files\Common Files\VST3\`).

## Parameters

| Parameter   | Range  | Default | Description                          |
|-------------|--------|---------|--------------------------------------|
| Sine Volume | 0–100% | 100%    | Sine oscillator level                |
| Square Vol  | 0–100% | 100%    | Square oscillator level              |
| Triangle Vol| 0–100% | 100%    | Triangle oscillator level            |
| Saw Volume  | 0–100% | 100%    | Saw oscillator level                 |
| Spice       | 0–100% | 25%     | Mandelbrot waveshaper amount         |
| Squeeze     | 0–100% | 0%      | Compressor amount                    |
| Glide       | 0–100% | 0%      | Portamento time (0–2 s)              |
| Distortion  | 0–100% | 0%      | Foldback distortion amount           |
| Pitch Bend  | ±12 st | center  | Mapped to MIDI pitch bend wheel      |
