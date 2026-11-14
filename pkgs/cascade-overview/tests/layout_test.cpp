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
    const auto closed = Cascade::overviewPhase(0), open = Cascade::overviewPhase(1);
    assert(closed.blur == 0 && closed.zoom == 0 && closed.chrome == 0);
    assert(open.blur == 1 && open.zoom == 1 && open.chrome == 1);
    const auto early = Cascade::overviewPhase(.05), middle = Cascade::overviewPhase(.5);
    assert(early.blur > 0 && early.zoom == 0 && early.chrome == 0);
    assert(middle.blur > middle.zoom && middle.zoom > middle.chrome);
    for (int i = 0; i <= 100; ++i) {
        const auto phase = Cascade::overviewPhase(i / 100.);
        assert(phase.blur >= 0 && phase.blur <= 1 && phase.zoom >= 0 && phase.zoom <= 1);
        assert(phase.chrome >= 0 && phase.chrome <= phase.zoom);
    }
    Cascade::Motion entry{0, 1}, exit{1, 0}, fade{0, 1}, previousEntry{0, 1}, previousExit{1, 0};
    entry.tick(.1, true, Cascade::overviewEnterRate);
    exit.tick(.1, true, Cascade::overviewExitRate);
    fade.tick(.1, true, Cascade::desktopFadeRate);
    previousEntry.tick(.1, true, 22);
    previousExit.tick(.1, true, 30);
    assert(entry.value > 0 && entry.value < previousEntry.value);
    assert(exit.value < 1 && exit.value > previousExit.value);
    assert(fade.value > 0 && fade.value < entry.value);
    for (const double rate : {Cascade::overviewEnterRate, Cascade::overviewExitRate, Cascade::desktopFadeRate}) {
        Cascade::Motion slow60{0, 1}, slow120{0, 1};
        for (int i = 0; i < 60; ++i) {
            slow60.tick(1. / 60, true, rate);
            slow120.tick(1. / 120, true, rate);
            slow120.tick(1. / 120, true, rate);
            assert(std::abs(slow60.value - slow120.value) < .001);
        }
        assert(slow60.value == 1 && slow120.value == 1);
    }
    const Cascade::Rect screen{0, 0, 1280, 800}, card{400, 262, 480, 300};
    const auto full = Cascade::interpolate(screen, card, 0), thumbnail = Cascade::interpolate(screen, card, 1);
    assert(full.x == 0 && full.y == 0 && full.w == 1280 && full.h == 800);
    assert(thumbnail.x == 400 && thumbnail.y == 262 && thumbnail.w == 480 && thumbnail.h == 300);
    const auto halfway = Cascade::interpolate(screen, card, .5);
    assert(halfway.x == 200 && halfway.y == 131 && halfway.w == 880 && halfway.h == 550);
    std::cout << "7 layout tests passed\n";
}
