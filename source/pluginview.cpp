#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "pluginview.h"
#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <unordered_map>
#include "dial_image_data.h"
#include "bg_image_data.h"
#include <gdiplus.h>


#pragma comment(lib, "gdiplus.lib")

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace Gdiplus;

namespace VSTVibe2 {

static ULONG_PTR gdiplusToken    = 0;
static int        gdiplusRefCount = 0;

// Maps each host HWND to its PluginView — avoids touching GWLP_USERDATA which the host may own
static std::unordered_map<HWND, PluginView*> g_hwndToView;

LRESULT CALLBACK WindowProcStub(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto it = g_hwndToView.find(hwnd);
    if (it != g_hwndToView.end())
        return it->second->onWindowMessage(hwnd, message, wParam, lParam);
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
    
    // Initialize GDI+ once per process; ref-count across instances
    if (gdiplusRefCount++ == 0) {
        GdiplusStartupInput gdiplusStartupInput;
        GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    }
    
    initializeKnobs();
    loadBackgroundImage();
    loadDialImage();
}

PluginView::~PluginView() {
    if (--gdiplusRefCount == 0) {
        Gdiplus::GdiplusShutdown(gdiplusToken);
        gdiplusToken = 0;
    }
}

void PluginView::initializeKnobs() {
    knobs.clear();
    knobs.push_back({80,  200, KNOB_SIZE, 128, "Sine"});
    knobs.push_back({180, 200, KNOB_SIZE, 128, "Square"});
    knobs.push_back({280, 200, KNOB_SIZE, 128, "Triangle"});
    knobs.push_back({380, 200, KNOB_SIZE, 128, "Saw"});
}

void PluginView::loadBackgroundImage() {
    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, kBgPngSize);
    if (!hGlobal) return;

    void* pBuf = GlobalLock(hGlobal);
    if (!pBuf) { GlobalFree(hGlobal); return; }
    memcpy(pBuf, kBgPngData, kBgPngSize);
    GlobalUnlock(hGlobal);

    IStream* pStream = nullptr;
    if (CreateStreamOnHGlobal(hGlobal, TRUE, &pStream) != S_OK || !pStream) {
        GlobalFree(hGlobal);
        return;
    }

    Bitmap* pBitmap = Bitmap::FromStream(pStream);
    pStream->Release();

    if (!pBitmap || pBitmap->GetLastStatus() != Ok) {
        delete pBitmap;
        return;
    }

    bgWidth  = static_cast<int>(pBitmap->GetWidth());
    bgHeight = static_cast<int>(pBitmap->GetHeight());
    bgImage.resize(bgWidth * bgHeight);

    for (int y = 0; y < bgHeight; ++y) {
        for (int x = 0; x < bgWidth; ++x) {
            Color c;
            pBitmap->GetPixel(x, y, &c);
            bgImage[y * bgWidth + x] = c.GetValue();  // ARGB
        }
    }
    delete pBitmap;
}

void PluginView::drawBackground() {
    if (bgImage.empty() || bgWidth <= 0 || bgHeight <= 0) return;

    const int winW = viewRect.right  - viewRect.left;
    const int winH = viewRect.bottom - viewRect.top;
    if (winW <= 0 || winH <= 0) return;

    // Cover scale: fill the window maintaining aspect ratio, center the crop
    const double scaleX = static_cast<double>(winW) / bgWidth;
    const double scaleY = static_cast<double>(winH) / bgHeight;
    const double scale  = std::max(scaleX, scaleY);  // cover: no letterbox bars

    const double scaledW = bgWidth  * scale;
    const double scaledH = bgHeight * scale;
    const double offX    = (winW - scaledW) * 0.5;
    const double offY    = (winH - scaledH) * 0.5;

    for (int py = 0; py < winH; ++py) {
        const double srcYf = (py - offY) / scale;
        const int    srcY  = static_cast<int>(srcYf);
        if (srcY < 0 || srcY >= bgHeight) continue;

        for (int px = 0; px < winW; ++px) {
            const double srcXf = (px - offX) / scale;
            const int    srcX  = static_cast<int>(srcXf);
            if (srcX < 0 || srcX >= bgWidth) continue;

            const uint32_t argb = bgImage[srcY * bgWidth + srcX];
            const uint32_t r = (argb >> 16) & 0xFF;
            const uint32_t g = (argb >>  8) & 0xFF;
            const uint32_t b = (argb >>  0) & 0xFF;
            pixelBuffer[py * winW + px] = (r << 16) | (g << 8) | b;
        }
    }
}

