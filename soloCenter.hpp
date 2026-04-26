#pragma once

#include <hyprland/src/layout/algorithm/Algorithm.hpp>

namespace DwindleSolo {
    void postRecalculate(Layout::CAlgorithm* algo);
    // Returns true if we handled the resize (caller should skip original)
    bool handleResize(Layout::CAlgorithm* algo, const Vector2D& delta, SP<Layout::ITarget> target, Layout::eRectCorner corner);
}
