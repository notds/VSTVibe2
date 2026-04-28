#include "plugincontroller.h"

namespace VSTVibe2 {

Steinberg::tresult PLUGIN_API VSTVibe2Controller::initialize(Steinberg::FUnknown* context) {
    Steinberg::tresult result = EditController::initialize(context);
    if (result != Steinberg::kResultOk) {
        return result;
    }

    // Add parameters for oscillator volumes (0.0 - 1.0, default 0.25)
    Steinberg::Vst::Parameter* param;
    
    param = new Steinberg::Vst::RangeParameter(
        STR16("Sine Volume"),
        kSineVolumeID,
        STR16("%"),
        0.0, 1.0, 0.25
    );
    parameters.addParameter(param);
    
    param = new Steinberg::Vst::RangeParameter(
        STR16("Square Volume"),
        kSquareVolumeID,
        STR16("%"),
        0.0, 1.0, 0.25
    );
    parameters.addParameter(param);
    
    param = new Steinberg::Vst::RangeParameter(
        STR16("Triangle Volume"),
        kTriangleVolumeID,
        STR16("%"),
        0.0, 1.0, 0.25
    );
    parameters.addParameter(param);
    
    param = new Steinberg::Vst::RangeParameter(
        STR16("Saw Volume"),
        kSawVolumeID,
        STR16("%"),
        0.0, 1.0, 0.25
    );
    parameters.addParameter(param);
    
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API VSTVibe2Controller::setComponentState(Steinberg::IBStream* /*state*/) {
    return Steinberg::kResultOk;
}

Steinberg::IPlugView* PLUGIN_API VSTVibe2Controller::createView(const char* name) {
    if (name && strcmp(name, "editor") == 0) {
        auto* view = new PluginView();
        setPluginView(view);
        return view;
    }
    return nullptr;
}

// Public factory functions
Steinberg::FUnknown* createControllerInstance(void*) {
    return static_cast<Steinberg::Vst::IEditController*>(new VSTVibe2Controller());
}

} // namespace VSTVibe2
