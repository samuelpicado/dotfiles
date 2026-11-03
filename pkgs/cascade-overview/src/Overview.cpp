#include "Overview.hpp"
#include "Globals.hpp"
#include "Layout.hpp"
#include <hyprland/src/state/MonitorState.hpp>
#include <hyprland/src/state/WorkspaceState.hpp>
#include <hyprland/src/desktop/state/WindowState.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/state/GlobalWindowController.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/pointer/PointerManager.hpp>
#include <hyprland/src/animation/WorkspaceAnimationController.hpp>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>

using Cascade::Id;

namespace {
std::string json(const std::string& value) {
    std::ostringstream out;
    out << '"';
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << int(c) << std::dec;
        else out << c;
    }
    return out.str() + '"';
}
}

void CCascadeOverview::Motion::tick(double dt, bool animate) {
    value = Cascade::approach(value, target, dt, animate);
}

CCascadeOverview::CCascadeOverview(int64_t owner) : ownerID(owner) {
    const char* state = std::getenv("XDG_STATE_HOME");
    const char* home = std::getenv("HOME");
    auto base = state && *state ? std::filesystem::path(state) : std::filesystem::path(home ? home : "/tmp") / ".local/state";
    auto monitor = getOwner();
    std::string name = monitor ? monitor->m_name : std::to_string(owner);
    for (auto& c : name) if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') c = '_';
    statePath = base / "cascade-overview" / (name + ".state");
    if (std::filesystem::exists(statePath) && !model.load(statePath)) {
        // Preserve the original metadata if a partial/manual edit cannot be read.
        std::error_code error;
        std::filesystem::copy_file(statePath, statePath.string() + ".invalid", std::filesystem::copy_options::overwrite_existing, error);
        readOnlyState = bool(error);
        notify(error ? "No se pudo leer la organización; el archivo original no se sobrescribirá" : "No se pudo leer la organización; se conservó una copia .invalid");
    }
    sync();
    targets(true);
}

CCascadeOverview::~CCascadeOverview() { cancelDesktopSwipe(); releaseComposition(); save(); }

PHLMONITOR CCascadeOverview::getOwner() { return State::monitorState()->query().id(ownerID).run(); }

std::vector<Id> CCascadeOverview::reservedDesktops() const {
    std::vector<Id> ids;
    for (const auto& group : model.groups) ids.insert(ids.end(), group.desktops.begin(), group.desktops.end());
    return ids;
}

std::vector<Id> CCascadeOverview::occupied() const {
    auto ids = cascadeReservedDesktops();
    for (const auto& ws : State::workspaceState()->workspaces()) if (ws && ws->m_id > 0) ids.push_back(ws->m_id);
    return ids;
}

std::vector<Id> CCascadeOverview::nonempty() const {
    std::vector<Id> ids;
    for (const auto& window : Desktop::windowState()->windows())
        if (window && window->m_isMapped && window->m_workspace) ids.push_back(window->m_workspace->m_id);
    return ids;
}

void CCascadeOverview::notify(const std::string& message) {
    statusText = message;
    statusUntil = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    if (!active) HyprlandAPI::addNotification(pHandle, "Cascade: " + message, CHyprColor(0.98, .72, .3, 1), 5000);
}

void CCascadeOverview::save() {
    if (readOnlyState || model.current().desktops.empty()) return;
    std::error_code directoryError;
    std::filesystem::create_directories(statePath.parent_path(), directoryError);
    if (directoryError || !model.save(statePath)) {
        if (!saveFailed) notify("No se pudo guardar la organización; revisa el directorio de estado");
        saveFailed = true;
    } else saveFailed = false;
}

