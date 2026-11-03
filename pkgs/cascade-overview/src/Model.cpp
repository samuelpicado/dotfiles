#include "Model.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>
#include <utility>

namespace Cascade {

namespace {

bool validName(const std::string& name) {
    return !name.empty() && name.size() <= 120 &&
        std::none_of(name.begin(), name.end(), [](unsigned char character) { return character < 32 || character == 127; });
}

Id firstFree(const std::set<Id>& ids) {
    Id candidate = 1;
    while (ids.contains(candidate)) {
        if (candidate == std::numeric_limits<Id>::max())
            return 0;
        ++candidate;
    }
    return candidate;
}

Id nextDesktop(const std::vector<Group>& groups, const std::vector<Id>& occupied) {
    std::set<Id> ids(occupied.begin(), occupied.end());
    for (const auto& group : groups)
        ids.insert(group.desktops.begin(), group.desktops.end());
    return firstFree(ids);
}

bool validState(const std::vector<Group>& groups, Id activeGroup) {
    std::set<Id> groupIds;
    std::set<Id> desktops;
    for (const auto& group : groups) {
        if (group.id <= 0 || !groupIds.insert(group.id).second || !validName(group.name) || group.desktops.empty() ||
            std::find(group.desktops.begin(), group.desktops.end(), group.selected) == group.desktops.end())
            return false;
        for (const Id desktop : group.desktops)
            if (desktop <= 0 || !desktops.insert(desktop).second)
                return false;
    }
    return groupIds.contains(activeGroup);
}

void repairSelection(Group& group, size_t removedIndex) {
    if (std::find(group.desktops.begin(), group.desktops.end(), group.selected) == group.desktops.end())
        group.selected = group.desktops[std::min(removedIndex, group.desktops.size() - 1)];
}

size_t shifted(size_t index, int delta, size_t count) {
    return static_cast<size_t>(std::clamp<int64_t>(static_cast<int64_t>(index) + delta, 0, static_cast<int64_t>(count) - 1));
}

}

Model::Model() : groups{{1, "Principal", {}, 0}} {}

Group& Model::current() {
    auto* found = group(activeGroup);
    return found ? *found : groups.front();
}

const Group& Model::current() const {
    const auto* found = group(activeGroup);
    return found ? *found : groups.front();
}

Group* Model::group(Id id) {
    const auto found = std::find_if(groups.begin(), groups.end(), [id](const auto& group) { return group.id == id; });
    return found == groups.end() ? nullptr : &*found;
}

const Group* Model::group(Id id) const {
    const auto found = std::find_if(groups.begin(), groups.end(), [id](const auto& group) { return group.id == id; });
    return found == groups.end() ? nullptr : &*found;
}

Id Model::groupFor(Id desktop) const {
    for (const auto& group : groups)
        if (std::find(group.desktops.begin(), group.desktops.end(), desktop) != group.desktops.end())
            return group.id;
    return 0;
}

void Model::adopt(const std::vector<Id>& live, Id activeDesktop) {
    for (const Id desktop : live) {
        if (desktop <= 0 || groupFor(desktop))
            continue;
        auto& target = current();
        target.desktops.push_back(desktop);
        if (!target.selected)
            target.selected = desktop;
    }
    select(activeDesktop);
}

Id Model::createDesktop(Id groupId, const std::vector<Id>& occupied) {
    auto* target = group(groupId);
    if (!target)
        return 0;
    const Id desktop = nextDesktop(groups, occupied);
    if (desktop) {
        target->desktops.push_back(desktop);
        target->selected = desktop;
    }
    return desktop;
}

bool Model::ensureTrailingEmpty(const std::vector<Id>& nonempty, const std::vector<Id>& occupied) {
    const std::set<Id> busy(nonempty.begin(), nonempty.end());
    bool changed = false;
    for (auto& target : groups) {
        while (target.desktops.size() > 1 && !busy.contains(target.desktops.back()) &&
               !busy.contains(target.desktops[target.desktops.size() - 2])) {
            const auto removed = target.desktops[target.desktops.size() - 2];
            target.desktops.erase(target.desktops.end() - 2);
            if (target.selected == removed) target.selected = target.desktops.back();
            changed = true;
        }
        const bool initial = target.desktops.empty();
        if (!initial && !busy.contains(target.desktops.back())) continue;
        const Id desktop = nextDesktop(groups, occupied);
        if (!desktop) continue;
        target.desktops.push_back(desktop);
        if (initial) target.selected = desktop;
        changed = true;
    }
    return changed;
}

Id Model::createGroup(std::string name, const std::vector<Id>& occupied) {
    if (!validName(name))
        return 0;
    std::set<Id> ids;
    for (const auto& group : groups)
        ids.insert(group.id);
    const Id id = firstFree(ids);
    const Id desktop = nextDesktop(groups, occupied);
    if (!id || !desktop)
        return 0;
    groups.push_back({id, std::move(name), {desktop}, desktop});
    activeGroup = id;
    return id;
}

bool Model::select(Id desktop) {
    const Id id = groupFor(desktop);
    if (!id)
        return false;
    activeGroup = id;
    current().selected = desktop;
    return true;
}

Id Model::navigate(int dx, int dy) {
    auto& source = current();
    if (!source.desktops.empty()) {
        const auto selected = std::find(source.desktops.begin(), source.desktops.end(), source.selected);
        const auto index = selected == source.desktops.end() ? 0 : static_cast<size_t>(selected - source.desktops.begin());
        source.selected = source.desktops[shifted(index, dx, source.desktops.size())];
    }
    const auto active = std::find_if(groups.begin(), groups.end(), [this](const auto& group) { return group.id == activeGroup; });
    const auto index = active == groups.end() ? 0 : static_cast<size_t>(active - groups.begin());
    activeGroup = groups[shifted(index, dy, groups.size())].id;
    return current().selected;
}

bool Model::moveGroup(Id id, int delta) {
    const auto found = std::find_if(groups.begin(), groups.end(), [id](const auto& group) { return group.id == id; });
    if (found == groups.end())
        return false;
    const auto index = static_cast<size_t>(found - groups.begin());
    const auto target = shifted(index, delta, groups.size());
    if (index == target)
        return false;
    auto moving = std::move(*found);
    groups.erase(found);
    groups.insert(groups.begin() + target, std::move(moving));
    return true;
}

bool Model::moveDesktop(Id desktop, Id targetGroup, size_t index) {
    auto* source = group(groupFor(desktop));
    auto* target = group(targetGroup);
    if (!source || !target || (source != target && source->desktops.size() == 1))
        return false;
    const auto found = std::find(source->desktops.begin(), source->desktops.end(), desktop);
    const auto removedIndex = static_cast<size_t>(found - source->desktops.begin());
    source->desktops.erase(found);
    target->desktops.insert(target->desktops.begin() + std::min(index, target->desktops.size()), desktop);
    repairSelection(*source, removedIndex);
    repairSelection(*target, index);
    return true;
}

bool Model::removeDesktop(Id desktop, const std::vector<Id>& nonempty) {
    auto* source = group(groupFor(desktop));
    if (!source || source->desktops.size() <= 1 || std::find(nonempty.begin(), nonempty.end(), desktop) != nonempty.end())
        return false;
    const auto found = std::find(source->desktops.begin(), source->desktops.end(), desktop);
    const auto index = static_cast<size_t>(found - source->desktops.begin());
    source->desktops.erase(found);
    repairSelection(*source, index);
    return true;
}

bool Model::removeGroup(Id id, const std::vector<Id>& nonempty) {
    const auto found = std::find_if(groups.begin(), groups.end(), [id](const auto& group) { return group.id == id; });
    if (found == groups.end() || groups.size() <= 1 ||
        std::any_of(found->desktops.begin(), found->desktops.end(), [&nonempty](Id desktop) {
            return std::find(nonempty.begin(), nonempty.end(), desktop) != nonempty.end();
        }))
        return false;
    const auto index = static_cast<size_t>(found - groups.begin());
    groups.erase(found);
    if (activeGroup == id)
        activeGroup = groups[std::min(index, groups.size() - 1)].id;
    return true;
}

bool Model::load(const std::filesystem::path& path) {
    std::ifstream input(path);
    std::string magic;
    int version;
    Id active;
    Id groupCount;
    if (!(input >> magic >> version >> active >> groupCount) || magic != "CASCADE_OVERVIEW" || version != 1 || groupCount <= 0)
        return false;
    std::vector<Group> loaded;
    for (Id i = 0; i < groupCount; ++i) {
        Group group;
        Id desktopCount;
        if (!(input >> group.id))
            return false;
        input >> std::ws;
        if (input.peek() != '"' || !(input >> std::quoted(group.name) >> group.selected >> desktopCount) || desktopCount <= 0)
            return false;
        for (Id j = 0; j < desktopCount; ++j) {
            Id desktop;
            if (!(input >> desktop))
                return false;
            group.desktops.push_back(desktop);
        }
        loaded.push_back(std::move(group));
    }
    input >> std::ws;
    if (!input.eof() || input.bad() || !validState(loaded, active))
        return false;
    groups = std::move(loaded);
    activeGroup = active;
    return true;
}

bool Model::save(const std::filesystem::path& path) const {
    if (!validState(groups, activeGroup))
        return false;
    auto temporary = path;
    temporary += ".tmp-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    std::error_code error;
    if (!std::filesystem::create_directory(temporary, error))
        return false;
    const auto file = temporary / "state";
    std::ofstream output(file);
    output << "CASCADE_OVERVIEW 1\n" << activeGroup << ' ' << groups.size() << '\n';
    for (const auto& group : groups) {
        output << group.id << ' ' << std::quoted(group.name) << ' ' << group.selected << ' ' << group.desktops.size();
        for (const Id desktop : group.desktops)
            output << ' ' << desktop;
        output << '\n';
    }
    output.close();
    const bool written = !output.fail();
    if (written)
        std::filesystem::rename(file, path, error);
    const bool saved = written && !error;
    std::filesystem::remove_all(temporary, error);
    return saved;
}

}
