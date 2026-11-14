#include "Overview.hpp"
#include "Globals.hpp"
#include "Layout.hpp"
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/state/WindowState.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/state/MonitorState.hpp>
#include <hyprland/src/state/WorkspaceState.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <linux/input-event-codes.h>

using Cascade::Id;

const CCascadeOverview::Hit* CCascadeOverview::hitAt(Vector2D position, bool windows) const {
    const auto owner = State::monitorState()->query().id(ownerID).run();
    if (!owner || position.y < owner->m_position.y + 88 || position.y > owner->m_position.y + owner->m_transformedSize.y / owner->m_scale - 44) return nullptr;
    if (position.x < owner->m_position.x || position.x >= owner->m_position.x + owner->m_transformedSize.x / owner->m_scale) return nullptr;
    for (auto it = hits.rbegin(); it != hits.rend(); ++it)
        if ((windows || it->kind != Kind::Window) && it->box.containsPoint(position)) return &*it;
    return nullptr;
}

void CCascadeOverview::activate(const Hit& hit) {
    std::string error;
    switch (hit.kind) {
        case Kind::Window:
            if (const auto window = hit.window.lock()) {
                focus(hit.desktop, true);
                Desktop::focusState()->fullWindowFocus(window, Desktop::FOCUS_REASON_CLICK);
            }
            break;
        case Kind::Desktop: focus(hit.desktop, true); break;
        case Kind::NewGroup: command("group-new", error); break;
        case Kind::GroupUp: case Kind::GroupDown: case Kind::RemoveGroup:
            if (auto group = model.group(hit.group)) {
                model.select(group->selected);
                command(hit.kind == Kind::GroupUp ? "group-up" : hit.kind == Kind::GroupDown ? "group-down" : "group-delete", error);
            }
            break;
    }
    if (!error.empty()) notify(error);
}

void CCascadeOverview::drop(const Hit& from, const Hit& to) {
    Id targetGroup = to.group, targetDesktop = to.desktop;
    if (to.kind == Kind::NewGroup) {
        targetGroup = model.createGroup("Grupo " + std::to_string(model.groups.size() + 1), occupied());
        targetDesktop = model.current().selected;
    }
    auto group = model.group(targetGroup);
    if (!group) return;
    if (targetDesktop <= 0) targetDesktop = group->selected;
    if (from.kind == Kind::Window) {
        if (moveWindow(from.window.lock(), targetDesktop)) model.select(targetDesktop);
    } else if (from.kind == Kind::Desktop) {
        const auto source = model.group(model.groupFor(from.desktop));
        if (!source) return;
        if (source->id != targetGroup && source->desktops.size() == 1) model.createDesktop(source->id, occupied());
        group = model.group(targetGroup);
        auto pos = std::find(group->desktops.begin(), group->desktops.end(), targetDesktop);
        model.moveDesktop(from.desktop, targetGroup, pos == group->desktops.end() ? group->desktops.size() : pos - group->desktops.begin());
        model.select(from.desktop);
    }
    targets();
    save();
}

CBox CCascadeOverview::dragBox(const Hit& source, Vector2D coordinates) {
    const auto owner = getOwner();
    if (!owner) return {};
    const double width = owner->m_transformedSize.x / owner->m_scale, height = owner->m_transformedSize.y / owner->m_scale;
    const auto window = source.window.lock();
    const bool desktop = source.kind == Kind::Desktop;
    const auto size = window ? window->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT) : Vector2D{width, height};
    const auto fitted = Cascade::fit(0, 0, std::min(240., width - 32), std::min(160., height - 64), size.x, size.y);
    const double w = fitted.w + (desktop ? 0 : 12), h = fitted.h + (desktop ? 0 : 32);
    return {std::clamp(coordinates.x - owner->m_position.x + 18, 8., std::max(8., width - w - 8)),
            std::clamp(coordinates.y - owner->m_position.y + 20, 8., std::max(8., height - h - 8)), w, h};
}

bool CCascadeOverview::buttonEvent(bool pressed, Vector2D coordinates, uint32_t button) {
    pointer = coordinates;
    if (button != BTN_LEFT) return true;
    if (pressed) {
        const auto hit = hitAt(coordinates);
        pressedHit = hit ? std::optional<Hit>(*hit) : std::nullopt;
        pressPosition = coordinates;
        dragging = false;
        dragPreview.reset();
    } else {
        const auto from = pressedHit;
        const bool wasDragging = dragging;
        if (dragPreview) {
            dragPreview->box = dragBox(dragPreview->source, coordinates);
            dragPreview->settling = true;
        }
        pressedHit.reset();
        dragging = false;
        if (from) {
            if (const auto to = hitAt(coordinates, !wasDragging)) {
                const auto target = *to;
                if (wasDragging) drop(*from, target);
                else if (from->kind == target.kind && from->group == target.group && from->desktop == target.desktop && from->window.lock() == target.window.lock()) activate(target);
            }
        }
    }
    targets();
    return true;
}

