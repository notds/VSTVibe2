#include "pluginview.h"
#include <windows.h>
#include <cstring>

namespace VSTVibe2 {

PluginView::PluginView()
    : Steinberg::CPluginView() {
    viewRect.right = 600;
    viewRect.bottom = 400;
    int moo = rand() % 0x1000000;  // Random color for testing      
    pixelBuffer.resize(600 * 400, moo);  // White: 0xFFFFFFFF
}

PluginView::~PluginView() {
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
            int moo = rand() % 0x1000000;  // Random color for testing      
            pixelBuffer.resize(width * height, moo);
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
    
    // Fill entire buffer with gray background color
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            pixelBuffer[y * width + x] = rand() % 0x1000000;  // Random color for testing         
        }
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
    
    // Create bitmap info - must match the working version exactly
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;  // Negative to flip Y axis
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    // Copy pixel data to the window - using exact same call as working version
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
    
    // Enforce minimum size
    if (width < MIN_WIDTH) {
        viewRect.right = viewRect.left + MIN_WIDTH;
    }
    if (height < MIN_HEIGHT) {
        viewRect.bottom = viewRect.top + MIN_HEIGHT;
    }
    
    // Enforce maximum size
    if (width > MAX_WIDTH) {
        viewRect.right = viewRect.left + MAX_WIDTH;
    }
    if (height > MAX_HEIGHT) {
        viewRect.bottom = viewRect.top + MAX_HEIGHT;
    }
}

} // namespace VSTVibe2
