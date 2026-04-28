#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "public.sdk/source/common/pluginview.h"
#include <vector>
#include <windows.h>

namespace VSTVibe2 {

struct Knob {
    int x, y;              // Position (center of knob)
    int size;              // Diameter
    int value;             // 0-255
    const char* label;     // Label text (waveform name)
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
    static constexpr int KNOB_SIZE = 80;
    
    Steinberg::ViewRect viewRect{0, 0, 600, 400};
    void* platformWindow = nullptr;
    std::vector<uint32_t> pixelBuffer;
    std::vector<Knob> knobs;
    int draggedKnobIndex = -1;
    int lastMouseY = -1;
    std::vector<uint32_t> dialImage;
    int dialWidth = 0;
    int dialHeight = 0;
    WNDPROC originalWindowProc = nullptr;
    
    void drawToWindow();
    void drawTextToWindow(HDC hdc, const char* text, int x, int y, uint32_t color);
    void constrainSize();
    void initializeKnobs();
    void drawKnob(int knobIndex);
    void drawRotatedImage(const Knob& knob);
    void drawText(const char* text, int x, int y, uint32_t color);
    int getKnobAtPosition(int x, int y) const;
    void updateKnobValue(int knobIndex, int y);
    bool loadDialImage();
    void invalidateRect();
    
    // Friend function for window procedure
    friend LRESULT CALLBACK WindowProcStub(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT onWindowMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
};

} // namespace VSTVibe2
