// Cohen-Sutherland line clipping and Sutherland-Hodgman polygon clipping against a rectangle.
#pragma once
#include <vector>
#include "math/Transform2D.h"

namespace og {

struct ClipRect { double xmin, ymin, xmax, ymax; };

int outcode(double x, double y, const ClipRect& r);
// Returns false if the segment is fully outside; otherwise clips the endpoints in place.
bool cohenSutherland(double& x0, double& y0, double& x1, double& y1, const ClipRect& r);
std::vector<Vec2> sutherlandHodgman(const std::vector<Vec2>& poly, const ClipRect& r);

}  // namespace og
