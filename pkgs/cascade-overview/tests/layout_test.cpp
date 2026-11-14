#include "Layout.hpp"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    const auto wide = Cascade::Geometry::make(1600, 900, 480, 340);
    assert(wide.cardWidth == 480);
    assert(wide.cardHeight == 270);
    assert(wide.pitchY >= wide.cardHeight + 70);
    const auto small = Cascade::Geometry::make(640, 360, 480, 340);
    assert(small.cardWidth < 480 && small.cardWidth > 0);
    assert(small.cardHeight < 220 && small.cardHeight > 0);
    const auto fitted = Cascade::fit(0, 0, 200, 100, 100, 100);
    assert(fitted.x == 50 && fitted.y == 0 && fitted.w == 100 && fitted.h == 100);
    assert(Cascade::swipeAxis(2, 3) == 0);
    assert(Cascade::swipeAxis(15, 9) == 1);
    assert(Cascade::swipeAxis(9, -15) == 2);
    Cascade::Motion at60{0, 1}, at120{0, 1};
    for (int i = 0; i < 30; ++i) {
        const double previous = at60.value;
        at60.tick(1. / 60, true);
        assert(at60.value >= previous && at60.value <= 1);
        at120.tick(1. / 120, true);
        at120.tick(1. / 120, true);
        assert(std::abs(at60.value - at120.value) < .001);
    }
    assert(at60.value > .99);
    at60.target = 0;
    const double beforeReverse = at60.value;
    at60.tick(0, true);
    assert(at60.value == beforeReverse);
    for (int i = 0; i < 60; ++i) at60.tick(1. / 60, true);
    assert(at60.value == 0 && at60.velocity == 0);
    at60.target = 7;
    at60.tick(.016, false);
    assert(at60.value == 7 && at60.velocity == 0);
    std::cout << "5 layout tests passed\n";
}
