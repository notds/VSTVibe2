# VSTVibe

A minimal VST3 instrument plugin that accepts MIDI note input and generates a mono saw wave in the correct pitch.

## Build

1. Download the Steinberg VST3 SDK and set `VST3_SDK_PATH` to its root directory.
2. Generate the build files and compile:

```powershell
mkdir build
cd build
cmake -DVST3_SDK_PATH="C:/Path/To/VST3_SDK" ..
cmake --build . --config Release
```

3. The plugin binary will be produced as `VSTVibe.vst3`.

## Notes

- This plugin is a monophonic synthesizer.
- It uses MIDI note-on/note-off events to trigger a saw wave.
- If you want a GUI, extend `plugincontroller.cpp` and add a custom view.
