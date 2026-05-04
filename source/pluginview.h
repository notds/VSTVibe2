#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "public.sdk/source/common/pluginview.h"
#include "mandelbrot_shaper.h"
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
    void setController(Steinberg::Vst::EditController* ctrl) { controller = ctrl; }
    void setKnobValue(int index, int value);
    void setNoteActive(bool active) { noteActive = active; }

private:
    static constexpr int MIN_WIDTH  = 520;
    static constexpr int MIN_HEIGHT = 380;
    static constexpr int MAX_WIDTH  = 2048;
    static constexpr int MAX_HEIGHT = 1536;
    static constexpr int KNOB_SIZE  = 50;
    
    Steinberg::ViewRect viewRect{0, 0, 600, 400};
    void* platformWindow = nullptr;
    std::vector<uint32_t> pixelBuffer;
    std::vector<Knob> knobs;
    int draggedKnobIndex = -1;
    int lastMouseY = -1;
    WNDPROC originalWindowProc = nullptr;
    Steinberg::Vst::EditController* controller = nullptr;
    std::vector<uint32_t> bgImage;
    int bgWidth  = 0;
    int bgHeight = 0;

    bool   noteActive    = false;
    double shakePhase    = 0.0;
    int    scrollOffset  = 0;

    std::vector<uint32_t> waveformFeedback;
    int    wfFeedbackW = 0;
    int    wfFeedbackH = 0;
    double waveformHue = 0.0;  // 0-360, cycles through bright colors
    MandelbrotState mandelbrotState;

    std::vector<uint32_t> dialImage;
    int    dialWidth  = 0;
    int    dialHeight = 0;
    double dialCx           = 0.0;  // Circle center X in image pixels (user-specified)
    double dialCy           = 0.0;  // Circle center Y in image pixels (user-specified)
    double dialRadius       = 0.0;  // Detected circle radius (image pixels)
    double dialMaxExtent    = 0.0;  // Max distance from center to any opaque pixel
    int    dialScreenExtent = 0;    // Screen pixels needed to draw full image at any rotation

    void drawToWindow();
    void drawTextToWindow(HDC hdc, const char* text, int x, int y, uint32_t color);
    void constrainSize();
    void initializeKnobs();
    void updateLayout();
    void drawKnob(int knobIndex);
    void drawWaveform();
    void loadBackgroundImage();
    void drawBackground();
    void loadDialImage();
    void drawDialImage(const Knob& knob);
    int getKnobAtPosition(int x, int y) const;
    void updateKnobValue(int knobIndex, int y);
    void invalidateRect();
    
    // Friend function for window procedure
    friend LRESULT CALLBACK WindowProcStub(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT onWindowMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
};

} // namespace VSTVibe2
