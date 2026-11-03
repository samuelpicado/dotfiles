#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Cascade {

using Id = int64_t;

struct Group {
    Id id;
    std::string name;
    std::vector<Id> desktops;
    Id selected = 0;
};

class Model {
  public:
    std::vector<Group> groups;
    Id activeGroup = 1;

    Model();
    Group& current();
    const Group& current() const;
    Group* group(Id id);
    const Group* group(Id id) const;
    Id groupFor(Id desktop) const;
    void adopt(const std::vector<Id>& live, Id activeDesktop);
    Id createDesktop(Id groupId, const std::vector<Id>& occupied);
    bool ensureTrailingEmpty(const std::vector<Id>& nonempty, const std::vector<Id>& occupied);
    Id createGroup(std::string name, const std::vector<Id>& occupied);
    bool select(Id desktop);
    Id navigate(int dx, int dy);
    bool moveGroup(Id id, int delta);
    bool moveDesktop(Id desktop, Id targetGroup, size_t index);
    bool removeDesktop(Id desktop, const std::vector<Id>& nonempty);
    bool removeGroup(Id group, const std::vector<Id>& nonempty);
    bool load(const std::filesystem::path& path);
    bool save(const std::filesystem::path& path) const;
};

}
