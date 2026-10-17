// GPU color-ID picking: render every object in a unique flat color, read the pixel under the cursor.
#pragma once
#include "render/Renderer.h"

namespace og {

struct ViewRect { int x, y, w, h; };  // GL window coordinates (origin bottom-left)

// Returns the object index under window pixel (x, yTop) (y measured from the top), or -1.
// Draws into the back buffer; the caller redraws the frame afterwards.
int pickObject(Renderer& r, const AppState& s, const Mat4& view, const Mat4& proj, ViewRect vp, int x, int yTop);

struct PickTestResult { int tested = 0, hits = 0; };
// Projects every visible, unambiguous (isolated) object with our own matrices, renders the ID buffer
// once and checks that the pixel at each projected position decodes back to that object.
PickTestResult runPickTest(Renderer& r, const AppState& s, const Mat4& view, const Mat4& proj, ViewRect vp);

}  // namespace og
