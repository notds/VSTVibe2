#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "pluginview.h"
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include "mandelbrot_shaper.h"
#include "pluginprocessor.h"

#ifdef _WIN32
#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <unordered_map>
#include "dial_image_data.h"
#include "bg_image_data.h"
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef _WIN32
using namespace Gdiplus;

namespace VSTVibe2 {

static ULONG_PTR gdiplusToken    = 0;
static int        gdiplusRefCount = 0;

static std::unordered_map<HWND, PluginView*> g_hwndToView;

LRESULT CALLBACK WindowProcStub(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto it = g_hwndToView.find(hwnd);
    if (it != g_hwndToView.end())
        return it->second->onWindowMessage(hwnd, message, wParam, lParam);
    return DefWindowProc(hwnd, message, wParam, lParam);
}

#else
namespace VSTVibe2 {
#endif

PluginView::PluginView()
    : Steinberg::CPluginView() {
    viewRect.right  = 600;
    viewRect.bottom = 400;
    initializeKnobs();
#ifdef _WIN32
    pixelBuffer.assign(600 * 400, 0x00FFFFFF);

    if (gdiplusRefCount++ == 0) {
        GdiplusStartupInput gdiplusStartupInput;
        GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    }

    loadBackgroundImage();
    loadDialImage();

    const int minH = std::max(MIN_HEIGHT, (dialScreenExtent + 46) * 100 / 18 + 1);
    const int w    = std::max(MIN_WIDTH, 600);
    viewRect.right  = viewRect.left + w;
    viewRect.bottom = viewRect.top  + minH;
    pixelBuffer.assign(static_cast<size_t>(w * minH), 0x00FFFFFF);
#endif
}

PluginView::~PluginView() {
#ifdef _WIN32
    if (--gdiplusRefCount == 0) {
        Gdiplus::GdiplusShutdown(gdiplusToken);
        gdiplusToken = 0;
    }
#endif
}

void PluginView::initializeKnobs() {
    knobs.clear();
    knobs.push_back({80,  200, KNOB_SIZE, 255, "Sine",        0}); // kSineVolumeID
    knobs.push_back({180, 200, KNOB_SIZE, 255, "Square",      1}); // kSquareVolumeID
    knobs.push_back({280, 200, KNOB_SIZE, 255, "Triangle",    2}); // kTriangleVolumeID
    knobs.push_back({380, 200, KNOB_SIZE, 255, "Saw",         3}); // kSawVolumeID
    knobs.push_back({480, 200, KNOB_SIZE,   0, "Sassiness",  11}); // kSassinessID
    knobs.push_back({300, 320, KNOB_SIZE, 128, "Spice",       4}); // kSpiceID
    knobs.push_back({450, 320, KNOB_SIZE,   0, "Squeeze",     7}); // kSqueezeID
    knobs.push_back({600, 320, KNOB_SIZE,   0, "Glide",       9}); // kGlideID
    knobs.push_back({750, 320, KNOB_SIZE,   0, "Distortion", 10}); // kDistortionID
    knobs.push_back({900, 320, KNOB_SIZE,   0, "XOR Rand",   12}); // kXorRandID
}

#ifdef _WIN32  // ---- All rendering below is Windows/GDI+ only ----

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
            bgBuffer[py * winW + px] = (r << 16) | (g << 8) | b;
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

// Fixed hues per knob index — red/pink/purple/blue family, same every run
static constexpr float kKnobHues[] = { 355.0f, 340.0f, 320.0f, 300.0f, 280.0f, 260.0f, 240.0f, 220.0f };

static void hsvToRgb(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b) {
    h = std::fmod(h, 360.0f);
    if (h < 0.0f) h += 360.0f;
    const float c  = v * s;
    const float x  = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    const float m  = v - c;
    float rf, gf, bf;
    if      (h < 60.0f)  { rf = c; gf = x; bf = 0; }
    else if (h < 120.0f) { rf = x; gf = c; bf = 0; }
    else if (h < 180.0f) { rf = 0; gf = c; bf = x; }
    else if (h < 240.0f) { rf = 0; gf = x; bf = c; }
    else if (h < 300.0f) { rf = x; gf = 0; bf = c; }
    else                 { rf = c; gf = 0; bf = x; }
    r = static_cast<uint8_t>((rf + m) * 255.0f + 0.5f);
    g = static_cast<uint8_t>((gf + m) * 255.0f + 0.5f);
    b = static_cast<uint8_t>((bf + m) * 255.0f + 0.5f);
}

void PluginView::drawDialImage(const Knob& knob, int knobIndex) {
    if (dialImage.empty() || dialWidth <= 0 || dialHeight <= 0 || dialRadius <= 0.0) return;

    const int cx = knob.x;
    const int cy = knob.y;
    const int radius = knob.size / 2;
    const int bufWidth  = viewRect.right  - viewRect.left;
    const int bufHeight = viewRect.bottom - viewRect.top;

    const float hue = kKnobHues[knobIndex % 8];

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

            // Colorize: preserve luminance (V), fix hue and saturation per knob index
            const float rN = ((argb >> 16) & 0xFF) / 255.0f;
            const float gN = ((argb >>  8) & 0xFF) / 255.0f;
            const float bN = ((argb >>  0) & 0xFF) / 255.0f;
            const float v  = std::max({rN, gN, bN});
            uint8_t cr, cg, cb;
            hsvToRgb(hue, 0.80f, v, cr, cg, cb);
            knobBuffer[py * bufWidth + px] = (alpha << 24) | (cr << 16u) | (cg << 8u) | cb;
        }
    }
}