void PluginView::loadDialImage() {
    // Load PNG from embedded byte array via a GDI+ memory stream
    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, kDialPngSize);
    if (!hGlobal) return;

    void* pBuf = GlobalLock(hGlobal);
    if (!pBuf) { GlobalFree(hGlobal); return; }
    memcpy(pBuf, kDialPngData, kDialPngSize);
    GlobalUnlock(hGlobal);

    IStream* pStream = nullptr;
    if (CreateStreamOnHGlobal(hGlobal, TRUE, &pStream) != S_OK || !pStream) {
        GlobalFree(hGlobal);
        return;
    }

    Bitmap* pBitmap = Bitmap::FromStream(pStream);
    pStream->Release();  // Also frees hGlobal (fDeleteOnRelease=TRUE)

    if (!pBitmap || pBitmap->GetLastStatus() != Ok) {
        delete pBitmap;
        return;
    }

    dialWidth  = static_cast<int>(pBitmap->GetWidth());
    dialHeight = static_cast<int>(pBitmap->GetHeight());
    dialImage.resize(dialWidth * dialHeight);

    for (int y = 0; y < dialHeight; ++y) {
        for (int x = 0; x < dialWidth; ++x) {
            Color c;
            pBitmap->GetPixel(x, y, &c);
            dialImage[y * dialWidth + x] = c.GetValue();  // ARGB
        }
    }
    delete pBitmap;

    // ---- Circle geometry ----

    // 1. Use user-specified center (avoids centroid drift from protruding shapes)
    dialCx = 379.0;
    dialCy = 281.0;

    // 2. Cast 360 rays from the known center to find the main circle radius.
    //    Protruding shapes (pointer tab) only affect a small arc; median is robust.
    const int    numRays = 360;
    const double maxR    = std::sqrt(static_cast<double>(dialWidth  * dialWidth +
                                                         dialHeight * dialHeight));
    std::vector<double> rayRadii;
    rayRadii.reserve(numRays);

    for (int i = 0; i < numRays; ++i) {
        double rayAngle = i * 2.0 * M_PI / numRays;
        double cosR = std::cos(rayAngle);
        double sinR = std::sin(rayAngle);

        double lastOpaque = 0.0;
        for (double r = 1.0; r < maxR; r += 0.5) {
            int px = static_cast<int>(dialCx + cosR * r + 0.5);
            int py = static_cast<int>(dialCy + sinR * r + 0.5);
            if (px < 0 || px >= dialWidth || py < 0 || py >= dialHeight) break;
            if (((dialImage[py * dialWidth + px] >> 24) & 0xFF) > 64)
                lastOpaque = r;
            else
                break;
        }
        if (lastOpaque > 0.0)
            rayRadii.push_back(lastOpaque);
    }

    // Median ray-exit distance = circle edge radius
    if (!rayRadii.empty()) {
        std::sort(rayRadii.begin(), rayRadii.end());
        dialRadius = rayRadii[rayRadii.size() / 2];
    } else {
        dialRadius = std::min(dialWidth, dialHeight) / 2.0;
    }

    // 3. Max opaque extent from center → how far we must draw to avoid clipping
    //    at any rotation angle (includes pointer protrusion)
    dialMaxExtent = 0.0;
    for (int y = 0; y < dialHeight; ++y) {
        for (int x = 0; x < dialWidth; ++x) {
            if (((dialImage[y * dialWidth + x] >> 24) & 0xFF) > 16) {
                double ddx = x - dialCx;
                double ddy = y - dialCy;
                dialMaxExtent = std::max(dialMaxExtent, std::sqrt(ddx * ddx + ddy * ddy));
            }
        }
    }
    if (dialMaxExtent < dialRadius) dialMaxExtent = dialRadius;

    // Pre-compute screen draw extent (pixels) using KNOB_SIZE so it's always valid
    if (dialRadius > 0.0)
        dialScreenExtent = static_cast<int>(
            std::ceil(dialMaxExtent * (KNOB_SIZE / 2.0) / dialRadius));
    else
        dialScreenExtent = KNOB_SIZE / 2;
}