void CCascadeOverview::motionEvent(Vector2D coordinates) {
    const bool moved = pointer != coordinates;
    pointer = coordinates;
    if (active && moved) {
        if (const auto owner = getOwner()) { g_pHyprRenderer->damageMonitor(owner); owner->scheduleFrame(); }
    }
    if (!active || !pressedHit) return;
    const auto kind = pressedHit->kind;
    if (!dragging && (kind == Kind::Window || kind == Kind::Desktop) && (coordinates - pressPosition).size() > 7) {
        dragging = true;
        dragPreview = DragPreview{.source = *pressedHit, .box = dragBox(*pressedHit, coordinates)};
    }
    if (dragging) {
        if (dragPreview) dragPreview->box = dragBox(dragPreview->source, coordinates);
        const auto owner = getOwner();
        if (!owner) return;
        const auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration<double>(now - lastWheel).count() > .35) {
            const double localX = coordinates.x - owner->m_position.x, localY = coordinates.y - owner->m_position.y;
            const double width = owner->m_transformedSize.x / owner->m_scale, height = owner->m_transformedSize.y / owner->m_scale;
            if (localX < 0 || localX >= width || localY < 0 || localY >= height) return;
            if (localY < 120 || localY > height - 72) { navigate(0, localY < 120 ? -1 : 1); lastWheel = now; }
            else if (localX < 48 || localX > width - 48) { navigate(localX < 48 ? -1 : 1, 0); lastWheel = now; }
        }
    }
}

bool CCascadeOverview::axisEvent(double delta, wl_pointer_axis axis, Vector2D) {
    auto& accumulated = axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL ? wheelX : wheelY;
    accumulated += delta;
    if (std::abs(accumulated) >= 12) {
        const int direction = accumulated > 0 ? 1 : -1;
        navigate(axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL ? direction : 0, axis == WL_POINTER_AXIS_VERTICAL_SCROLL ? direction : 0);
        accumulated = 0;
    }
    return true;
}

bool CCascadeOverview::keyEvent(uint32_t key, bool shift, bool control, bool alt, bool super) {
    if (key == XKB_KEY_Escape || (key == XKB_KEY_Tab && super)) { hide(); return true; }
    if (key == XKB_KEY_Return || key == XKB_KEY_KP_Enter) {
        const auto window = keyboardWindow.lock();
        const auto desktop = model.current().selected;
        focus(desktop, true);
        if (window && window->m_workspace && window->m_workspace->m_id == desktop)
            Desktop::focusState()->fullWindowFocus(window, Desktop::FOCUS_REASON_KEYBIND);
        return true;
    }
    std::string action, error;
    if (key == XKB_KEY_Left || key == XKB_KEY_Right || key == XKB_KEY_Up || key == XKB_KEY_Down) {
        const int dx = key == XKB_KEY_Left ? -1 : key == XKB_KEY_Right ? 1 : 0;
        const int dy = key == XKB_KEY_Up ? -1 : key == XKB_KEY_Down ? 1 : 0;
        if (alt) action = dy ? (dy < 0 ? "group-up" : "group-down") : (dx < 0 ? "desktop-left" : "desktop-right");
        else navigate(dx, dy, shift);
    } else if (key == XKB_KEY_n || key == XKB_KEY_N) action = control ? "group-new" : "desktop-new";
    else if (key == XKB_KEY_Delete) action = control ? "group-delete" : "desktop-delete";
    else if (key == XKB_KEY_Tab || key == XKB_KEY_ISO_Left_Tab) {
        std::vector<PHLWINDOW> windows;
        for (const auto& window : Desktop::windowState()->windows())
            if (window && window->m_isMapped && window->m_workspace && window->m_workspace->m_id == model.current().selected) windows.push_back(window);
        if (!windows.empty()) {
            const auto found = std::find(windows.begin(), windows.end(), keyboardWindow.lock());
            const int index = found == windows.end() ? (shift ? 0 : -1) : found - windows.begin();
            keyboardWindow = windows[(index + (shift ? -1 : 1) + windows.size()) % windows.size()];
        }
    }
    if (!action.empty() && !command(action, error)) notify(error);
    targets();
    return true;
}