void CCascadeOverview::sync() {
    const auto owner = getOwner();
    if (!owner) return;
    if (desktopSwipeSource && owner->m_activeWorkspace != desktopSwipeSource) cancelDesktopSwipe();
    // Hyprland may reuse an unrealized empty ID when connecting another monitor.
    // Replace only the stale association; windows on that monitor stay untouched.
    const auto registered = reservedDesktops();
    for (const auto desktop : registered) {
        const auto ws = State::workspaceState()->query().id(desktop).run();
        if (!ws || !ws->m_monitor || ws->m_monitor->m_id == ownerID) continue;
        auto& group = *model.group(model.groupFor(desktop));
        const auto selected = group.selected;
        const auto position = std::find(group.desktops.begin(), group.desktops.end(), desktop) - group.desktops.begin();
        const auto replacement = model.createDesktop(group.id, occupied());
        if (!replacement) continue;
        model.removeDesktop(desktop, {});
        model.moveDesktop(replacement, group.id, position);
        group.selected = selected == desktop ? replacement : selected;
    }
    std::vector<Id> ids;
    const auto used = nonempty();
    for (const auto& ws : State::workspaceState()->workspaces())
        if (ws && ws->m_id > 0 && ws->m_monitor && ws->m_monitor->m_id == ownerID &&
            (ws == owner->m_activeWorkspace || std::find(used.begin(), used.end(), ws->m_id) != used.end())) ids.push_back(ws->m_id);
    const auto desktop = owner->m_activeWorkspace ? owner->m_activeWorkspace->m_id : 0;
    std::sort(ids.begin(), ids.end());
    model.adopt(ids, desktop);
    if (model.current().desktops.empty()) model.createDesktop(model.activeGroup, occupied());
    targets();
    save();
}

void CCascadeOverview::targets(bool warp) {
    cascadeRefreshEmptyDesktops();
    for (size_t i = 0; i < model.groups.size(); ++i) {
        const auto& group = model.groups[i];
        if (group.id == model.activeGroup) row.target = i;
        const auto selected = std::find(group.desktops.begin(), group.desktops.end(), group.selected);
        columns[group.id].target = selected == group.desktops.end() ? 0 : selected - group.desktops.begin();
        if (warp) columns[group.id].value = columns[group.id].target;
    }
    if (warp) row.value = row.target;
    if (const auto owner = getOwner()) { g_pHyprRenderer->damageMonitor(owner); owner->scheduleFrame(); }
}

void CCascadeOverview::refreshEmptyDesktop() {
    const auto owner = getOwner();
    const auto native = owner ? owner->m_activeWorkspace : PHLWORKSPACE{};
    const auto nativeGroup = native ? model.groupFor(native->m_id) : 0;
    if (!model.ensureTrailingEmpty(nonempty(), occupied())) return;
    if ((desktopSwipeSource && !model.groupFor(desktopSwipeSource->m_id)) ||
        (desktopSwipeTarget && !model.groupFor(desktopSwipeTarget->m_id))) cancelDesktopSwipe();
    if (nativeGroup && !model.groupFor(native->m_id)) {
        const auto selected = model.current().selected;
        focus(model.group(nativeGroup)->desktops.back());
        model.select(selected);
    }
    if (pressedHit && pressedHit->desktop && !model.groupFor(pressedHit->desktop)) {
        pressedHit.reset();
        dragging = false;
    }
    std::erase_if(hits, [&](const auto& hit) { return hit.desktop && !model.groupFor(hit.desktop); });
    targets();
    save();
}

void CCascadeOverview::show() {
    if (active) return;
    cancelDesktopSwipe();
    sync();
    if (const auto owner = getOwner(); owner && !compositionLocked) {
        // A counted software-cursor lock prevents direct scanout from bypassing the overlay.
        Pointer::mgr()->lockSoftwareForMonitor(owner);
        compositionLocked = true;
        cascadeCursorOverride(true);
    }
    active = true;
    reveal.target = 1;
    keyboardWindow.reset();
    lastFrame = std::chrono::steady_clock::now();
    targets();
}

void CCascadeOverview::releaseComposition() {
    // Restoring the hardware cursor starts its own GL render and must not nest inside a frame.
    if (!compositionLocked || g_pHyprRenderer->m_renderData.pMonitor) return;
    cascadeCursorOverride(false);
    if (const auto owner = getOwner()) Pointer::mgr()->unlockSoftwareForMonitor(owner);
    compositionLocked = false;
}

void CCascadeOverview::beforeRender() {
    if (desktopSwipePending) {
        updateDesktopSwipe();
        // The scheduler checked scanout before these frame-batched offsets changed.
        if (const auto owner = getOwner()) owner->recheckSolitary();
    }
    if (!active && reveal.value < .001) releaseComposition();
}

