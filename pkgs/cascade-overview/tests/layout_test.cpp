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
    assert(Cascade::approach(0, 10, 0, true) == 0);
    assert(Cascade::approach(0, 10, 0.016, true) > 0);
    assert(Cascade::approach(0, 10, 0.016, true) < 10);
    assert(Cascade::approach(0, 10, 0.016, false) == 10);
    std::cout << "4 layout tests passed\n";
}
