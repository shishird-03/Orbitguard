// Hand-written color-model conversions: RGB <-> HSV, YIQ, CMY.
#pragma once

namespace og {

struct RGB { double r = 0, g = 0, b = 0; };
struct HSV { double h = 0, s = 0, v = 0; };  // h in degrees [0,360)
struct YIQ { double y = 0, i = 0, q = 0; };
struct CMY { double c = 0, m = 0, y = 0; };

HSV rgbToHsv(RGB c);
RGB hsvToRgb(HSV c);
YIQ rgbToYiq(RGB c);
RGB yiqToRgb(YIQ c);
CMY rgbToCmy(RGB c);
RGB cmyToRgb(CMY c);

enum class ColorMode { RGB = 0, HSVHeat = 1, YIQNight = 2, CMYPrint = 3 };
const char* colorModeName(ColorMode m);
const char* colorModeExplain(ColorMode m);

// The single color pipeline every non-data color passes through.
RGB applyColorMode(ColorMode m, RGB c);
// Risk gradient: hue 0 (red) at d=0 to 120 (green) at d>=greenKm, interpolated in HSV.
RGB riskColor(double dKm, double greenKm);
// Altitude heat (HSV): blue for low LEO to red above ~1400 km.
RGB altitudeHeat(double altKm);

}  // namespace og
