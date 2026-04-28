#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "pluginview.h"

namespace VSTVibe2 {

class VSTVibe2Controller : public Steinberg::Vst::EditController {
public:
    VSTVibe2Controller() = default;
    ~VSTVibe2Controller() override = default;

    static Steinberg::FUnknown* createInstance(void*) {
        return (Steinberg::Vst::IEditController*)new VSTVibe2Controller();
    }

    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) override;
    Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream* state) override;
    Steinberg::IPlugView* PLUGIN_API createView(const char* name) override;

    void setPluginView(PluginView* view) { pluginView = view; }

private:
    PluginView* pluginView = nullptr;
};

} // namespace VSTVibe2
