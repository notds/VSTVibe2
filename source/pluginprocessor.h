#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"
#include <vector>
#include <cmath>

namespace VSTVibe2 {

static const Steinberg::FUID kVSTVibe2ProcessorUID (0xAABBCCDD, 0xEEFF1122, 0x33445566, 0x77889900);
static const Steinberg::FUID kVSTVibe2ControllerUID (0x11223344, 0x55667788, 0x99AABBCC, 0xDDEEFF00);

class VSTVibe2Processor : public Steinberg::Vst::AudioEffect {
public:
    VSTVibe2Processor();
    ~VSTVibe2Processor() override = default;

    // Volume knobs for each oscillator (sine, square, triangle, saw)
    std::vector<float> oscillatorVolumes = {0.25f, 0.25f, 0.25f, 0.25f};

    static Steinberg::FUnknown* createInstance(void*) {
        return (Steinberg::Vst::IAudioProcessor*)new VSTVibe2Processor();
    }

    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) override;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) override;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) override;
    Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup& setup) override;
    Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data) override;
    Steinberg::int32 PLUGIN_API getTailSize() const { return 0; }
    
    // Knob control methods (0-255 range for UI)
    void setOscillatorVolume(int oscIndex, int knobValue) {
        if (oscIndex >= 0 && oscIndex < 4) {
            oscillatorVolumes[oscIndex] = (knobValue / 255.0f);
        }
    }
    
    int getOscillatorVolume(int oscIndex) const {
        if (oscIndex >= 0 && oscIndex < 4) {
            return static_cast<int>(oscillatorVolumes[oscIndex] * 255.0f);
        }
        return 0;
    }

private:
    // Sample rate and phase tracking
    float sampleRate = 44100.0f;
    double phase = 0.0;
    
    // MIDI note state
    float baseFrequency     = 0.0f;
    float currentVelocity   = 1.0f;
    float pitchBendSemitones = 0.0f;
    bool  noteActive         = false;
    

    
    // Oscillator functions
    float generateSineWave(double phase);
    float generateSquareWave(double phase);
    float generateTriangleWave(double phase);
    float generateSawWave(double phase);
    
    // Mix all oscillators
    float mixOscillators(double phase);
};

} // namespace VSTVibe2