void PluginView::drawDialImage(const Knob& knob) {
    if (dialImage.empty() || dialWidth <= 0 || dialHeight <= 0 || dialRadius <= 0.0) return;

    const int cx = knob.x;
    const int cy = knob.y;
    const int radius = knob.size / 2;
    const int bufWidth  = viewRect.right  - viewRect.left;
    const int bufHeight = viewRect.bottom - viewRect.top;

    // Knob value (0-255) → angle over 270° range, starting at -135°, +50° clockwise offset
    const double angle = (knob.value / 255.0) * (270.0 * M_PI / 180.0) - (135.0 * M_PI / 180.0) + (50.0 * M_PI / 180.0);
    const double cosA  =  std::cos(angle);
    const double sinA  =  std::sin(angle);

    // Uniform scale: circle radius in image → knob radius on screen (no X/Y distortion)
    const double scale = dialRadius / static_cast<double>(radius);

    // Extend draw area to fit full image at any rotation (includes pointer protrusion)
    const int drawExtent = (dialScreenExtent > 0) ? dialScreenExtent : radius;

    for (int dy = -drawExtent; dy <= drawExtent; ++dy) {
        for (int dx = -drawExtent; dx <= drawExtent; ++dx) {
            // Inverse rotation so the image appears rotated by +angle
            const double srcDx =  dx * cosA + dy * sinA;
            const double srcDy = -dx * sinA + dy * cosA;

            // Map screen offset → image pixel using detected circle center
            const int imgX = static_cast<int>(dialCx + srcDx * scale);
            const int imgY = static_cast<int>(dialCy + srcDy * scale);

            if (imgX < 0 || imgX >= dialWidth || imgY < 0 || imgY >= dialHeight) continue;

            const uint32_t argb  = dialImage[imgY * dialWidth + imgX];
            const uint32_t alpha = (argb >> 24) & 0xFF;
            if (alpha < 16) continue;  // Skip near-transparent pixels

            const int px = cx + dx;
            const int py = cy + dy;
            if (px < 0 || px >= bufWidth || py < 0 || py >= bufHeight) continue;

            // Convert ARGB → pixel-buffer format (0x00RRGGBB)
            const uint32_t r = (argb >> 16) & 0xFF;
            const uint32_t g = (argb >>  8) & 0xFF;
            const uint32_t b = (argb >>  0) & 0xFF;

            if (alpha >= 240) {
                // Fully opaque — write directly
                pixelBuffer[py * bufWidth + px] = (r << 16) | (g << 8) | b;
            } else {
                // Alpha-blend over existing pixel
                const uint32_t dst = pixelBuffer[py * bufWidth + px];
                const uint32_t dstR = (dst >> 16) & 0xFF;
                const uint32_t dstG = (dst >>  8) & 0xFF;
                const uint32_t dstB = (dst >>  0) & 0xFF;
                const uint32_t a255 = alpha;
                const uint32_t blendR = (r * a255 + dstR * (255 - a255)) / 255;
                const uint32_t blendG = (g * a255 + dstG * (255 - a255)) / 255;
                const uint32_t blendB = (b * a255 + dstB * (255 - a255)) / 255;
                pixelBuffer[py * bufWidth + px] = (blendR << 16) | (blendG << 8) | blendB;
            }
        }
    }
}

void PluginView::setKnobValue(int index, int value) {
    if (index < 0 || index >= static_cast<int>(knobs.size())) return;
    value = std::max(0, std::min(255, value));
    if (knobs[index].value == value) return;
    knobs[index].value = value;
    render();
    drawToWindow();
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

    HWND hwnd = static_cast<HWND>(platformWindow);
    g_hwndToView[hwnd] = this;
    originalWindowProc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)WindowProcStub);

    SetTimer(hwnd, 1, 33, nullptr);  // ~30fps render timer

    render();
    drawToWindow();
    return Steinberg::kResultTrue;
}

Steinberg::tresult PLUGIN_API PluginView::removed() {
    // Restore original window procedure to prevent DAW crash
    if (platformWindow && originalWindowProc) {
        HWND hwnd = static_cast<HWND>(platformWindow);
        KillTimer(hwnd, 1);
        g_hwndToView.erase(hwnd);
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)originalWindowProc);
        originalWindowProc = nullptr;
    }
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