void CCascadeOverview::hide(bool immediate) {
    if (immediate) cancelDesktopSwipe();
    active = false;
    reveal.target = 0;
    if (immediate || !config.animate->value()) {
        reveal.value = 0;
        releaseComposition();
    }
    dragging = false;
    pressedHit.reset();
    keyboardWindow.reset();
    save();
    targets();
}

void CCascadeOverview::focus(Id desktop, bool close) {
    cancelDesktopSwipe();
    auto owner = getOwner();
    if (!owner || desktop <= 0 || !model.groupFor(desktop)) return;
    auto ws = State::workspaceState()->query().id(desktop).run();
    if (ws && ws->m_monitor && ws->m_monitor->m_id != ownerID) {
        notify("Ese escritorio está abierto en otro monitor");
        return;
    }
    if (!ws) ws = State::workspaceState()->create(desktop, ownerID);
    if (!ws) { notify("No se pudo crear el escritorio"); return; }
    const auto previous = owner->m_activeWorkspace;
    const auto fromGroup = previous ? model.groupFor(previous->m_id) : 0;
    const auto toGroup = model.groupFor(desktop);
    const bool vertical = fromGroup != toGroup;
    bool forward = previous && desktop > previous->m_id;
    if (fromGroup && vertical) {
        const auto from = std::find_if(model.groups.begin(), model.groups.end(), [&](const auto& group) { return group.id == fromGroup; });
        const auto to = std::find_if(model.groups.begin(), model.groups.end(), [&](const auto& group) { return group.id == toGroup; });
        forward = to > from;
    } else if (fromGroup) {
        const auto& desktops = model.group(toGroup)->desktops;
        forward = std::find(desktops.begin(), desktops.end(), desktop) > std::find(desktops.begin(), desktops.end(), previous->m_id);
    }
    model.select(desktop);
    const bool background = owner != Desktop::focusState()->monitor();
    owner->changeWorkspace(ws, false, background, background);
    if (previous && previous != ws) {
        // Workspace IDs stay stable when cards move; animation follows their visual order.
        const std::string style = vertical ? "slidevert" : "slide";
        Animation::Workspace::startAnimation(previous, Animation::Workspace::ANIMATION_TYPE_OUT, forward, !config.animate->value(), style);
        Animation::Workspace::startAnimation(ws, Animation::Workspace::ANIMATION_TYPE_IN, forward, !config.animate->value(), style);
    }
    targets();
    save();
    if (close) hide();
}

bool CCascadeOverview::moveWindow(PHLWINDOW window, Id desktop) {
    auto owner = getOwner();
    if (!owner || !window || !window->m_isMapped || desktop <= 0) return false;
    auto ws = State::workspaceState()->query().id(desktop).run();
    if (ws && ws->m_monitor && ws->m_monitor->m_id != ownerID) { notify("El escritorio pertenece a otro monitor"); return false; }
    if (!ws) ws = State::workspaceState()->create(desktop, ownerID);
    if (ws) Desktop::globalWindowController()->moveWindowToWorkspace(window, ws);
    return bool(ws);
}

void CCascadeOverview::navigate(int dx, int dy, bool moving) {
    if (!active) sync();
    const auto window = keyboardWindow.lock() ? keyboardWindow.lock() : Desktop::focusState()->window();
    const auto desktop = model.navigate(dx, dy);
    if (moving) moveWindow(window, desktop);
    keyboardWindow.reset();
    targets();
    if (!active) {
        focus(desktop);
    }
    save();
}

