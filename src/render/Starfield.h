// Procedural starfield: seeded random directions on a far sphere, drawn rotation-only (at infinity).
#pragma once
#include <vector>
#include "color/ColorModels.h"
#include "math/Mat4.h"

namespace og {

class Starfield {
public:
    void build(int count, unsigned seed);
    void draw(const Mat4& view, ColorMode mode) const;

private:
    std::vector<float> xyz_, rgb_;
};

}  // namespace og
