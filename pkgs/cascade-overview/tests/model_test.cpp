#include "../src/Model.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

using Cascade::Id;
using Cascade::Model;

#define CHECK(condition) do { if (!(condition)) throw std::runtime_error(std::string(__func__) + ": " + #condition); } while (false)

static std::string snapshot(const Model& model) {
    std::ostringstream out;
    out << model.activeGroup << '|';
    for (const auto& group : model.groups) {
        out << group.id << ':' << group.name << ':' << group.selected << ':';
        for (const auto desktop : group.desktops)
            out << desktop << ',';
        out << '|';
    }
    return out.str();
}

static void adoptPreservesEmptyDesktops() {
    Model model;
    CHECK(model.current().id == 1);
    model.adopt({1, 2, 2, 0, -1}, 2);
    CHECK(model.current().desktops == std::vector<Id>({1, 2}));
    CHECK(model.current().selected == 2);
    model.adopt({2, 9}, 9);
    CHECK(model.current().desktops == std::vector<Id>({1, 2, 9}));
    CHECK(model.current().selected == 9);
    CHECK(model.groupFor(1) == 1);
    CHECK(model.groupFor(0) == 0);
    CHECK(model.group(888) == nullptr);
    CHECK(!model.select(888));
    CHECK(model.current().selected == 9);
}

static void createWithoutFixedLimitsOrCollisions() {
    Model model;
    model.adopt({1, 4}, 1);
    CHECK(model.createDesktop(999, {}) == 0);
    CHECK(model.createDesktop(1, {2, 3, 5}) == 6);
    std::set<Id> ids = {1, 4, 6};
    for (int i = 0; i < 24; ++i) {
        const Id group = model.createGroup("Grupo " + std::to_string(i), {2, 3, 5, 7});
        CHECK(group != 0);
        CHECK(model.current().id == group);
        const auto desktop = model.current().selected;
        CHECK(desktop > 7);
        CHECK(ids.insert(desktop).second);
    }
    CHECK(model.groups.size() == 25);
    for (int i = 0; i < 24; ++i) {
        const Id desktop = model.createDesktop(1, {2, 3, 5, 7});
        CHECK(desktop > 7);
        CHECK(ids.insert(desktop).second);
    }
    CHECK(model.group(1)->desktops.size() == 27);
}

static void trailingEmptyDesktopsPreserveOrderAndSelection() {
    const std::vector<Id> occupied = {1, 2, 3, 4, 5, 8, 9, 13, 14};
    Model initial;
    CHECK(initial.ensureTrailingEmpty({}, occupied));
    CHECK(initial.current().selected == initial.current().desktops.back());
    CHECK(initial.current().desktops.size() == 1);
    const auto initialState = snapshot(initial);
    CHECK(!initial.ensureTrailingEmpty({}, occupied));
    CHECK(snapshot(initial) == initialState);

    Model model;
    model.adopt({10, 20, 30}, 20);
    const auto second = model.createGroup("Segundo", occupied);
    const auto secondFirst = model.current().selected;
    const auto secondLast = model.createDesktop(second, occupied);
    CHECK(model.select(secondFirst));
    const auto manual = model.createGroup("Vacios manuales", occupied);
    const auto manualFirst = model.current().selected;
    const auto manualLast = model.createDesktop(manual, occupied);
    CHECK(manualLast);
    CHECK(model.select(manualFirst));
    std::vector<Id> nonempty = {30, secondLast};
    for (int i = 0; i < 13; ++i) {
        CHECK(model.createGroup("Extra " + std::to_string(i), occupied));
        nonempty.push_back(model.current().selected);
    }
    CHECK(model.select(20));
    const auto before = model.groups;
    const auto active = model.activeGroup;
    CHECK(model.ensureTrailingEmpty(nonempty, occupied));
    CHECK(model.activeGroup == active);
    CHECK(model.groups.size() == before.size());
    std::set<Id> ids(occupied.begin(), occupied.end());
    for (size_t i = 0; i < before.size(); ++i) {
        const auto& original = before[i];
        const auto& group = *model.group(original.id);
        const bool appended = std::find(nonempty.begin(), nonempty.end(), original.desktops.back()) != nonempty.end();
        CHECK(model.groups[i].id == original.id);
        CHECK(group.name == original.name);
        if (original.id == manual) {
            CHECK(group.desktops == std::vector<Id>({manualLast}));
            CHECK(group.selected == manualLast);
        } else {
            CHECK(group.selected == original.selected);
            CHECK(group.desktops.size() == original.desktops.size() + appended);
            CHECK(std::equal(original.desktops.begin(), original.desktops.end(), group.desktops.begin()));
        }
        for (const auto desktop : group.desktops) CHECK(ids.insert(desktop).second);
    }
    const auto normalized = snapshot(model);
    CHECK(!model.ensureTrailingEmpty(nonempty, occupied));
    CHECK(snapshot(model) == normalized);
    const auto reserve = model.group(1)->desktops.back();
    const auto secondReserve = model.group(second)->desktops.back();
    const auto manualDesktops = model.group(manual)->desktops;
    nonempty.push_back(reserve);
    CHECK(model.ensureTrailingEmpty(nonempty, occupied));
    CHECK(model.group(1)->desktops.size() == 5);
    CHECK(model.group(1)->selected == 20);
    CHECK(model.group(1)->desktops[3] == reserve);
    CHECK(ids.insert(model.group(1)->desktops.back()).second);
    CHECK(model.group(second)->desktops.back() == secondReserve);
    CHECK(model.group(manual)->desktops == manualDesktops);
    CHECK(model.activeGroup == active);
    CHECK(!model.ensureTrailingEmpty(nonempty, occupied));
}

