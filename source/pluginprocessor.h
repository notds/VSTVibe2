#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "mandelbrot_shaper.h"
#include <vector>
#include <cmath>

namespace VSTVibe2 {

static const Steinberg::FUID kVSTVibe2ProcessorUID (0xAABBCCDD, 0xEEFF1122, 0x33445566, 0x77889900);
static const Steinberg::FUID kVSTVibe2ControllerUID (0x11223344, 0x55667788, 0x99AABBCC, 0xDDEEFF00);

class VSTVibe2Processor : public Steinberg::Vst::AudioEffect {
public:
    VSTVibe2Processor();
    ~VSTVibe2Processor() override = default;

    inline static std::vector<float> oscillatorVolumes = {1.0f, 1.0f, 1.0f, 1.0f};

    static Steinberg::FUnknown* createInstance(void*) {
        return (Steinberg::Vst::IAudioProcessor*)new VSTVibe2Processor();
    }

    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) override;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) override;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) override;
    Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup& setup) override;
    Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data) override;
    Steinberg::int32 PLUGIN_API getTailSize() const { return 0; }

private:
    struct HeldNote {
        int   pitch;
        float frequency;
        float velocity;
    };

    std::vector<HeldNote> heldNotes;

    float sampleRate         = 44100.0f;
    double phase             = 0.0;

    float currentFrequency   = 0.0f;
    float targetFrequency    = 0.0f;
    float portamentoRatio    = 1.0f;
    int   portamentoSamples  = 0;
    float currentVelocity    = 1.0f;
    float pitchBendSemitones = 0.0f;
    bool  noteActive         = false;
    float spice              = 0.25f;
    float squeeze            = 0.0f;
    float glide              = 0.0f;
    float distortion         = 0.0f;
    float compressorEnv      = 0.0f;
    MandelbrotState mandelbrotState;

    float generateSineWave(double phase);
    float generateSquareWave(double phase);
    float generateTriangleWave(double phase);
    float generateSawWave(double phase);
    float mixOscillators(double phase);
};

} // namespace VSTVibe2