bool CCascadeOverview::beginSwipe(const IPointer::SSwipeBeginEvent& event) {
    if (event.fingers != 3 && event.fingers != 4) return false;
    cancelDesktopSwipe();
    swiping = true;
    swipeFingers = event.fingers;
    swipeAxis = 0;
    swipeDelta = {0, 0};
    if (!active) {
        sync();
        if (const auto owner = getOwner(); owner && event.fingers == 3 && !owner->m_activeSpecialWorkspace)
            desktopSwipeSource = owner->m_activeWorkspace;
    }
    return true;
}

void CCascadeOverview::updateDesktopSwipe() {
    desktopSwipePending = false;
    const auto owner = getOwner();
    if (!owner || owner->m_activeWorkspace != desktopSwipeSource) { cancelDesktopSwipe(); return; }
    const auto group = model.group(model.groupFor(desktopSwipeSource->m_id));
    if (!group) { cancelDesktopSwipe(); return; }
    const auto position = std::find(group->desktops.begin(), group->desktops.end(), desktopSwipeSource->m_id) - group->desktops.begin();
    const int direction = desktopSwipeDelta < 0 ? 1 : -1;
    PHLWORKSPACE target;
    if (group->desktops.size() > 1) {
        const auto count = static_cast<int64_t>(group->desktops.size());
        const auto index = (static_cast<int64_t>(position) + direction + count) % count;
        const auto id = group->desktops[index];
        target = State::workspaceState()->query().id(id).run();
        if (!target) target = State::workspaceState()->create(id, ownerID);
        if (target && target->m_monitor != owner) target.reset();
    }
    if (desktopSwipeTarget && desktopSwipeTarget != target) {
        desktopSwipeTarget->m_forceRendering = false;
        desktopSwipeTarget->m_renderOffset->setValueAndWarp(Vector2D{});
        desktopSwipeTarget->m_alpha->setValueAndWarp(0.F);
        desktopSwipeTarget->updateWindowDecos();
    }
    desktopSwipeTarget = target;
    desktopSwipeDirection = direction;
    if (!target) { desktopSwipeDelta = 0; swipeDelta.x = 0; }
    static const auto distance = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_distance");
    static const auto gap = CConfigValue<Config::INTEGER>("general:gaps_workspaces");
    const double span = owner->m_size.x + *gap;
    const double offset = target ? std::clamp(desktopSwipeDelta / std::max<int64_t>(1, *distance), -1., 1.) * span : 0.;
    // Native workspace offsets also disable solitary/direct scanout while a fullscreen client moves.
    desktopSwipeSource->m_forceRendering = bool(target);
    desktopSwipeSource->m_alpha->setValueAndWarp(1.F);
    desktopSwipeSource->m_renderOffset->setValueAndWarp(Vector2D{offset, 0.0});
    desktopSwipeSource->updateWindowDecos();
    if (target) {
        target->m_forceRendering = true;
        target->m_alpha->setValueAndWarp(1.F);
        target->m_renderOffset->setValueAndWarp(Vector2D{offset + direction * span, 0.0});
        target->updateWindowDecos();
    }
    g_pHyprRenderer->damageMonitor(owner);
    owner->scheduleFrame();
}

void CCascadeOverview::finishDesktopSwipe(bool commit, bool immediate) {
    auto source = std::move(desktopSwipeSource), target = std::move(desktopSwipeTarget);
    const auto direction = desktopSwipeDirection;
    desktopSwipeDirection = 0;
    desktopSwipeDelta = 0;
    desktopSwipePending = false;
    if (!source) return;
    const auto owner = getOwner();
    const auto sourceOffset = source->m_renderOffset->value();
    const auto targetOffset = target ? target->m_renderOffset->value() : Vector2D{};
    if (!owner || owner->m_activeWorkspace != source) { commit = false; immediate = true; }
    if (commit && target) { focus(target->m_id); commit = owner->m_activeWorkspace == target; }
    else commit = false;
    static const auto gap = CConfigValue<Config::INTEGER>("general:gaps_workspaces");
    const double span = owner ? owner->m_size.x + *gap : 0.;
    for (const auto& workspace : {source, target}) {
        if (!workspace) continue;
        workspace->m_forceRendering = false;
        if (immediate) {
            workspace->m_renderOffset->setValueAndWarp(Vector2D{});
            workspace->m_alpha->setValueAndWarp(workspace->isVisible() ? 1.F : 0.F);
        } else {
            const Vector2D goal = workspace == source ? (commit ? Vector2D{-direction * span, 0.0} : Vector2D{}) : (commit ? Vector2D{} : Vector2D{direction * span, 0.0});
            workspace->m_alpha->setValueAndWarp(1.F);
            workspace->m_renderOffset->setValueAndWarp(workspace == source ? sourceOffset : targetOffset);
            if (config.animate->value()) *workspace->m_renderOffset = goal;
            else workspace->m_renderOffset->setValueAndWarp(goal);
        }
        workspace->updateWindowDecos();
    }
    if (owner) { g_pHyprRenderer->damageMonitor(owner); owner->scheduleFrame(); }
}

