#include "plugincontroller.h"

namespace VSTVibe2 {

Steinberg::tresult PLUGIN_API VSTVibe2Controller::initialize(Steinberg::FUnknown* context) {
    return EditController::initialize(context);
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