void PluginView::setKnobValue(int index, int value) {
    if (index < 0 || index >= static_cast<int>(knobs.size())) return;
    value = std::max(0, std::min(255, value));
    if (knobs[index].value == value) return;
    knobs[index].value = value;
    knobLayerDirty = true;
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
    if (knobs.empty() || w <= 0 || h <= 0) return;

    // Row 1: first 5 knobs, evenly spaced across full width
    const int row1Count   = std::min(5, static_cast<int>(knobs.size()));
    const int row1Y       = h * 52 / 100;
    const int row1Spacing = w / (row1Count + 1);
    for (int i = 0; i < row1Count; ++i) {
        knobs[i].x = row1Spacing * (i + 1);
        knobs[i].y = row1Y;
    }

    // Row 2: remaining knobs, evenly spaced and centered below row 1
    const int row2Count = static_cast<int>(knobs.size()) - row1Count;
    if (row2Count > 0) {
        const int row2Y       = h * 82 / 100;
        const int row2Spacing = w / (row2Count + 1);
        for (int i = 0; i < row2Count; ++i) {
            knobs[row1Count + i].x = row2Spacing * (i + 1);
            knobs[row1Count + i].y = row2Y;
        }
    }
}

void PluginView::render() {
    const int w = viewRect.right  - viewRect.left;
    const int h = viewRect.bottom - viewRect.top;
    if (w <= 0 || h <= 0) return;

    // Full-redraw safety timer (~30 s at 30 fps)
    if (--fullRedrawCounter <= 0) {
        fullRedrawCounter = FULL_REDRAW_INTERVAL;
        backgroundDirty   = true;
        knobLayerDirty    = true;
        waveformFeedback.clear();
    }

    const size_t total = static_cast<size_t>(w * h);

    if (pixelBuffer.size() != total) pixelBuffer.assign(total, 0x00FFFFFFu);
    if (bgBuffer.size()    != total) { bgBuffer.assign(total, 0x00FFFFFFu); backgroundDirty = true; }
    if (knobBuffer.size()  != total) { knobBuffer.assign(total, 0u);        knobLayerDirty  = true; }
    if (textBuffer.size()  != total) { textBuffer.assign(total, 0u);        knobLayerDirty  = true; }

    // Layout: only recompute on resize
    if (lastRenderW != w || lastRenderH != h) {
        updateLayout();
        knobLayerDirty = true;
        lastRenderW = w;
        lastRenderH = h;
    }

    // 1. Background (cached — only redrawn on resize / full-redraw)
    if (backgroundDirty) {
        std::fill(bgBuffer.begin(), bgBuffer.end(), 0x00FFFFFFu);
        drawBackground();
        backgroundDirty = false;
    }

    // 2. Copy background into composite buffer
    std::copy(bgBuffer.begin(), bgBuffer.end(), pixelBuffer.begin());

    // 3. Waveform into its own ARGB buffer, then composite
    drawWaveform();
    for (int py = 0; py < waveH && py < h; ++py) {
        for (int px = 0; px < waveW && px < w; ++px) {
            const uint32_t src   = waveBuffer[py * waveW + px];
            const uint32_t alpha = src >> 24;
            if (!alpha) continue;
            const uint32_t dst = pixelBuffer[py * w + px];
            const uint32_t inv = 255 - alpha;
            pixelBuffer[py * w + px] =
                ((((src >> 16 & 0xFF) * alpha + (dst >> 16 & 0xFF) * inv) / 255) << 16) |
                ((((src >>  8 & 0xFF) * alpha + (dst >>  8 & 0xFF) * inv) / 255) <<  8) |
                 (((src       & 0xFF) * alpha + (dst       & 0xFF) * inv) / 255);
        }
    }

    // 4. Knob + text layers (cached together — rebuilt when any knob value changes)
    if (knobLayerDirty) {
        std::fill(knobBuffer.begin(), knobBuffer.end(), 0u);
        for (int i = 0; i < static_cast<int>(knobs.size()); ++i)
            drawKnob(i);
        drawTextLayer();
        knobLayerDirty = false;
    }

    // Helper: alpha-composite one cached ARGB layer over pixelBuffer
    auto compositeLayer = [&](const std::vector<uint32_t>& layer) {
        for (size_t i = 0; i < total; ++i) {
            const uint32_t src   = layer[i];
            const uint32_t alpha = src >> 24;
            if (!alpha) continue;
            const uint32_t dst = pixelBuffer[i];
            const uint32_t inv = 255 - alpha;
            pixelBuffer[i] =
                ((((src >> 16 & 0xFF) * alpha + (dst >> 16 & 0xFF) * inv) / 255) << 16) |
                ((((src >>  8 & 0xFF) * alpha + (dst >>  8 & 0xFF) * inv) / 255) <<  8) |
                 (((src       & 0xFF) * alpha + (dst       & 0xFF) * inv) / 255);
        }
    };

    // 5. Composite knob layer, then text layer
    compositeLayer(knobBuffer);
    compositeLayer(textBuffer);
}