static void trailingEmptyRunKeepsLastIdAndRepairsSelection() {
    Model model;
    model.adopt({10, 20, 30, 40}, 30);
    CHECK(model.ensureTrailingEmpty({10}, {}));
    CHECK(model.current().desktops == std::vector<Id>({10, 40}));
    CHECK(model.current().selected == 40);
    CHECK(model.groupFor(20) == 0);
    CHECK(model.groupFor(30) == 0);
    CHECK(model.groupFor(40) == 1);
}

static void trailingCleanupPreservesInteriorEmptyDesktops() {
    Model model;
    model.adopt({10, 20, 30, 40, 50, 60, 70, 80, 90}, 50);
    CHECK(model.ensureTrailingEmpty({10, 40, 70}, {}));
    CHECK(model.current().desktops == std::vector<Id>({10, 20, 30, 40, 50, 60, 70, 90}));
    CHECK(model.current().selected == 50);
    CHECK(model.groupFor(80) == 0);
}

static void trailingCleanupIsIdempotent() {
    Model model;
    model.adopt({10, 20, 30, 40}, 10);
    CHECK(model.ensureTrailingEmpty({10, 20}, {}));
    CHECK(model.current().desktops == std::vector<Id>({10, 20, 40}));
    CHECK(model.current().selected == 10);
    const auto normalized = snapshot(model);
    CHECK(!model.ensureTrailingEmpty({10, 20}, {}));
    CHECK(snapshot(model) == normalized);
    CHECK(!model.ensureTrailingEmpty({10, 20}, {1, 2, 3, 30}));
    CHECK(snapshot(model) == normalized);
}

static void allEmptyGroupsKeepTheirLastDesktop() {
    Model model;
    model.adopt({10, 20, 30}, 20);
    model.group(1)->name = "Principal vacio";
    const auto second = model.createGroup("Segundo vacio", {});
    const auto secondFirst = model.current().selected;
    CHECK(model.createDesktop(second, {}));
    const auto secondLast = model.createDesktop(second, {});
    CHECK(model.select(secondFirst));
    const auto third = model.createGroup("Unico vacio", {});
    const auto thirdDesktop = model.current().selected;
    CHECK(model.moveGroup(second, -1));
    CHECK(model.select(20));
    CHECK(model.ensureTrailingEmpty({}, {}));
    CHECK(model.groups.size() == 3);
    CHECK(model.groups[0].id == second);
    CHECK(model.groups[1].id == 1);
    CHECK(model.groups[2].id == third);
    CHECK(model.activeGroup == 1);
    CHECK(model.group(1)->name == "Principal vacio");
    CHECK(model.group(second)->name == "Segundo vacio");
    CHECK(model.group(third)->name == "Unico vacio");
    CHECK(model.group(1)->desktops == std::vector<Id>({30}));
    CHECK(model.group(1)->selected == 30);
    CHECK(model.group(second)->desktops == std::vector<Id>({secondLast}));
    CHECK(model.group(second)->selected == secondLast);
    CHECK(model.group(third)->desktops == std::vector<Id>({thirdDesktop}));
    CHECK(model.group(third)->selected == thirdDesktop);
    const auto normalized = snapshot(model);
    CHECK(!model.ensureTrailingEmpty({}, {}));
    CHECK(snapshot(model) == normalized);
}

