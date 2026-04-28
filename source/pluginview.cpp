#include "pluginview.h"
#include <windows.h>
#include <windowsx.h>
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
static PluginView* g_pluginViewInstance = nullptr;

// Global window procedure stub
LRESULT CALLBACK WindowProcStub(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (g_pluginViewInstance) {
        return g_pluginViewInstance->onWindowMessage(hwnd, message, wParam, lParam);
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

PluginView::PluginView()
    : Steinberg::CPluginView() {
    viewRect.right = 600;
    viewRect.bottom = 400;
    pixelBuffer.resize(600 * 400);
    
    // Initialize white background (BGR format)
    for (size_t i = 0; i < pixelBuffer.size(); ++i) {
        pixelBuffer[i] = 0x00FFFFFF;  // White in BGR
    }
    
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
        // Fallback: create a visible dial with gradient
        dialWidth = 80;
        dialHeight = 80;
        dialImage.resize(dialWidth * dialHeight);
        
        int cx = dialWidth / 2;
        int cy = dialHeight / 2;
        int radius = 35;
        
        // Draw circular gradient dial
        for (int y = 0; y < dialHeight; ++y) {
            for (int x = 0; x < dialWidth; ++x) {
                int dx = x - cx;
                int dy = y - cy;
                double dist = std::sqrt(dx * dx + dy * dy);
                
                if (dist <= radius) {
                    // Gray dial body
                    uint8_t gray = static_cast<uint8_t>(200 - (dist / radius) * 50);
                    dialImage[y * dialWidth + x] = (gray << 16) | (gray << 8) | gray;
                } else {
                    // Transparent outside
                    dialImage[y * dialWidth + x] = 0x00000000;
                }
            }
        }
        return false;
    }
    
    dialWidth = pBitmap->GetWidth();
    dialHeight = pBitmap->GetHeight();
    dialImage.resize(dialWidth * dialHeight);
    
    // Convert bitmap to BGR format for Windows DIB
    for (int y = 0; y < dialHeight; ++y) {
        for (int x = 0; x < dialWidth; ++x) {
            Color pixelColor;
            pBitmap->GetPixel(x, y, &pixelColor);
            
            // GDI+ returns ARGB, convert to BGR for SetDIBitsToDevice
            uint32_t argb = pixelColor.GetValue();
            uint32_t a = (argb >> 24) & 0xFF;
            uint32_t r = (argb >> 16) & 0xFF;
            uint32_t g = (argb >>  8) & 0xFF;
            uint32_t b = (argb >>  0) & 0xFF;
            
            // Store as ARGB (we'll handle it in rendering)
            dialImage[y * dialWidth + x] = (a << 24) | (r << 16) | (g << 8) | b;
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
    g_pluginViewInstance = this;
    
    // Subclass the window for mouse event handling
    HWND hwnd = static_cast<HWND>(platformWindow);
    originalWindowProc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)WindowProcStub);
    
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
            pixelBuffer.clear();
            pixelBuffer.resize(width * height, 0x00FFFFFF);  // White in BGR
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
    
    // Fill background with white (BGR format: 0x00FFFFFF)
    for (size_t i = 0; i < pixelBuffer.size(); ++i) {
        pixelBuffer[i] = 0x00FFFFFF;
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
    
    // Draw rotated image centered at (cx, cy)
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            // Check if within circle radius
            double distance = std::sqrt(dx * dx + dy * dy);
            if (distance > radius) continue;
            
            // Rotate the point using cosine/sine (inverse rotation to get source pixel)
            double rotX = dx * cosA - dy * sinA;
            double rotY = dx * sinA + dy * cosA;
            
            // Map to dial image coordinates (centered)
            int imgX = dialCx + static_cast<int>(rotX);
            int imgY = dialCy + static_cast<int>(rotY);
            
            // Check bounds in dial image
            if (imgX >= 0 && imgX < dialWidth && imgY >= 0 && imgY < dialHeight) {
                uint32_t srcPixel = dialImage[imgY * dialWidth + imgX];
                
                // Extract ARGB components from source
                uint32_t alpha = (srcPixel >> 24) & 0xFF;
                
                // Only draw if not fully transparent
                if (alpha > 128) {
                    int px = cx + dx;
                    int py = cy + dy;
                    
                    // Draw to buffer if in bounds
                    if (px >= 0 && px < bufWidth && py >= 0 && py < bufHeight) {
                        // Convert ARGB to BGR for SetDIBitsToDevice
                        uint32_t b = (srcPixel >>  0) & 0xFF;
                        uint32_t g = (srcPixel >>  8) & 0xFF;
                        uint32_t r = (srcPixel >> 16) & 0xFF;
                        uint32_t bgrPixel = (r << 16) | (g << 8) | b;
                        
                        pixelBuffer[py * bufWidth + px] = bgrPixel;
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

void PluginView::invalidateRect() {
    if (platformWindow) {
        HWND hwnd = static_cast<HWND>(platformWindow);
        InvalidateRect(hwnd, nullptr, FALSE);
        UpdateWindow(hwnd);
    }
}

LRESULT PluginView::onWindowMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            draggedKnobIndex = getKnobAtPosition(x, y);
            lastMouseY = y;
            return 0;
        }
        
        case WM_MOUSEMOVE: {
            if (draggedKnobIndex >= 0) {
                int y = GET_Y_LPARAM(lParam);
                
                if (lastMouseY >= 0) {
                    int delta = lastMouseY - y;  // Up motion = positive = increase value
                    
                    int currentValue = knobs[draggedKnobIndex].value;
                    int newValue = currentValue + delta;
                    
                    // Clamp to 0-255
                    if (newValue < 0) newValue = 0;
                    if (newValue > 255) newValue = 255;
                    
                    if (newValue != currentValue) {
                        knobs[draggedKnobIndex].value = newValue;
                        
                        // Render and display immediately
                        render();
                        drawToWindow();
                    }
                }
                
                lastMouseY = y;
            }
            return 0;
        }
        
        case WM_LBUTTONUP: {
            draggedKnobIndex = -1;
            lastMouseY = -1;
            return 0;
        }
        
        case WM_MOUSEWHEEL: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            
            // Convert screen coordinates to client coordinates
            POINT pt = {x, y};
            ScreenToClient(hwnd, &pt);
            
            int knobIndex = getKnobAtPosition(pt.x, pt.y);
            if (knobIndex >= 0) {
                int wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
                int delta = (wheelDelta > 0) ? 5 : -5;
                
                int newValue = knobs[knobIndex].value + delta;
                if (newValue < 0) newValue = 0;
                if (newValue > 255) newValue = 255;
                
                knobs[knobIndex].value = newValue;
                render();
                drawToWindow();
            }
            return 0;
        }
        
        case WM_PAINT: {
            render();
            drawToWindow();
            ValidateRect(hwnd, nullptr);
            return 0;
        }
    }
    
    // Call original window procedure for unhandled messages
    if (originalWindowProc) {
        return CallWindowProc(originalWindowProc, hwnd, message, wParam, lParam);
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
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

} // namespace VSTVibe2