void PluginView::updateLayout() {
    const int w = viewRect.right  - viewRect.left;
    const int h = viewRect.bottom - viewRect.top;
    const int n = static_cast<int>(knobs.size());
    if (n == 0 || w <= 0 || h <= 0) return;

    // Knobs evenly spaced and horizontally centered; Y at 67% of height
    const int spacing = w / (n + 1);
    const int knobY   = h * 67 / 100;
    for (int i = 0; i < n; ++i) {
        knobs[i].x = spacing * (i + 1);
        knobs[i].y = knobY;
    }
}

void PluginView::render() {
    const int width  = viewRect.right  - viewRect.left;
    const int height = viewRect.bottom - viewRect.top;

    if (width <= 0 || height <= 0) return;

    const size_t requiredSize = static_cast<size_t>(width * height);
    if (pixelBuffer.size() != requiredSize)
        pixelBuffer.resize(requiredSize, 0x00FFFFFF);

    for (size_t i = 0; i < pixelBuffer.size(); ++i)
        pixelBuffer[i] = 0x00FFFFFF;

    updateLayout();
    drawBackground();
    drawWaveform();

    for (int i = 0; i < static_cast<int>(knobs.size()); ++i)
        drawKnob(i);
}

void PluginView::drawWaveform() {
    if (knobs.size() < 4) return;

    const int bufWidth  = viewRect.right  - viewRect.left;
    const int bufHeight = viewRect.bottom - viewRect.top - 150;
    const int x1 = 0, y1 = 0, x2 = bufWidth, y2 = bufHeight;
    const int w  = x2 - x1;
    const int h  = y2 - y1;
    if (w <= 0 || h <= 0) return;

    auto plot = [&](int x, int y, uint32_t color) {
        if (x >= 0 && x < bufWidth && y >= 0 && y < bufHeight)
            pixelBuffer[y * bufWidth + x] = color;
    };

    // --- Background: previous frame scaled outward + 80% alpha blend ---
    if (!waveformFeedback.empty() && wfFeedbackW == w && wfFeedbackH == h) {
        // Expand content by 0-3px per axis independently each frame
        const double scaleX = static_cast<double>(w + (rand() % 4)) / w;
        const double scaleY = static_cast<double>(h + (rand() % 4)) / h;
        const double cx = w * 0.5;
        const double cy = h * 0.5;

        for (int dy = 0; dy < h; ++dy) {
            for (int dx = 0; dx < w; ++dx) {
                // Inverse-map: find source pixel in feedback that lands at (dx, dy)
                const int srcX = static_cast<int>(cx + (dx - cx) / scaleX + 0.5);
                const int srcY = static_cast<int>(cy + (dy - cy) / scaleY + 0.5);

                const int px = x1 + dx;
                const int py = y1 + dy;

                // Current bg pixel (background image already drawn by drawBackground)
                const uint32_t bg  = pixelBuffer[py * bufWidth + px];
                const uint32_t bgR = (bg >> 16) & 0xFF;
                const uint32_t bgG = (bg >>  8) & 0xFF;
                const uint32_t bgB = (bg >>  0) & 0xFF;

                if (srcX >= 0 && srcX < w && srcY >= 0 && srcY < h) {
                    const uint32_t prev = waveformFeedback[srcY * w + srcX];
                    const uint32_t pR   = (prev >> 16) & 0xFF;
                    const uint32_t pG   = (prev >>  8) & 0xFF;
                    const uint32_t pB   = (prev >>  0) & 0xFF;
                    // 95% previous frame + 5% background (net 5% alpha reduction per frame)
                    pixelBuffer[py * bufWidth + px] =
                        (((pR * 242 + bgR * 13) / 255) << 16) |
                        (((pG * 242 + bgG * 13) / 255) <<  8) |
                         ((pB * 242 + bgB * 13) / 255);
                }
                // else: keep existing background pixel at this position
            }
        }
    }
    // On first frame or after resize: background image shows through as-is

    // Advance shake LFO (~5 Hz at 30 fps)
    if (noteActive) {
        shakePhase += (rand()%5) / 30.0;
        if (shakePhase >= 1.0) shakePhase -= 1.0;
        scrollOffset += 2;
    }
    const double shakeSin    = noteActive ? std::sin(2.0 * M_PI * shakePhase) : 0.0;
    const double shakeCos    = noteActive ? std::cos(2.0 * M_PI * shakePhase) : 0.0;
    const int    shakeOffsetY = static_cast<int>(shakeSin * 24.0);
    const int    shakeOffsetX = static_cast<int>(shakeCos * 24.0);

    // Scroll left-to-right per redraw (always running)
    scrollOffset -= 1;

    // --- Zero line ---
    const int midY = (y1 + y2) / 2 + shakeOffsetY;
    for (int x = x1 + 1; x < x2 - 1; ++x)
        plot(x, midY, 0x00CCCCCC);

    // --- Waveform line ---
    const float vols[4] = {
        knobs[0].value / 255.0f,
        knobs[1].value / 255.0f,
        knobs[2].value / 255.0f,
        knobs[3].value / 255.0f
    };

    const int    amplitude  = h / 2 - 4;
    const int    numSamples = w - 2;
    const double numCycles  = 2.0;

    // Cycle hue ~2 degrees per redraw through a fully saturated, full-brightness spectrum
    waveformHue += 2.0;
    if (waveformHue >= 360.0) waveformHue -= 360.0;

    const double   hf = waveformHue / 60.0;
    const int      hi = static_cast<int>(hf) % 6;
    const double   f  = hf - std::floor(hf);
    const uint32_t t  = static_cast<uint32_t>(f * 255.0);        // rising component
    const uint32_t q  = static_cast<uint32_t>((1.0 - f) * 255.0); // falling component

    uint32_t lineColor;
    switch (hi) {
        case 0:  lineColor = (255 << 16) | (t   <<  8) | 0;   break; // red → yellow
        case 1:  lineColor = (q   << 16) | (255 <<  8) | 0;   break; // yellow → green
        case 2:  lineColor = (0   << 16) | (255 <<  8) | t;   break; // green → cyan
        case 3:  lineColor = (0   << 16) | (q   <<  8) | 255; break; // cyan → blue
        case 4:  lineColor = (t   << 16) | (0   <<  8) | 255; break; // blue → magenta
        default: lineColor = (255 << 16) | (0   <<  8) | q;   break; // magenta → red
    }

    // Scroll phase offset: subtracting scrollOffset moves the wave rightward
    const double scrollPhaseOffset = (static_cast<double>(scrollOffset) / numSamples) * numCycles;

    int prevPy = -1;
    for (int s = 0; s < numSamples; ++s) {
        double phase     = (static_cast<double>(s) / numSamples) * numCycles - scrollPhaseOffset;
        double normPhase = phase - std::floor(phase);

        float sine   = static_cast<float>(std::sin(2.0 * M_PI * normPhase)) * vols[0];
        float square = (normPhase < 0.5 ? 1.0f : -1.0f) * vols[1];

        float tri;
        float np = static_cast<float>(normPhase);
        if      (normPhase < 0.25) tri = (-1.0f + 4.0f * np)               * vols[2];
        else if (normPhase < 0.75) tri = ( 1.0f - 4.0f * (np - 0.25f))     * vols[2];
        else                       tri = (-1.0f + 4.0f * (np - 0.75f))     * vols[2];

        float saw   = (-1.0f + 2.0f * np) * vols[3];
        float mixed = (sine + square + tri + saw) * 0.25f;

        int py = midY - static_cast<int>(mixed * amplitude);
        py = std::max(y1 + 1, std::min(y2 - 2, py));

        int bx = x1 + 1 + s + shakeOffsetX;
        if (prevPy >= 0) {
            int yStart = std::min(prevPy, py);
            int yEnd   = std::max(prevPy, py);
            for (int vy = yStart; vy <= yEnd; ++vy)
                plot(bx, vy, lineColor);
        } else {
            plot(bx, py, lineColor);
        }
        prevPy = py;
    }

    // --- Save waveform area as feedback for next frame ---
    wfFeedbackW = w;
    wfFeedbackH = h;
    waveformFeedback.resize(w * h);
    for (int dy = 0; dy < h; ++dy)
        for (int dx = 0; dx < w; ++dx)
            waveformFeedback[dy * w + dx] = pixelBuffer[(y1 + dy) * bufWidth + (x1 + dx)];
}