static void manyTrailingEmptiesRepairRemovedSelection() {
    Model model;
    std::vector<Id> desktops;
    for (Id desktop = 1000; desktop <= 1255; ++desktop) desktops.push_back(desktop);
    model.adopt(desktops, 1100);
    CHECK(model.ensureTrailingEmpty({1000}, desktops));
    CHECK(model.current().desktops == std::vector<Id>({1000, 1255}));
    CHECK(model.current().selected == 1255);
    CHECK(model.groupFor(1100) == 0);
    CHECK(model.activeGroup == 1);
    CHECK(!model.ensureTrailingEmpty({1000}, desktops));
}

static void navigationRemembersSelectionAndClamps() {
    Model model;
    model.adopt({10, 20, 30}, 20);
    const Id second = model.createGroup("Segundo", {1, 2});
    const Id firstSecond = model.current().selected;
    const Id lastSecond = model.createDesktop(second, {});
    CHECK(model.select(20));
    CHECK(model.navigate(0, 1) == lastSecond);
    CHECK(model.navigate(0, 1) == lastSecond);
    CHECK(model.navigate(-1, 0) == firstSecond);
    CHECK(model.navigate(-1, 0) == firstSecond);
    CHECK(model.navigate(0, -1) == 20);
    CHECK(model.navigate(1, 0) == 30);
    CHECK(model.navigate(1, 0) == 30);
    CHECK(model.navigate(0, -1) == 30);
    CHECK(model.navigate(0, 1) == firstSecond);
    model.adopt({90}, 90);
    CHECK(model.groupFor(90) == second);
    CHECK(model.current().selected == 90);
    const Model& view = model;
    CHECK(view.current().selected == 90);
    CHECK(view.group(second)->id == second);
}

static void orderingAndMovingKeepGroupsUsable() {
    Model model;
    model.adopt({10, 20, 30}, 20);
    const Id second = model.createGroup("Segundo", {});
    const Id original = model.current().selected;
    CHECK(model.moveGroup(second, -1));
    CHECK(model.groups.front().id == second);
    CHECK(!model.moveGroup(second, -1));
    CHECK(!model.moveGroup(999, 1));
    CHECK(model.moveDesktop(30, 1, 0));
    CHECK(model.group(1)->desktops == std::vector<Id>({30, 10, 20}));
    CHECK(model.moveDesktop(20, second, 0));
    CHECK(model.group(second)->desktops == std::vector<Id>({20, original}));
    CHECK(model.group(1)->selected == 10);
    CHECK(model.moveDesktop(10, second, 100));
    CHECK(model.group(second)->desktops == std::vector<Id>({20, original, 10}));
    CHECK(!model.moveDesktop(30, second, 0));
    CHECK(!model.moveDesktop(999, second, 0));
    CHECK(!model.moveDesktop(20, 999, 0));
    CHECK(model.group(1)->desktops == std::vector<Id>({30}));
}

static void movingIntoInitialGroupSelectsItsDesktop() {
    Model model;
    const Id second = model.createGroup("Segundo", {});
    const Id desktop = model.createDesktop(second, {});
    CHECK(model.moveDesktop(desktop, 1, 0));
    CHECK(model.group(1)->selected == desktop);
    CHECK(model.select(desktop));
    CHECK(model.navigate(0, 0) == desktop);
}

static void removalProtectsWindowsAndSelection() {
    Model model;
    model.adopt({10, 20, 30}, 20);
    CHECK(!model.removeDesktop(20, {20}));
    CHECK(model.current().selected == 20);
    CHECK(model.removeDesktop(20, {}));
    CHECK(model.current().selected == 30);
    CHECK(model.removeDesktop(30, {}));
    CHECK(model.current().selected == 10);
    CHECK(!model.removeDesktop(10, {}));
    CHECK(!model.removeGroup(1, {}));
    const Id second = model.createGroup("Segundo", {});
    const Id secondDesktop = model.current().selected;
    CHECK(!model.removeGroup(second, {secondDesktop}));
    CHECK(model.removeGroup(second, {}));
    CHECK(model.current().id == 1);
    CHECK(model.current().selected == 10);
    CHECK(!model.removeGroup(999, {}));
    CHECK(!model.removeDesktop(999, {}));
}

static void namesRejectControlsAndPreserveUtf8() {
    Model model;
    model.adopt({1}, 1);
    CHECK(model.createGroup("Trabajo — José 🦥", {}));
    CHECK(model.current().name == "Trabajo — José 🦥");
    CHECK(model.createGroup("", {}) == 0);
    CHECK(model.createGroup("mal\nnombre", {}) == 0);
    CHECK(model.createGroup(std::string("mal\0nombre", 10), {}) == 0);
    CHECK(model.createGroup(std::string(121, 'a'), {}) == 0);
    CHECK(model.createGroup("mal\tnombre", {}) == 0);
    CHECK(model.groups.size() == 2);
}