void PluginView::drawWaveform() {
    if (knobs.size() < 4) return;

    const int w = viewRect.right  - viewRect.left;
    const int h = viewRect.bottom - viewRect.top - 150;
    if (w <= 0 || h <= 0) return;

    // Resize waveBuffer if needed; invalidate feedback on size change
    if (waveW != w || waveH != h) {
        waveW = w;
        waveH = h;
        waveBuffer.assign(static_cast<size_t>(w * h), 0u);
        waveformFeedback.clear();
    }

    // plot() writes into waveBuffer with full alpha
    auto plot = [&](int x, int y, uint32_t color) {
        if (x >= 0 && x < w && y >= 0 && y < h)
            waveBuffer[y * w + x] = 0xFF000000u | color;
    };

    // --- Feedback: scale-expanded previous waveBuffer, alpha-faded ---
    // Pure waveform ARGB — background is not baked in, so the trail fades to transparent.
    std::fill(waveBuffer.begin(), waveBuffer.end(), 0u);
    if (!waveformFeedback.empty() && waveformFeedback.size() == waveBuffer.size()) {
        const double scaleX = static_cast<double>(w + (rand() % 4)) / w;
        const double scaleY = static_cast<double>(h + (rand() % 4)) / h;
        const double cx = w * 0.5, cy = h * 0.5;

        for (int dy = 0; dy < h; ++dy) {
            for (int dx = 0; dx < w; ++dx) {
                const int srcX = static_cast<int>(cx + (dx - cx) / scaleX + 0.5);
                const int srcY = static_cast<int>(cy + (dy - cy) / scaleY + 0.5);
                if (srcX < 0 || srcX >= w || srcY < 0 || srcY >= h) continue;
                const uint32_t prev = waveformFeedback[srcY * w + srcX];
                const uint32_t a    = prev >> 24;
                if (!a) continue;
                const uint32_t newA = (a * 242) / 255;  // ~95% retention per frame
                if (newA)
                    waveBuffer[dy * w + dx] = (newA << 24) | (prev & 0x00FFFFFFu);
            }
        }
    }

    // Noise envelope: instant attack on note-on, ~150ms exponential decay at ~30fps
    if (noteActive)
        visualNoiseEnv = 1.0f;
    else
        visualNoiseEnv *= 0.8007f;  // exp(-1 / (30 * 0.150))

    // Advance shake LFO (~5 Hz at 30 fps)
    if (noteActive) {
        shakePhase += 1 / 30.0;
        if (shakePhase >= 1.0) shakePhase -= 1.0;
        scrollOffset += 2;
    }
    const double shakeSin    = noteActive ? std::sin(2.0 * M_PI * shakePhase) : 0.0;
    const double shakeCos    = noteActive ? std::cos(2.0 * M_PI * shakePhase) : 0.0;
    const int    shakeOffsetY = static_cast<int>(shakeSin * 12.0);
    const int    shakeOffsetX = static_cast<int>(shakeCos * 12.0);

    // Scroll left-to-right per redraw (always running)
    scrollOffset -= 1;

    // --- Zero line ---
    const int midY = h / 2 + shakeOffsetY;
    for (int x = 1; x < w - 1; ++x)
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
    waveformHue += 1.9;
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

    const float spiceLevel      = knobs.size() > 4 ? knobs[4].value / 255.0f : 0.0f;
    const float squeezeLevel    = knobs.size() > 5 ? knobs[5].value / 255.0f : 0.0f;
    const float distortionLevel = knobs.size() > 7 ? knobs[7].value / 255.0f : 0.0f;

    // Steady-state compressor constants (mirrors audio processor, no envelope follower)
    const float compThreshDb = -18.0f;
    const float compRatio    = 1.0f + squeezeLevel * 19.0f;
    const float compMakeupDb = squeezeLevel * 9.0f;

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
        mixed = applyMandelbrot(mixed, spiceLevel, mandelbrotState);

        // Steady-state compression: compute gain from instantaneous amplitude
        if (squeezeLevel > 1e-4f) {
            const float levelDb  = 20.0f * std::log10(std::abs(mixed) + 1e-7f);
            const float overDb   = std::max(0.0f, levelDb - compThreshDb);
            const float reductDb = overDb * (1.0f - 1.0f / compRatio);
            mixed *= std::pow(10.0f, (-reductDb + compMakeupDb) / 20.0f);
        }

        // Distortion: foldback — mirrors audio chain
        if (distortionLevel > 1e-4f) {
            float x = mixed * (1.0f + distortionLevel * 7.0f);
            for (int fi = 0; fi < 8; ++fi) {
                if      (x >  1.0f) x =  2.0f - x;
                else if (x < -1.0f) x = -2.0f - x;
                else break;
            }
            mixed = mixed * (1.0f - distortionLevel) + x * distortionLevel;

            if (distortionLevel > 0.5f && visualNoiseEnv > 1e-4f) {
                const uint32_t hash  = static_cast<uint32_t>(s) * 2654435761u;
                const float noise    = static_cast<float>(hash) * (1.0f / 4294967296.0f) * 2.0f - 1.0f;
                const float noiseAmt = (distortionLevel - 0.5f) * 2.0f * 0.04f * visualNoiseEnv;
                mixed += noise * noiseAmt;
            }
        }

        int py = midY - static_cast<int>(mixed * amplitude);
        py = std::max(1, std::min(h - 2, py));

        int bx = 1 + s + shakeOffsetX;
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

    // Save waveBuffer as feedback for next frame (pure waveform ARGB, no background)
    waveformFeedback = waveBuffer;
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
            knobBuffer[ly * bufWidth + lx] = 0xFF000000u;  // Full-alpha black in knob layer
        }
    }

    // Draw dial.png on top, alpha-blended and rotated to match knob position
    drawDialImage(knob, knobIndex);
}