void PluginView::drawKnob(int knobIndex) {
    if (knobIndex < 0 || knobIndex >= static_cast<int>(knobs.size())) {
        return;
    }

    const Knob& knob = knobs[knobIndex];
    int cx = knob.x;
    int cy = knob.y;
    int radius = knob.size / 2;
    int bufWidth = viewRect.right - viewRect.left;
    int bufHeight = viewRect.bottom - viewRect.top;



    // Draw indicator line showing knob position
    double angle = (knob.value / 255.0) * (270.0 * M_PI / 180.0) - (135.0 * M_PI / 180.0);
    int lineLen = radius - 8;
    int x2 = cx + static_cast<int>(std::cos(angle) * lineLen);
    int y2 = cy + static_cast<int>(std::sin(angle) * lineLen);

    int steps = std::max(std::abs(x2 - cx), std::abs(y2 - cy));
    for (int s = 0; s <= steps; ++s) {
        int lx = cx + (steps > 0 ? (x2 - cx) * s / steps : 0);
        int ly = cy + (steps > 0 ? (y2 - cy) * s / steps : 0);
        if (lx >= 0 && lx < bufWidth && ly >= 0 && ly < bufHeight) {
            pixelBuffer[ly * bufWidth + lx] = 0x00000000;  // Black indicator
        }
    }

    // Draw dial.png on top, alpha-blended and rotated to match knob position
    drawDialImage(knob);
}