struct TemporaryDirectory {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("cascade-model-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    TemporaryDirectory() { std::filesystem::create_directories(path); }
    ~TemporaryDirectory() { std::error_code error; std::filesystem::remove_all(path, error); }
};

static void persistenceRoundTripAndInvalidLoadIsAtomic() {
    TemporaryDirectory temporary;
    const auto path = temporary.path / "state";
    Model model;
    model.adopt({10, 20}, 20);
    const Id second = model.createGroup("Diseño \\\"José\\\" 🦥", {});
    model.createDesktop(second, {});
    CHECK(model.moveGroup(second, -1));
    CHECK(model.select(20));
    CHECK(model.save(path));
    Model restored;
    CHECK(restored.load(path));
    CHECK(snapshot(restored) == snapshot(model));
    CHECK(!restored.load(temporary.path / "missing"));
    const auto before = snapshot(restored);
    const std::vector<std::string> invalid = {
        "", "CASCADE_OVERVIEW 2\n1 1\n1 \"Main\" 1 1 1\n",
        "CASCADE_OVERVIEW 1\n1 0\n",
        "CASCADE_OVERVIEW 1\n9 1\n1 \"Main\" 1 1 1\n",
        "CASCADE_OVERVIEW 1\n1 1\n0 \"Main\" 1 1 1\n",
        "CASCADE_OVERVIEW 1\n1 1\n1 \"Main\" 2 1 1\n",
        "CASCADE_OVERVIEW 1\n1 1\n1 \"Main\" 0 0\n",
        "CASCADE_OVERVIEW 1\n1 1\n1 \"Main\" 1 2 1 1\n",
        "CASCADE_OVERVIEW 1\n1 2\n1 \"A\" 1 1 1\n1 \"B\" 2 1 2\n",
        "CASCADE_OVERVIEW 1\n1 2\n1 \"A\" 1 1 1\n2 \"B\" 1 1 1\n",
        "CASCADE_OVERVIEW 1\n1 1\n1 \"Main\" -1 1 -1\n",
        "CASCADE_OVERVIEW 1\n1 1\n1 \"Main\" 1 1 1\ntrailing",
        "CASCADE_OVERVIEW 1\n1 1\n1 \"bad\nname\" 1 1 1\n",
        "CASCADE_OVERVIEW 1\n1 1\n1 Main 1 1 1\n",
        "CASCADE_OVERVIEW 1\n1 1\n1 \"" + std::string(121, 'x') + "\" 1 1 1\n",
        "CASCADE_OVERVIEW 1\n1 1\n1 \"Main\" 1 99999999999999999999999 1\n",
    };
    for (const auto& contents : invalid) {
        std::ofstream(path, std::ios::trunc) << contents;
        CHECK(!restored.load(path));
        CHECK(snapshot(restored) == before);
    }
}

static void failedSavePreservesOriginal() {
    TemporaryDirectory temporary;
    const auto path = temporary.path / "state";
    Model model;
    model.adopt({1}, 1);
    CHECK(model.save(path));
    const auto original = snapshot(model);
    model.groups.front().selected = 999;
    CHECK(!model.save(path));
    Model restored;
    CHECK(restored.load(path));
    CHECK(snapshot(restored) == original);
    CHECK(!restored.save(temporary.path / "missing" / "state"));
    CHECK(!restored.save(temporary.path));
}

int main() {
    void (*tests[])() = {
        adoptPreservesEmptyDesktops,
        createWithoutFixedLimitsOrCollisions,
        trailingEmptyDesktopsPreserveOrderAndSelection,
        trailingEmptyRunKeepsLastIdAndRepairsSelection,
        trailingCleanupPreservesInteriorEmptyDesktops,
        trailingCleanupIsIdempotent,
        allEmptyGroupsKeepTheirLastDesktop,
        manyTrailingEmptiesRepairRemovedSelection,
        navigationRemembersSelectionAndClamps,
        orderingAndMovingKeepGroupsUsable,
        movingIntoInitialGroupSelectsItsDesktop,
        removalProtectsWindowsAndSelection,
        namesRejectControlsAndPreserveUtf8,
        persistenceRoundTripAndInvalidLoadIsAtomic,
        failedSavePreservesOriginal,
    };
    size_t failures = 0;
    for (const auto test : tests) {
        try {
            test();
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            ++failures;
        }
    }
    if (failures) return 1;
    std::cout << std::size(tests) << " model tests passed\n";
}
