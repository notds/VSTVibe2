#include "pluginview.h"
#include <windows.h>
#include <cstring>
#include <cmath>
#include <cstdlib>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace VSTVibe2 {

PluginView::PluginView()
    : Steinberg::CPluginView() {
    viewRect.right = 600;
    viewRect.bottom = 400;
    pixelBuffer.resize(600 * 400, 0xFFFFFFFF);
    initializeKnobs();
}

PluginView::~PluginView() {
}

void PluginView::initializeKnobs() {
    knobs.clear();
    knobs.push_back({80, 200, KNOB_SIZE, 128, "Sine"});
    knobs.push_back({180, 200, KNOB_SIZE, 128, "Square"});
    knobs.push_back({280, 200, KNOB_SIZE, 128, "Triangle"});
    knobs.push_back({380, 200, KNOB_SIZE, 128, "Saw"});
}

Steinberg::tresult PLUGIN_API PluginView::isPlatformTypeSupported(const char* type) {
    if (type && strcmp(type, "HWND") == 0) {
        return Steinberg::kResultTrue;
    }
    return Steinberg::kResultFalse;
}

Steinberg::tresult PLUGIN_API PluginView::attached(void* parent, const char* type) {
    if (!parent || !type) {
        return Steinberg::kResultFalse;
    }
    if (strcmp(type, "HWND") != 0) {
        return Steinberg::kResultFalse;
    }
    
    platformWindow = parent;
    render();
    drawToWindow();
    return Steinberg::kResultTrue;
}

Steinberg::tresult PLUGIN_API PluginView::removed() {
    platformWindow = nullptr;
    return Steinberg::kResultTrue;
}

Steinberg::tresult PLUGIN_API PluginView::onSize(Steinberg::ViewRect* newSize) {
    if (newSize) {
        viewRect = *newSize;
        constrainSize();
        int width = viewRect.right - viewRect.left;
        int height = viewRect.bottom - viewRect.top;
        if (width > 0 && height > 0) {
            pixelBuffer.resize(width * height, 0xFFFFFFFF);
            render();
            drawToWindow();
        }
    }
    return Steinberg::kResultTrue;
}

Steinberg::tresult PLUGIN_API PluginView::getSize(Steinberg::ViewRect* size) {
    if (size) {
        *size = viewRect;
        return Steinberg::kResultTrue;
    }
    return Steinberg::kResultFalse;
}

void PluginView::render() {
    int width = viewRect.right - viewRect.left;
    int height = viewRect.bottom - viewRect.top;
    
    if (width <= 0 || height <= 0) {
        return;
    }
    
    // Fill background with white
    for (size_t i = 0; i < pixelBuffer.size(); ++i) {
        pixelBuffer[i] = 0xFFFFFFFF;
    }
    
    // Draw each knob
    for (size_t i = 0; i < knobs.size(); ++i) {
        drawKnob(knobs[i]);
    }
}

void PluginView::drawKnob(const Knob& knob) {
    int cx = knob.x;
    int cy = knob.y;
    int radius = knob.size / 2;
    int width = viewRect.right - viewRect.left;
    int height = viewRect.bottom - viewRect.top;
    
    // Draw knob circle (light gray background)
    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            int dx = x;
            int dy = y;
            if (dx * dx + dy * dy <= radius * radius) {
                int px = cx + x;
                int py = cy + y;
                if (px >= 0 && px < width && py >= 0 && py < height) {
                    pixelBuffer[py * width + px] = 0xFFDDDDDD;
                }
            }
        }
    }
    
    // Draw darker circle border
    for (int angle = 0; angle < 360; ++angle) {
        double rad = angle * M_PI / 180.0;
        int bx = cx + static_cast<int>(radius * std::cos(rad));
        int by = cy + static_cast<int>(radius * std::sin(rad));
        if (bx >= 0 && bx < width && by >= 0 && by < height) {
            pixelBuffer[by * width + bx] = 0xFF888888;
        }
    }
    
    // Draw indicator line based on knob value (0-255 maps to -135 to +135 degrees)
    double angle = (knob.value / 255.0) * (270.0 * M_PI / 180.0) - (135.0 * M_PI / 180.0);
    int lineLength = radius - 10;
    
    for (int i = 0; i < lineLength; ++i) {
        int lx = cx + static_cast<int>((i / static_cast<float>(lineLength)) * lineLength * std::cos(angle));
        int ly = cy + static_cast<int>((i / static_cast<float>(lineLength)) * lineLength * std::sin(angle));
        if (lx >= 0 && lx < width && ly >= 0 && ly < height) {
            pixelBuffer[ly * width + lx] = 0xFF000000;  // Black indicator
        }
    }
}

void PluginView::drawText(const char* text, int x, int y, uint32_t color) {
    // Placeholder for text drawing
    (void)text;
    (void)x;
    (void)y;
    (void)color;
}

int PluginView::getKnobAtPosition(int x, int y) const {
    for (size_t i = 0; i < knobs.size(); ++i) {
        int dx = x - knobs[i].x;
        int dy = y - knobs[i].y;
        int radius = knobs[i].size / 2;
        if (dx * dx + dy * dy <= radius * radius) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void PluginView::updateKnobValue(int knobIndex, int y) {
    if (knobIndex >= 0 && knobIndex < static_cast<int>(knobs.size())) {
        int cy = knobs[knobIndex].y;
        int delta = cy - y;
        int newValue = knobs[knobIndex].value + delta;
        
        if (newValue < 0) newValue = 0;
        if (newValue > 255) newValue = 255;
        
        knobs[knobIndex].value = newValue;
        render();
        drawToWindow();
    }
}

void PluginView::drawToWindow() {
    if (!platformWindow) {
        return;
    }
    
    HWND hwnd = static_cast<HWND>(platformWindow);
    HDC hdc = GetDC(hwnd);
    if (!hdc) {
        return;
    }
    
    int width = viewRect.right - viewRect.left;
    int height = viewRect.bottom - viewRect.top;
    
    if (width <= 0 || height <= 0 || pixelBuffer.empty()) {
        ReleaseDC(hwnd, hdc);
        return;
    }
    
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    SetDIBitsToDevice(
        hdc,
        0, 0, width, height,
        0, 0, 0, height,
        pixelBuffer.data(),
        &bmi,
        DIB_RGB_COLORS
    );
    
    ReleaseDC(hwnd, hdc);
}

void PluginView::constrainSize() {
    int width = viewRect.right - viewRect.left;
    int height = viewRect.bottom - viewRect.top;
    
    if (width < MIN_WIDTH) {
        viewRect.right = viewRect.left + MIN_WIDTH;
    }
    if (height < MIN_HEIGHT) {
        viewRect.bottom = viewRect.top + MIN_HEIGHT;
    }
    
    if (width > MAX_WIDTH) {
        viewRect.right = viewRect.left + MAX_WIDTH;
    }
    if (height > MAX_HEIGHT) {
        viewRect.bottom = viewRect.top + MAX_HEIGHT;
    }
}

} // namespace VSTVibe2