void PluginView::drawTextLayer() {
    const int w = viewRect.right  - viewRect.left;
    const int h = viewRect.bottom - viewRect.top;
    if (w <= 0 || h <= 0) return;

    std::fill(textBuffer.begin(), textBuffer.end(), 0u);

    // Render directly into textBuffer via a GDI+ Bitmap wrapping the same memory
    Bitmap bmp(w, h, w * 4, PixelFormat32bppARGB, (BYTE*)textBuffer.data());
    Graphics gr(&bmp);
    gr.SetSmoothingMode(SmoothingModeAntiAlias);
    gr.SetTextRenderingHint(TextRenderingHintAntiAlias);

    Font        font(L"Arial", 9, FontStyleBold);
    SolidBrush  black(Color(255, 0, 0, 0));
    SolidBrush  boxBrush(Color(185, 255, 255, 255));  // ~73% opaque white backing
    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentNear);

    // Title — measure, draw one box, then black text
    {
        RectF m;
        gr.MeasureString(L"MONODUCK 1.02", -1, &font, PointF(0, 0), &m);
        gr.FillRectangle(&boxBrush, RectF(18.0f, 7.0f, m.Width + 6.0f, m.Height + 6.0f));
        gr.DrawString(L"MONODUCK 1.02", -1, &font, PointF(21.0f, 10.0f), &black);
    }

    // Knob labels + percentage: one box per knob, both strings centered inside it
    const int labelOffset = dialScreenExtent + 6;
    for (const auto& knob : knobs) {
        wchar_t wLabel[64], wAmp[16];
        MultiByteToWideChar(CP_ACP, 0, knob.label, -1, wLabel, 64);
        char ampBuf[16];
        snprintf(ampBuf, sizeof(ampBuf), "%d%%", (knob.value * 100) / 255);
        MultiByteToWideChar(CP_ACP, 0, ampBuf, -1, wAmp, 16);

        RectF mLabel, mAmp;
        gr.MeasureString(wLabel, -1, &font, PointF(0, 0), &mLabel);
        gr.MeasureString(wAmp,   -1, &font, PointF(0, 0), &mAmp);

        const float lineH = mLabel.Height;
        const float boxW  = std::max(mLabel.Width, mAmp.Width) + 10.0f;
        const float boxH  = lineH * 2.0f + 6.0f;
        const float boxX  = static_cast<float>(knob.x) - boxW * 0.5f;
        const float boxY  = static_cast<float>(knob.y + labelOffset);

        // One white box behind both lines
        gr.FillRectangle(&boxBrush, RectF(boxX - 2.0f, boxY - 2.0f, boxW + 4.0f, boxH + 4.0f));

        // Centered black text: label on top, percentage below
        gr.DrawString(wLabel, -1, &font, RectF(boxX, boxY,              boxW, lineH),        &sf, &black);
        gr.DrawString(wAmp,   -1, &font, RectF(boxX, boxY + lineH + 3, boxW, lineH),        &sf, &black);
    }
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
                    controller->beginEdit(static_cast<Steinberg::Vst::ParamID>(knobs[draggedKnobIndex].paramID));
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
                        knobLayerDirty = true;
                        const int pid = knobs[draggedKnobIndex].paramID;
                        if (pid < 4)
                            VSTVibe2Processor::oscillatorVolumes[pid] = newValue / 255.0f;
                        if (controller)
                            controller->performEdit(
                                static_cast<Steinberg::Vst::ParamID>(pid),
                                newValue / 255.0);
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
                    controller->endEdit(static_cast<Steinberg::Vst::ParamID>(knobs[draggedKnobIndex].paramID));
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
                knobLayerDirty = true;
                const int pid = knobs[knobIndex].paramID;
                if (pid < 4)
                    VSTVibe2Processor::oscillatorVolumes[pid] = newValue / 255.0f;
                if (controller) {
                    controller->beginEdit(static_cast<Steinberg::Vst::ParamID>(pid));
                    controller->performEdit(static_cast<Steinberg::Vst::ParamID>(pid), newValue / 255.0);
                    controller->endEdit(static_cast<Steinberg::Vst::ParamID>(pid));
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
    if (!platformWindow || pixelBuffer.empty()) return;

    HWND hwnd = static_cast<HWND>(platformWindow);
    HDC  hdc  = GetDC(hwnd);
    if (!hdc) return;

    const int w = viewRect.right  - viewRect.left;
    const int h = viewRect.bottom - viewRect.top;

    if (w > 0 && h > 0) {
        BITMAPINFO bmi          = {};
        bmi.bmiHeader.biSize    = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth   = w;
        bmi.bmiHeader.biHeight  = -h;  // top-down
        bmi.bmiHeader.biPlanes  = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        // Single blit — pixelBuffer already has all layers composited
        SetDIBitsToDevice(hdc, 0, 0, w, h, 0, 0, 0, h,
                          pixelBuffer.data(), &bmi, DIB_RGB_COLORS);
    }

    ReleaseDC(hwnd, hdc);
}

