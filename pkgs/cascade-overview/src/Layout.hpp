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
inline Rect interpolate(Rect from, Rect to, double t) {
    return {from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t,
            from.w + (to.w - from.w) * t, from.h + (to.h - from.h) * t};
}
struct OverviewPhase { double blur, zoom, chrome; };
inline OverviewPhase overviewPhase(double progress) {
    auto phase = [](double value, double start, double end) {
        const double t = std::clamp((value - start) / (end - start), 0., 1.);
        return t * t * (3 - 2 * t);
    };
    const double zoom = phase(progress, .06, .995);
    return {phase(progress, 0, .6), zoom, zoom * phase(zoom, .72, 1)};
}
inline constexpr double overviewEnterRate = 18;
inline constexpr double overviewExitRate = 24;
inline constexpr double desktopFadeRate = 16;
inline Rect fit(double x, double y, double w, double h, double sourceWidth, double sourceHeight) {
    const double scale = std::min(w / std::max(sourceWidth, 1.0), h / std::max(sourceHeight, 1.0));
    const double fw = sourceWidth * scale, fh = sourceHeight * scale;
    return {x + (w - fw) / 2, y + (h - fh) / 2, fw, fh};
}
inline int swipeAxis(double x, double y) {
    if (std::max(std::abs(x), std::abs(y)) < 8) return 0;
    return std::abs(x) > std::abs(y) ? 1 : 2;
}
struct Motion {
    double value = 0, target = 0, velocity = 0;
    void tick(double dt, bool animate, double rate = 22) {
        if (!animate || (std::abs(target - value) < .001 && std::abs(velocity) < .01)) {
            value = target;
            velocity = 0;
            return;
        }
        const double step = std::clamp(dt, 0., .1), decay = std::exp(-rate * step);
        const double offset = value - target, momentum = velocity + rate * offset;
        value = target + (offset + momentum * step) * decay;
        velocity = (velocity - rate * momentum * step) * decay;
    }
};
}
