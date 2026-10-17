#include "color/ColorModels.h"
#include <algorithm>
#include <cmath>

namespace og {

static double clamp01(double x) { return std::min(1.0, std::max(0.0, x)); }

HSV rgbToHsv(RGB c) {
    double mx = std::max({c.r, c.g, c.b}), mn = std::min({c.r, c.g, c.b});
    double d = mx - mn;
    HSV o;
    o.v = mx;
    o.s = mx > 0 ? d / mx : 0;
    if (d <= 0) { o.h = 0; return o; }
    if (mx == c.r) o.h = 60.0 * std::fmod((c.g - c.b) / d, 6.0);
    else if (mx == c.g) o.h = 60.0 * ((c.b - c.r) / d + 2.0);
    else o.h = 60.0 * ((c.r - c.g) / d + 4.0);
    if (o.h < 0) o.h += 360.0;
    return o;
}

RGB hsvToRgb(HSV c) {
    double h = std::fmod(c.h, 360.0);
    if (h < 0) h += 360.0;
    double C = c.v * c.s;
    double X = C * (1 - std::fabs(std::fmod(h / 60.0, 2.0) - 1));
    double m = c.v - C;
    double r = 0, g = 0, b = 0;
    switch (int(h / 60.0)) {
        case 0: r = C; g = X; break;
        case 1: r = X; g = C; break;
        case 2: g = C; b = X; break;
        case 3: g = X; b = C; break;
        case 4: r = X; b = C; break;
        default: r = C; b = X; break;
    }
    return {r + m, g + m, b + m};
}

// NTSC YIQ
YIQ rgbToYiq(RGB c) {
    return {0.299 * c.r + 0.587 * c.g + 0.114 * c.b,
            0.596 * c.r - 0.274 * c.g - 0.322 * c.b,
            0.211 * c.r - 0.523 * c.g + 0.312 * c.b};
}

RGB yiqToRgb(YIQ c) {
    // Exact inverse of the matrix above (computed, not the rounded textbook one) so round-trips are lossless.
    const double m[9] = {0.299, 0.587, 0.114, 0.596, -0.274, -0.322, 0.211, -0.523, 0.312};
    double det = m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6]) + m[2] * (m[3] * m[7] - m[4] * m[6]);
    double inv[9] = {
        (m[4] * m[8] - m[5] * m[7]) / det, (m[2] * m[7] - m[1] * m[8]) / det, (m[1] * m[5] - m[2] * m[4]) / det,
        (m[5] * m[6] - m[3] * m[8]) / det, (m[0] * m[8] - m[2] * m[6]) / det, (m[2] * m[3] - m[0] * m[5]) / det,
        (m[3] * m[7] - m[4] * m[6]) / det, (m[1] * m[6] - m[0] * m[7]) / det, (m[0] * m[4] - m[1] * m[3]) / det};
    return {inv[0] * c.y + inv[1] * c.i + inv[2] * c.q,
            inv[3] * c.y + inv[4] * c.i + inv[5] * c.q,
            inv[6] * c.y + inv[7] * c.i + inv[8] * c.q};
}

CMY rgbToCmy(RGB c) { return {1 - c.r, 1 - c.g, 1 - c.b}; }
RGB cmyToRgb(CMY c) { return {1 - c.c, 1 - c.m, 1 - c.y}; }

const char* colorModeName(ColorMode m) {
    switch (m) {
        case ColorMode::RGB: return "RGB";
        case ColorMode::HSVHeat: return "HSV risk heatmap";
        case ColorMode::YIQNight: return "YIQ night-ops";
        case ColorMode::CMYPrint: return "CMY print preview";
    }
    return "?";
}

const char* colorModeExplain(ColorMode m) {
    switch (m) {
        case ColorMode::RGB: return "Additive RGB: the framebuffer's native model, colors shown as-is.";
        case ColorMode::HSVHeat: return "Objects colored by hue = f(altitude/risk); scene desaturated by scaling S in HSV.";
        case ColorMode::YIQNight: return "Only luminance Y = .299R+.587G+.114B kept (I,Q dropped) -> green night-vision mono.";
        case ColorMode::CMYPrint: return "Subtractive CMY = 1-RGB, ink quantized to 4 levels, rendered on white 'paper'.";
    }
    return "";
}

RGB applyColorMode(ColorMode m, RGB c) {
    switch (m) {
        case ColorMode::RGB: return c;
        case ColorMode::HSVHeat: {
            HSV h = rgbToHsv(c);
            h.s *= 0.25;
            h.v *= 0.8;
            return hsvToRgb(h);
        }
        case ColorMode::YIQNight: {
            double y = clamp01(rgbToYiq(c).y);
            return {0.1 * y, y, 0.25 * y};
        }
        case ColorMode::CMYPrint: {
            CMY k = rgbToCmy(c);
            auto q = [](double x) { return std::round(clamp01(x) * 3.0) / 3.0; };
            return cmyToRgb({q(k.c), q(k.m), q(k.y)});
        }
    }
    return c;
}

RGB riskColor(double dKm, double greenKm) {
    double t = clamp01(dKm / greenKm);
    return hsvToRgb({120.0 * t, 1.0, 1.0});
}

RGB altitudeHeat(double altKm) {
    // log scale over the crowded LEO band, 300 km (blue, hue 240) .. 1400 km (red, hue 0); MEO/GEO clamp red
    double t = clamp01(std::log(std::max(altKm, 300.0) / 300.0) / std::log(1400.0 / 300.0));
    return hsvToRgb({240.0 * (1.0 - t), 0.9, 1.0});
}

}  // namespace og