void PluginView::constrainSize() {
    // Same formula as constructor: ensures row-2 labels never clip during resize
    const int contentMinH = std::max(MIN_HEIGHT, (dialScreenExtent + 46) * 100 / 18 + 1);

    int width  = viewRect.right  - viewRect.left;
    int height = viewRect.bottom - viewRect.top;

    if (width  < MIN_WIDTH)   viewRect.right  = viewRect.left + MIN_WIDTH;
    if (height < contentMinH) viewRect.bottom = viewRect.top  + contentMinH;
    if (width  > MAX_WIDTH)   viewRect.right  = viewRect.left + MAX_WIDTH;
    if (height > MAX_HEIGHT)  viewRect.bottom = viewRect.top  + MAX_HEIGHT;
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

#else  // !_WIN32 — Mac/Linux stubs ----------------------------------------

void PluginView::loadBackgroundImage() {}
void PluginView::loadDialImage()       {}
void PluginView::drawBackground()      {}
void PluginView::drawWaveform()        {}
void PluginView::drawKnob(int)         {}
void PluginView::drawDialImage(const Knob&, int) {}
void PluginView::drawTextLayer()       {}
void PluginView::drawToWindow()        {}
void PluginView::constrainSize()       {}
void PluginView::invalidateRect()      {}
void PluginView::render()              {}

Steinberg::tresult PLUGIN_API PluginView::isPlatformTypeSupported(const char* type) {
    // Accept NSView on macOS; reject everything else
    if (type && strcmp(type, "NSView") == 0) return Steinberg::kResultTrue;
    return Steinberg::kResultFalse;
}
Steinberg::tresult PLUGIN_API PluginView::attached(void* /*parent*/, const char* /*type*/) {
    return Steinberg::kResultFalse;  // No GUI on non-Windows builds
}
Steinberg::tresult PLUGIN_API PluginView::removed() {
    platformWindow = nullptr;
    return Steinberg::kResultOk;
}
Steinberg::tresult PLUGIN_API PluginView::onSize(Steinberg::ViewRect* newSize) {
    if (newSize) viewRect = *newSize;
    return Steinberg::kResultOk;
}
Steinberg::tresult PLUGIN_API PluginView::getSize(Steinberg::ViewRect* size) {
    if (size) { *size = viewRect; return Steinberg::kResultOk; }
    return Steinberg::kResultFalse;
}

#endif // _WIN32

} // namespace VSTVibe2