void CCascadeOverview::cancelDesktopSwipe() {
    finishDesktopSwipe(false, true);
    swiping = false;
}

bool CCascadeOverview::updateSwipe(const IPointer::SSwipeUpdateEvent& event) {
    if (!swiping) return false;
    const auto delta = event.delta * (config.reverseSwipe->value() ? -1 : 1);
    swipeDelta += delta;
    const auto previousAxis = swipeAxis;
    if (!swipeAxis) swipeAxis = Cascade::swipeAxis(swipeDelta.x, swipeDelta.y);
    static const auto distance = CConfigValue<Config::INTEGER>("gestures:workspace_swipe_distance");
    const double limit = active ? 285. : std::max<int64_t>(1, *distance);
    const double accumulatedX = swipeDelta.x;
    swipeDelta.x = std::clamp(swipeDelta.x, -limit, limit);
    if (!active && desktopSwipeSource && swipeAxis == 1) {
        if (previousAxis != 1) {
            static const auto gap = CConfigValue<Config::INTEGER>("general:gaps_workspaces");
            const auto owner = getOwner();
            if (!owner) { cancelDesktopSwipe(); return true; }
            // Anchor when the direction locks: an earlier transition may still be moving.
            desktopSwipeDelta = desktopSwipeSource->m_renderOffset->value().x / std::max(1., owner->m_size.x + *gap) * limit;
            desktopSwipeDelta += accumulatedX;
        } else desktopSwipeDelta += delta.x;
        while (std::abs(desktopSwipeDelta) >= limit) {
            const double remainder = desktopSwipeDelta + (desktopSwipeDelta < 0 ? limit : -limit);
            updateDesktopSwipe();
            const auto target = desktopSwipeTarget;
            if (!target) return true;
            // Commit a completed desktop in input handling, then keep the unconsumed movement.
            finishDesktopSwipe(true, true);
            const auto owner = getOwner();
            if (!owner || owner->m_activeWorkspace != target) return true;
            desktopSwipeSource = target;
            swiping = true;
            desktopSwipeDelta = remainder;
            swipeDelta.x = remainder;
        }
        desktopSwipePending = true;
        if (!desktopSwipeTarget || (desktopSwipeDelta < 0 ? 1 : -1) != desktopSwipeDirection) updateDesktopSwipe();
        else if (const auto owner = getOwner()) { g_pHyprRenderer->damageMonitor(owner); owner->scheduleFrame(); }
    }
    if (active && swipeFingers == 3) {
        if (swipeAxis == 1) {
            columns[model.activeGroup].value = columns[model.activeGroup].target - swipeDelta.x / 300;
            columns[model.activeGroup].velocity = 0;
        }
        if (swipeAxis == 2) {
            row.value = row.target - std::clamp(swipeDelta.y / 240, -.95, .95);
            row.velocity = 0;
        }
        if (const auto owner = getOwner()) { g_pHyprRenderer->damageMonitor(owner); owner->scheduleFrame(); }
    }
    return true;
}

bool CCascadeOverview::endSwipe(const IPointer::SSwipeEndEvent& event) {
    if (!swiping) return false;
    swiping = false;
    if (desktopSwipeSource) {
        const bool horizontal = swipeFingers == 3 && swipeAxis == 1;
        if (horizontal && !event.cancelled && desktopSwipePending) updateDesktopSwipe();
        const bool commit = horizontal && !event.cancelled && std::abs(desktopSwipeDelta) >= 45 && desktopSwipeDelta * swipeDelta.x > 0;
        finishDesktopSwipe(commit, !horizontal);
        if (horizontal) { targets(); return true; }
    }
    if (!event.cancelled) {
        if (swipeFingers == 4 && swipeAxis == 2 && std::abs(swipeDelta.y) >= 35) active ? hide() : show();
        else if (swipeFingers == 3) {
            if (swipeAxis == 1 && std::abs(swipeDelta.x) >= 45) navigate(swipeDelta.x < 0 ? 1 : -1, 0);
            if (swipeAxis == 2 && std::abs(swipeDelta.y) >= 45) navigate(0, swipeDelta.y < 0 ? 1 : -1);
        }
    }
    targets();
    return true;
}
