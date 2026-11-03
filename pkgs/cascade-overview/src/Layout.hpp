#pragma once
#include <algorithm>
#include <cmath>

namespace Cascade {
struct Geometry {
    double cardWidth, cardHeight, pitchX, pitchY, centerY;
    static Geometry make(double width, double height, double requestedWidth, double requestedHeight) {
        const double aspect = std::max(width, 1.0) / std::max(height, 1.0);
        const double cardWidth = std::max(80.0, std::min({requestedWidth, width * .72, (height * .62 - 48) * aspect}));
        const double cardHeight = cardWidth / aspect;
        return {cardWidth, cardHeight, cardWidth + 24, std::max(cardHeight + 76, std::min(requestedHeight, height * .72)), height / 2 + 12};
    }
};
struct Rect { double x, y, w, h; };
inline Rect fit(double x, double y, double w, double h, double sourceWidth, double sourceHeight) {
    const double scale = std::min(w / std::max(sourceWidth, 1.0), h / std::max(sourceHeight, 1.0));
    const double fw = sourceWidth * scale, fh = sourceHeight * scale;
    return {x + (w - fw) / 2, y + (h - fh) / 2, fw, fh};
}
inline int swipeAxis(double x, double y) {
    if (std::max(std::abs(x), std::abs(y)) < 8) return 0;
    return std::abs(x) > std::abs(y) ? 1 : 2;
}
inline double approach(double value, double target, double dt, bool animate) {
    if (!animate || std::abs(target - value) < .001) return target;
    return value + (target - value) * (1 - std::exp(-18 * std::clamp(dt, 0.0, .1)));
}
}