bool CCascadeOverview::command(const std::string& line, std::string& error) {
    if (!active) sync();
    const auto split = line.find(' ');
    const auto verb = line.substr(0, split);
    const auto arg = split == std::string::npos ? "" : line.substr(split + 1);
    const auto current = model.activeGroup;
    bool ok = true;
    if (verb == "group-new") {
        ok = model.createGroup("Grupo " + std::to_string(model.groups.size() + 1), occupied()) > 0;
    } else if (verb == "desktop-new") {
        ok = model.createDesktop(current, occupied()) > 0;
    } else if (verb == "group-up" || verb == "group-down") ok = model.moveGroup(current, verb == "group-up" ? -1 : 1);
    else if (verb == "group-delete") ok = model.removeGroup(current, nonempty());
    else if (verb == "desktop-delete") ok = model.removeDesktop(model.current().selected, nonempty());
    else if (verb == "desktop-left" || verb == "desktop-right") {
        const auto& group = model.current();
        const auto position = std::find(group.desktops.begin(), group.desktops.end(), group.selected) - group.desktops.begin();
        ok = model.moveDesktop(group.selected, current, std::clamp<int64_t>(position + (verb == "desktop-left" ? -1 : 1), 0, group.desktops.size() - 1));
    } else if (verb == "desktop-to") {
        Id target = 0;
        const auto result = std::from_chars(arg.data(), arg.data() + arg.size(), target);
        ok = result.ec == std::errc{} && result.ptr == arg.data() + arg.size() && model.group(target);
        if (ok) {
            const auto selected = model.current().selected;
            if (target != current && model.current().desktops.size() == 1) model.createDesktop(current, occupied());
            ok = model.moveDesktop(selected, target, model.group(target)->desktops.size());
        }
    } else if (verb == "desktop-select" || verb == "window-to") {
        const auto window = Desktop::focusState()->window();
        if (verb == "window-to" && (!window || !window->m_isMapped)) {
            error = "No hay una ventana activa para mover";
            return false;
        }
        size_t ordinal = 0;
        const auto result = std::from_chars(arg.data(), arg.data() + arg.size(), ordinal);
        ok = result.ec == std::errc{} && result.ptr == arg.data() + arg.size() && ordinal > 0;
        if (ok && ordinal <= 10) while (model.current().desktops.size() < ordinal) model.createDesktop(current, occupied());
        ok = ok && ordinal <= model.current().desktops.size();
        if (ok) {
            const auto desktop = model.current().desktops[ordinal - 1];
            if (verb == "window-to") ok = moveWindow(window, desktop);
            if (ok) model.select(desktop);
        }
    } else { error = "Comando desconocido: " + verb; return false; }
    if (!ok) { error = "No se puede realizar: conserva un escritorio por grupo y elimina solo espacios vacíos"; return false; }
    targets();
    keyboardWindow.reset();
    save();
    const auto owner = getOwner();
    const bool removedActive = owner && owner->m_activeWorkspace && !model.groupFor(owner->m_activeWorkspace->m_id);
    if (!active || removedActive) focus(model.current().selected);
    return true;
}

std::string CCascadeOverview::inspect() {
    std::ostringstream out;
    out << "{\"active\":" << (active ? "true" : "false") << ",\"monitor\":" << ownerID << ",\"activeGroup\":" << model.activeGroup << ",\"selected\":" << model.current().selected << ",\"renaming\":false,\"status\":" << json(std::chrono::steady_clock::now() < statusUntil ? statusText : "") << ",\"statePath\":" << json(statePath.string()) << ",\"groups\":[";
    for (size_t i = 0; i < model.groups.size(); ++i) {
        if (i) out << ',';
        const auto& group = model.groups[i];
        out << "{\"id\":" << group.id << ",\"name\":" << json(group.name) << ",\"selected\":" << group.selected << ",\"desktops\":[";
        for (size_t j = 0; j < group.desktops.size(); ++j) { if (j) out << ','; out << group.desktops[j]; }
        out << "]}";
    }
    out << "],\"hits\":[";
    for (size_t i = 0; i < hits.size(); ++i) {
        if (i) out << ',';
        const auto& hit = hits[i];
        out << "{\"kind\":" << int(hit.kind) << ",\"group\":" << hit.group << ",\"desktop\":" << hit.desktop << ",\"x\":" << hit.box.x << ",\"y\":" << hit.box.y << ",\"w\":" << hit.box.w << ",\"h\":" << hit.box.h;
        if (const auto window = hit.window.lock()) {
            std::ostringstream address;
            address << "0x" << std::hex << reinterpret_cast<uintptr_t>(window.get());
            out << ",\"window\":" << json(address.str()) << ",\"title\":" << json(window->m_title);
        }
        out << '}';
    }
    return out.str() + "]}";
}
