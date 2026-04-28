#include "pluginview.h"
#include <windows.h>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace Gdiplus;

namespace VSTVibe2 {

static ULONG_PTR gdiplusToken = 0;

PluginView::PluginView()
    : Steinberg::CPluginView() {
    viewRect.right = 600;
    viewRect.bottom = 400;
    pixelBuffer.resize(600 * 400, 0xFFFFFFFF);
    
    // Initialize GDI+
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    
    initializeKnobs();
    loadDialImage();
}

PluginView::~PluginView() {
    Gdiplus::GdiplusShutdown(gdiplusToken);
}

void PluginView::initializeKnobs() {
    knobs.clear();
    knobs.push_back({80, 200, KNOB_SIZE, 128, "Sine"});
    knobs.push_back({180, 200, KNOB_SIZE, 128, "Square"});
    knobs.push_back({280, 200, KNOB_SIZE, 128, "Triangle"});
    knobs.push_back({380, 200, KNOB_SIZE, 128, "Saw"});
}

bool PluginView::loadDialImage() {
    // Try multiple paths to find the dial.png image
    const char* pathsToTry[] = {
        "source\\images\\dial.png",
        "..\\..\\source\\images\\dial.png",
        "..\\..\\..\\source\\images\\dial.png",
        "C:\\code\\C++\\VSTVibe2\\source\\images\\dial.png"
    };
    
    wchar_t imagePath[MAX_PATH];
    Bitmap* pBitmap = nullptr;
    
    for (const char* path : pathsToTry) {
        MultiByteToWideChar(CP_ACP, 0, path, -1, imagePath, MAX_PATH);
        pBitmap = Bitmap::FromFile(imagePath);
        if (pBitmap && pBitmap->GetLastStatus() == Ok) {
            break;  // Success!
        }
        if (pBitmap) {
            delete pBitmap;
            pBitmap = nullptr;
        }
    }
    
    if (!pBitmap || pBitmap->GetLastStatus() != Ok) {
        // Fallback: create a simple circular dial
        dialWidth = 80;
        dialHeight = 80;
        dialImage.resize(dialWidth * dialHeight, 0xFFCCCCCC);
        // Draw a simple circle pattern
        int cx = dialWidth / 2;
        int cy = dialHeight / 2;
        int radius = 35;
        for (int y = 0; y < dialHeight; ++y) {
            for (int x = 0; x < dialWidth; ++x) {
                int dx = x - cx;
                int dy = y - cy;
                if (dx * dx + dy * dy <= radius * radius) {
                    dialImage[y * dialWidth + x] = 0xFF999999;
                }
            }
        }
        return false;
    }
    
    dialWidth = pBitmap->GetWidth();
    dialHeight = pBitmap->GetHeight();
    dialImage.resize(dialWidth * dialHeight);
    
    // Convert bitmap to ARGB format
    for (int y = 0; y < dialHeight; ++y) {
        for (int x = 0; x < dialWidth; ++x) {
            Color pixelColor;
            pBitmap->GetPixel(x, y, &pixelColor);
            dialImage[y * dialWidth + x] = pixelColor.GetValue();
        }
    }
    
    delete pBitmap;
    return true;
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
    
    // Draw each knob with rotated dial image
    for (int i = 0; i < static_cast<int>(knobs.size()); ++i) {
        drawKnob(i);
    }
}

void PluginView::drawKnob(int knobIndex) {
    if (knobIndex < 0 || knobIndex >= static_cast<int>(knobs.size())) {
        return;
    }
    
    const Knob& knob = knobs[knobIndex];
    
    // Draw the rotated dial image
    drawRotatedImage(knob);
}

void PluginView::drawRotatedImage(const Knob& knob) {
    if (dialImage.empty() || dialWidth <= 0 || dialHeight <= 0) {
        return;
    }
    
    int cx = knob.x;
    int cy = knob.y;
    int bufWidth = viewRect.right - viewRect.left;
    int bufHeight = viewRect.bottom - viewRect.top;
    
    // Convert knob value (0-255) to angle (-135 to +135 degrees, 270 degree range)
    double angle = (knob.value / 255.0) * (270.0 * M_PI / 180.0) - (135.0 * M_PI / 180.0);
    double cosA = std::cos(angle);
    double sinA = std::sin(angle);
    
    int radius = knob.size / 2;
    
    // Get dial image center
    int dialCx = dialWidth / 2;
    int dialCy = dialHeight / 2;
    
    // Draw rotated image centered at (cx, cy) with cosine-based center alignment
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            // Apply cosine damping for smoother center positioning
            double distance = std::sqrt(dx * dx + dy * dy);
            if (distance > radius) continue;
            
            // Rotate the point using cosine/sine
            double rotX = dx * cosA - dy * sinA;
            double rotY = dx * sinA + dy * cosA;
            
            // Map to dial image coordinates (centered)
            int imgX = dialCx + static_cast<int>(rotX);
            int imgY = dialCy + static_cast<int>(rotY);
            
            // Check bounds in dial image
            if (imgX >= 0 && imgX < dialWidth && imgY >= 0 && imgY < dialHeight) {
                uint32_t srcPixel = dialImage[imgY * dialWidth + imgX];
                
                // Check if pixel is not fully transparent
                uint32_t alpha = (srcPixel >> 24) & 0xFF;
                if (alpha > 128) {
                    int px = cx + dx;
                    int py = cy + dy;
                    
                    // Draw to buffer if in bounds
                    if (px >= 0 && px < bufWidth && py >= 0 && py < bufHeight) {
                        pixelBuffer[py * bufWidth + px] = srcPixel;
                    }
                }
            }
        }
    }
}

void PluginView::drawText(const char* text, int x, int y, uint32_t color) {
    // Render text directly to pixel buffer using simple bitmap font approach
    // This is a placeholder that will be called during drawToWindow
    // For now, we'll just store the text info and render it via GDI+ on the window
    (void)text;
    (void)x;
    (void)y;
    (void)color;
}

void PluginView::drawTextToWindow(HDC hdc, const char* text, int x, int y, uint32_t color) {
    if (!text) return;
    
    Graphics graphics(hdc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    
    Font font(L"Arial", 9);
    PointF pointF(static_cast<REAL>(x), static_cast<REAL>(y));
    
    // Convert string to wide char
    wchar_t wText[256];
    MultiByteToWideChar(CP_ACP, 0, text, -1, wText, 256);
    
    // Extract color components (assume ARGB format)
    int b = (color >> 0) & 0xFF;
    int g = (color >> 8) & 0xFF;
    int r = (color >> 16) & 0xFF;
    
    SolidBrush brush(Color(255, r, g, b));
    graphics.DrawString(wText, -1, &font, pointF, &brush);
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
    
    // Now render text overlays
    drawTextToWindow(hdc, "VSTVibe2 - Oscillator Amplitudes", 20, 10, 0xFF000000);
    
    for (const auto& knob : knobs) {
        // Draw waveform label
        drawTextToWindow(hdc, knob.label, knob.x - 15, knob.y + 50, 0xFF000000);
        
        // Draw amplitude percentage
        char ampText[16];
        snprintf(ampText, sizeof(ampText), "%d%%", (knob.value * 100) / 255);
        drawTextToWindow(hdc, ampText, knob.x - 12, knob.y + 65, 0xFF000080);
    }
    
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
