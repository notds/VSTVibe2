#include "pluginprocessor.h"
#include "mandelbrot_shaper.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace VSTVibe2 {

VSTVibe2Processor::VSTVibe2Processor() {
    setControllerClass(kVSTVibe2ControllerUID);
}

Steinberg::tresult PLUGIN_API VSTVibe2Processor::initialize(Steinberg::FUnknown* context) {
    Steinberg::tresult result = AudioEffect::initialize(context);
    if (result != Steinberg::kResultOk) {
        return result;
    }

    addEventInput(STR16("MIDI Input"), 1);
    addAudioOutput(STR16("Stereo Out"), Steinberg::Vst::SpeakerArr::kStereo);
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API VSTVibe2Processor::setState(Steinberg::IBStream* /*state*/) {
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API VSTVibe2Processor::getState(Steinberg::IBStream* /*state*/) {
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API VSTVibe2Processor::setupProcessing(Steinberg::Vst::ProcessSetup& setup) {
    sampleRate = setup.sampleRate;
    return AudioEffect::setupProcessing(setup);
}

float VSTVibe2Processor::generateSineWave(double phaseVal) {
    return static_cast<float>(std::sin(2.0 * M_PI * phaseVal));
}    
  

float VSTVibe2Processor::generateSquareWave(double phaseVal) {
    // Normalize phase to [0, 1)
    double normalizedPhase = phaseVal - std::floor(phaseVal);
    return (normalizedPhase < 0.5) ? 1.0f : -1.0f;
}

float VSTVibe2Processor::generateTriangleWave(double phaseVal) {
    // Normalize phase to [0, 1)
    double normalizedPhase = phaseVal - std::floor(phaseVal);
    if (normalizedPhase < 0.25) {
        return -1.0f + 4.0f * normalizedPhase;
    } else if (normalizedPhase < 0.75) {
        return 1.0f - 4.0f * (normalizedPhase - 0.25f);
    } else {
        return -1.0f + 4.0f * (normalizedPhase - 0.75f);
    }
}

float VSTVibe2Processor::generateSawWave(double phaseVal) {
    // Normalize phase to [0, 1)
    double normalizedPhase = phaseVal - std::floor(phaseVal);
    return -1.0f + 2.0f * normalizedPhase;
}

// Feedforward peak compressor. atkCoeff/relCoeff are precomputed per block.
// ratio scales 1:1 → 20:1 with squeeze; threshold fixed at -18 dBFS.
// Auto makeup gain (+9 dB max) keeps perceived loudness stable.
static float applyCompressor(float sample, float squeeze,
                              float& env, float atkCoeff, float relCoeff) {
    if (squeeze < 1e-4f) return sample;

    const float level = std::abs(sample);
    env = (level > env)
        ? atkCoeff * env + (1.0f - atkCoeff) * level
        : relCoeff * env + (1.0f - relCoeff) * level;

    const float thresholdDb     = -18.0f;
    const float ratio           = 1.0f + squeeze * 19.0f;
    const float levelDb         = 20.0f * std::log10(env + 1e-7f);
    const float overDb          = std::max(0.0f, levelDb - thresholdDb);
    const float gainReductionDb = overDb * (1.0f - 1.0f / ratio);
    const float makeupDb        = squeeze * 9.0f;

    return sample * std::pow(10.0f, (-gainReductionDb + makeupDb) / 20.0f);
}

float VSTVibe2Processor::mixOscillators(double phaseVal) {
    float sine = generateSineWave(phaseVal) * oscillatorVolumes[0];
    float square = generateSquareWave(phaseVal) * oscillatorVolumes[1];
    float triangle = generateTriangleWave(phaseVal) * oscillatorVolumes[2];
    float saw = generateSawWave(phaseVal) * oscillatorVolumes[3];
    
    // Mix all oscillators and normalize
    return (sine + square + triangle + saw) * 0.25f;
}

Steinberg::tresult PLUGIN_API VSTVibe2Processor::process(Steinberg::Vst::ProcessData& data) {
    // Handle parameter changes from the UI
    if (data.inputParameterChanges) {
        Steinberg::int32 numParams = data.inputParameterChanges->getParameterCount();
        for (Steinberg::int32 i = 0; i < numParams; ++i) {
            Steinberg::Vst::IParamValueQueue* queue = data.inputParameterChanges->getParameterData(i);
            if (!queue) continue;
            Steinberg::Vst::ParamID id = queue->getParameterId();
            Steinberg::int32 numPoints = queue->getPointCount();
            if (numPoints == 0) continue;
            Steinberg::int32 sampleOffset;
            Steinberg::Vst::ParamValue value;
            if (queue->getPoint(numPoints - 1, sampleOffset, value) != Steinberg::kResultOk) continue;

            if (id < static_cast<Steinberg::Vst::ParamID>(oscillatorVolumes.size())) {
                oscillatorVolumes[id] = static_cast<float>(value);
            } else if (id == 4) {
                spice = static_cast<float>(value);
            } else if (id == 7) {
                squeeze = static_cast<float>(value);
            } else if (id == 5) {
                // normalized 0–1 → semitones -12 to +12; 0.5 = no bend
                pitchBendSemitones = (static_cast<float>(value) - 0.5f) * 24.0f;
            }
        }
    }

    // Handle MIDI events
    bool noteStateChanged = false;
    if (data.inputEvents) {
        Steinberg::int32 eventCount = data.inputEvents->getEventCount();
        for (Steinberg::int32 i = 0; i < eventCount; ++i) {
            Steinberg::Vst::Event event;
            if (data.inputEvents->getEvent(i, event) != Steinberg::kResultOk) {
                continue;
            }

            if (event.type == Steinberg::Vst::Event::kNoteOnEvent) {
                int midiNote = event.noteOn.pitch;
                baseFrequency = 440.0f * std::pow(2.0f, (midiNote - 105) / 12.0f);
                currentVelocity = event.noteOn.velocity;
                if (!noteActive) { noteActive = true; noteStateChanged = true; }
            } else if (event.type == Steinberg::Vst::Event::kNoteOffEvent) {
                if (noteActive) { noteActive = false; noteStateChanged = true; }
            }
        }
    }

    // Notify controller of note active state changes (param ID 6 = kNoteActiveID)
    if (noteStateChanged && data.outputParameterChanges) {
        Steinberg::int32 index;
        auto* queue = data.outputParameterChanges->addParameterData(6, index);
        if (queue) {
            Steinberg::int32 pointIndex;
            queue->addPoint(0, noteActive ? 1.0 : 0.0, pointIndex);
        }
    }

    if (data.numOutputs == 0) {
        return Steinberg::kResultOk;
    }

    Steinberg::Vst::AudioBusBuffers& output = data.outputs[0];
    float** channels = output.channelBuffers32;
    if (!channels) {
        return Steinberg::kResultOk;
    }

    Steinberg::int32 numSamples = data.numSamples;
    Steinberg::int32 numChannels = output.numChannels;

    // Apply pitch bend once per block
    const float effectiveFrequency = baseFrequency * std::pow(2.0f, pitchBendSemitones / 12.0f);

    // Compressor time constants — computed once per block (sampleRate is stable)
    const float cAtk = std::exp(-1.0f / (sampleRate * 0.010f));  // 10 ms attack
    const float cRel = std::exp(-1.0f / (sampleRate * 0.150f));  // 150 ms release

    // Generate audio
    for (Steinberg::int32 sample = 0; sample < numSamples; ++sample) {
        float sampleValue = 0.0f;

        if (noteActive) {
            sampleValue = mixOscillators(phase) * currentVelocity;
            sampleValue = applyMandelbrot(sampleValue, spice, mandelbrotState);
            phase += effectiveFrequency / sampleRate;
            if (phase >= 1.0) phase -= 1.0;
        }

        // Compressor runs on every sample (including silence) so the envelope
        // decays naturally between notes
        sampleValue = applyCompressor(sampleValue, squeeze, compressorEnv, cAtk, cRel);

        for (Steinberg::int32 channel = 0; channel < numChannels; ++channel) {
            if (channels[channel])
                channels[channel][sample] = sampleValue;
        }
    }

    return Steinberg::kResultOk;
}

// Public factory functions
Steinberg::FUnknown* createProcessorInstance(void*) {
    return static_cast<Steinberg::Vst::IAudioProcessor*>(new VSTVibe2Processor());
}

} // namespace VSTVibe2