void PluginView::drawTextToWindow(HDC hdc, const char* text, int x, int y, uint32_t color) {
    if (!text) return;

    Graphics graphics(hdc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);

    Font font(L"Arial", 9, FontStyleBold);

    wchar_t wText[256];
    MultiByteToWideChar(CP_ACP, 0, text, -1, wText, 256);

    int b = (color >> 0) & 0xFF;
    int g = (color >> 8) & 0xFF;
    int r = (color >> 16) & 0xFF;

    // White shadow offset by 1px
    SolidBrush shadowBrush(Color(255, 255, 255, 255));
    PointF shadowPt(static_cast<REAL>(x + 1), static_cast<REAL>(y + 1));
    graphics.DrawString(wText, -1, &font, shadowPt, &shadowBrush);

    SolidBrush brush(Color(255, r, g, b));
    PointF pointF(static_cast<REAL>(x), static_cast<REAL>(y));
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
            if (draggedKnobIndex >= 0) {
                SetCapture(hwnd);  // Receive WM_LBUTTONUP even if cursor leaves window
                if (controller)
                    controller->beginEdit(static_cast<Steinberg::Vst::ParamID>(draggedKnobIndex));
            }
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
                        if (controller) {
                            controller->performEdit(
                                static_cast<Steinberg::Vst::ParamID>(draggedKnobIndex),
                                newValue / 255.0);
                        }
                        render();
                        drawToWindow();
                    }
                }
                
                lastMouseY = y;
            }
            return 0;
        }
        
        case WM_LBUTTONUP: {
            if (draggedKnobIndex >= 0) {
                ReleaseCapture();
                if (controller)
                    controller->endEdit(static_cast<Steinberg::Vst::ParamID>(draggedKnobIndex));
            }
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
                if (controller) {
                    controller->beginEdit(static_cast<Steinberg::Vst::ParamID>(knobIndex));
                    controller->performEdit(
                        static_cast<Steinberg::Vst::ParamID>(knobIndex),
                        newValue / 255.0);
                    controller->endEdit(static_cast<Steinberg::Vst::ParamID>(knobIndex));
                }
                render();
                drawToWindow();
            }
            return 0;
        }
        
        case WM_TIMER: {
            render();
            drawToWindow();
            ValidateRect(hwnd, nullptr);
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
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
    drawTextToWindow(hdc, "MONODUCK 1.0", 20, 10, 0xFF000000);
    
    // Place labels just below the full dial draw extent (includes pointer protrusion)
    const int labelOffset = dialScreenExtent + 6;
    for (const auto& knob : knobs) {
        drawTextToWindow(hdc, knob.label, knob.x - 15, knob.y + labelOffset, 0xFF000000);

        char ampText[16];
        snprintf(ampText, sizeof(ampText), "%d%%", (knob.value * 100) / 255);
        drawTextToWindow(hdc, ampText, knob.x - 12, knob.y + labelOffset + 14, 0xFF000080);
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
