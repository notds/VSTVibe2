#include "plugincontroller.h"
#include "pluginprocessor.h"

namespace VSTVibe2 {

Steinberg::tresult PLUGIN_API VSTVibe2Controller::initialize(Steinberg::FUnknown* context) {
    Steinberg::tresult result = EditController::initialize(context);
    if (result != Steinberg::kResultOk) {
        return result;
    }

    Steinberg::Vst::Parameter* param;

    param = new Steinberg::Vst::RangeParameter(
        STR16("Sine Volume"), kSineVolumeID, STR16("%"), 0.0, 1.0, 1.0);
    parameters.addParameter(param);

    param = new Steinberg::Vst::RangeParameter(
        STR16("Square Volume"), kSquareVolumeID, STR16("%"), 0.0, 1.0, 1.0);
    parameters.addParameter(param);

    param = new Steinberg::Vst::RangeParameter(
        STR16("Triangle Volume"), kTriangleVolumeID, STR16("%"), 0.0, 1.0, 1.0);
    parameters.addParameter(param);

    param = new Steinberg::Vst::RangeParameter(
        STR16("Saw Volume"), kSawVolumeID, STR16("%"), 0.0, 1.0, 1.0);
    parameters.addParameter(param);

    param = new Steinberg::Vst::RangeParameter(
        STR16("Spice"), kSpiceID, STR16("%"), 0.0, 1.0, 0.25);
    parameters.addParameter(param);

    parameters.addParameter(new Steinberg::Vst::RangeParameter(
        STR16("Squeeze"), kSqueezeID, STR16("%"), 0.0, 1.0, 0.0));

    parameters.addParameter(new Steinberg::Vst::RangeParameter(
        STR16("Glide"), kGlideID, STR16("s"), 0.0, 1.0, 0.0));

    parameters.addParameter(new Steinberg::Vst::RangeParameter(
        STR16("Distortion"), kDistortionID, STR16("%"), 0.0, 1.0, 0.0));

    parameters.addParameter(new Steinberg::Vst::RangeParameter(
        STR16("Note Active"), kNoteActiveID, STR16(""), 0.0, 1.0, 0.0));

    parameters.addParameter(new Steinberg::Vst::RangeParameter(
        STR16("Pitch Bend"), kPitchBendID, STR16("st"), 0.0, 1.0, 0.5));

    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API VSTVibe2Controller::setComponentState(Steinberg::IBStream* /*state*/) {
    return Steinberg::kResultOk;
}

Steinberg::IPlugView* PLUGIN_API VSTVibe2Controller::createView(const char* name) {
    if (name && strcmp(name, "editor") == 0) {
        auto* view = new PluginView();
        view->setController(this);
        setPluginView(view);
        return view;
    }
    return nullptr;
}

Steinberg::tresult PLUGIN_API VSTVibe2Controller::setParamNormalized(Steinberg::Vst::ParamID tag, Steinberg::Vst::ParamValue value) {
    Steinberg::tresult result = EditController::setParamNormalized(tag, value);
    if (result == Steinberg::kResultOk && pluginView) {
        if (tag < 4) {
            VSTVibe2Processor::oscillatorVolumes[tag] = static_cast<float>(value);
            pluginView->setKnobValue(static_cast<int>(tag), static_cast<int>(value * 255.0));
        } else if (tag == kNoteActiveID) {
            pluginView->setNoteActive(value > 0.5);
        } else if (tag == kSpiceID) {
            pluginView->setKnobValue(4, static_cast<int>(value * 255.0));
        } else if (tag == kSqueezeID) {
            pluginView->setKnobValue(5, static_cast<int>(value * 255.0));
        } else if (tag == kGlideID) {
            pluginView->setKnobValue(6, static_cast<int>(value * 255.0));
        } else if (tag == kDistortionID) {
            pluginView->setKnobValue(7, static_cast<int>(value * 255.0));
        }
    }
    return result;
}

Steinberg::tresult PLUGIN_API VSTVibe2Controller::getMidiControllerAssignment(
    Steinberg::int32 /*busIndex*/, Steinberg::int16 /*channel*/,
    Steinberg::Vst::CtrlNumber midiControllerNumber,
    Steinberg::Vst::ParamID& id)
{
    if (midiControllerNumber == Steinberg::Vst::kPitchBend) {
        id = kPitchBendID;
        return Steinberg::kResultTrue;
    }
    return Steinberg::kResultFalse;
}

Steinberg::FUnknown* createControllerInstance(void*) {
    return static_cast<Steinberg::Vst::IEditController*>(new VSTVibe2Controller());
}

} // namespace VSTVibe2
