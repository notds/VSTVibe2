#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "public.sdk/source/vst/vstparameters.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "pluginview.h"

namespace VSTVibe2 {

// Parameter IDs for oscillator volumes
enum ParamID {
    kSineVolumeID = 0,
    kSquareVolumeID = 1,
    kTriangleVolumeID = 2,
    kSawVolumeID = 3,
    kSpiceID      = 4,
    kPitchBendID  = 5,
    kNoteActiveID = 6,
    kSqueezeID    = 7
};

class VSTVibe2Controller : public Steinberg::Vst::EditController,
                           public Steinberg::Vst::IMidiMapping {
public:
    VSTVibe2Controller() = default;
    ~VSTVibe2Controller() override = default;

    static Steinberg::FUnknown* createInstance(void*) {
        return (Steinberg::Vst::IEditController*)new VSTVibe2Controller();
    }

    // EditController overrides
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) override;
    Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream* state) override;
    Steinberg::IPlugView* PLUGIN_API createView(const char* name) override;
    Steinberg::tresult PLUGIN_API setParamNormalized(Steinberg::Vst::ParamID tag, Steinberg::Vst::ParamValue value) override;

    // IMidiMapping — lets the host route MIDI pitch bend to kPitchBendID
    Steinberg::tresult PLUGIN_API getMidiControllerAssignment(
        Steinberg::int32 busIndex, Steinberg::int16 channel,
        Steinberg::Vst::CtrlNumber midiControllerNumber,
        Steinberg::Vst::ParamID& id) override;

    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid, void** obj) override {
        QUERY_INTERFACE(iid, obj, Steinberg::Vst::IMidiMapping::iid, Steinberg::Vst::IMidiMapping)
        return EditController::queryInterface(iid, obj);
    }
    REFCOUNT_METHODS(EditController)

    void setPluginView(PluginView* view) { pluginView = view; }
    PluginView* getPluginView() const { return pluginView; }

private:
    PluginView* pluginView = nullptr;
};

} // namespace VSTVibe2
