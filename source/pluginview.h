#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "public.sdk/source/common/pluginview.h"
#include "mandelbrot_shaper.h"
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace VSTVibe2 {

struct Knob {
    int x, y;              // Position (center of knob)
    int size;              // Diameter
    int value;             // 0-255
    const char* label;     // Label text (waveform name)
    int paramID;           // VST3 parameter ID (NOT the knob array index)
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
#ifdef _WIN32
    WNDPROC originalWindowProc = nullptr;
#endif
    Steinberg::Vst::EditController* controller = nullptr;
    std::vector<uint32_t> bgImage;
    int bgWidth  = 0;
    int bgHeight = 0;

    bool   noteActive    = false;
    float  visualNoiseEnv = 0.0f;  // Mirrors audio noiseEnv; decays ~150ms at ~30fps
    double shakePhase    = 0.0;
    int    scrollOffset  = 0;

    std::vector<uint32_t> waveformFeedback;  // Previous frame's waveBuffer (ARGB)
    double waveformHue = 0.0;
    MandelbrotState mandelbrotState;

    // Layered rendering buffers
    std::vector<uint32_t> bgBuffer;    // Cached background (RGB) — repainted on resize only
    std::vector<uint32_t> waveBuffer;  // Waveform layer (ARGB, transparent where undrawn)
    std::vector<uint32_t> knobBuffer;  // Knob layer     (ARGB, transparent where undrawn)
    std::vector<uint32_t> textBuffer;  // Text overlay   (ARGB, rebuilt when knobs change)
    int  waveW = 0, waveH = 0;        // Current waveBuffer dimensions
    int  lastRenderW = 0, lastRenderH = 0;

    bool backgroundDirty = true;
    bool knobLayerDirty  = true;

    static constexpr int FULL_REDRAW_INTERVAL = 900;  // ~30 s at 30 fps
    int fullRedrawCounter = FULL_REDRAW_INTERVAL;

    std::vector<uint32_t> dialImage;
    int    dialWidth  = 0;
    int    dialHeight = 0;
    double dialCx           = 0.0;  // Circle center X in image pixels (user-specified)
    double dialCy           = 0.0;  // Circle center Y in image pixels (user-specified)
    double dialRadius       = 0.0;  // Detected circle radius (image pixels)
    double dialMaxExtent    = 0.0;  // Max distance from center to any opaque pixel
    int    dialScreenExtent = 0;    // Screen pixels needed to draw full image at any rotation

    void drawToWindow();
    void drawTextLayer();
    void constrainSize();
    void initializeKnobs();
    void updateLayout();
    void drawKnob(int knobIndex);
    void drawWaveform();
    void loadBackgroundImage();
    void drawBackground();
    void loadDialImage();
    void drawDialImage(const Knob& knob, int knobIndex);
    int getKnobAtPosition(int x, int y) const;
    void invalidateRect();

#ifdef _WIN32
    friend LRESULT CALLBACK WindowProcStub(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT onWindowMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
#endif
};

} // namespace VSTVibe2
