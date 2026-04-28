#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "public.sdk/source/common/pluginview.h"
#include <vector>

namespace VSTVibe2 {

struct Knob {
    int x, y;              // Position
    int size;              // Diameter
    int value;             // 0-255
    const char* label;     // Label text
};

class PluginView : public Steinberg::CPluginView {
public:
    PluginView();
    ~PluginView() override;

    // CPluginView overrides
    Steinberg::tresult PLUGIN_API isPlatformTypeSupported(const char* type) override;
    Steinberg::tresult PLUGIN_API attached(void* parent, const char* type) override;
    Steinberg::tresult PLUGIN_API removed() override;
    Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect* newSize) override;
    Steinberg::tresult PLUGIN_API getSize(Steinberg::ViewRect* size) override;
    Steinberg::tresult PLUGIN_API canResize() override { return Steinberg::kResultTrue; }
    void render();

private:
    static constexpr int MIN_WIDTH = 300;
    static constexpr int MIN_HEIGHT = 200;
    static constexpr int MAX_WIDTH = 2048;
    static constexpr int MAX_HEIGHT = 1536;
    static constexpr int KNOB_SIZE = 60;
    
    Steinberg::ViewRect viewRect{0, 0, 600, 400};
    void* platformWindow = nullptr;
    std::vector<uint32_t> pixelBuffer;
    std::vector<Knob> knobs;
    int draggedKnobIndex = -1;
    
    void drawToWindow();
    void constrainSize();
    void initializeKnobs();
    void drawKnob(const Knob& knob);
    void drawText(const char* text, int x, int y, uint32_t color);
    int getKnobAtPosition(int x, int y) const;
    void updateKnobValue(int knobIndex, int y);
};

} // namespace VSTVibe2
