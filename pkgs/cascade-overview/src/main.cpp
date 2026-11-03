// Runtime integration derives from Hyprspace by KZdkm (GPL-2.0).
#include "Globals.hpp"
#include "Overview.hpp"

#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/managers/SessionLockManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/state/MonitorState.hpp>
#include <hyprland/src/pointer/cursor/CursorShapeOverrideController.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <unordered_set>
#include <xkbcommon/xkbcommon.h>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace {
    using Widget = std::shared_ptr<CCascadeOverview>;
    std::vector<Widget> widgets;
    std::vector<CHyprSignalListener> listeners;
    std::weak_ptr<CCascadeOverview> pointerWidget, swipeWidget;
    std::unordered_set<uint32_t> swallowedButtons, swallowedKeys;
    SP<SHyprCtlCommand> inspectCommand;
    UP<SEventLoopDoLaterLock> emptyRefresh;
    bool refreshingEmpties = false;
    bool wasLocked = false;
    size_t cursorOwners = 0;

    bool locked() {
        const bool state = g_pSessionLockManager && g_pSessionLockManager->isSessionLocked();
        if (state && !wasLocked) {
            wasLocked = true;
            if (const auto widget = swipeWidget.lock()) {
                IPointer::SSwipeEndEvent cancelled;
                cancelled.cancelled = true;
                widget->endSwipe(cancelled);
            }
            swipeWidget.reset();
            pointerWidget.reset();
            swallowedButtons.clear();
            swallowedKeys.clear();
            for (const auto& widget : widgets)
                if (widget->isActive())
                    widget->hide(true);
        }
        wasLocked = state;
        return state;
    }

    Widget widgetFor(PHLMONITOR monitor) {
        if (!monitor)
            return nullptr;
        const auto found = std::ranges::find_if(widgets, [&](const auto& widget) { return widget->getOwner() == monitor; });
        return found == widgets.end() ? nullptr : *found;
    }

    Widget currentWidget() {
        if (!g_pInputManager || !State::monitorState())
            return nullptr;
        return widgetFor(State::monitorState()->query().vec(g_pInputManager->getMouseCoordsInternal()).run());
    }

    Widget activeWidget() {
        if (auto widget = pointerWidget.lock(); widget && widget->isActive())
            return widget;
        if (auto widget = currentWidget(); widget && widget->isActive())
            return widget;
        const auto found = std::ranges::find_if(widgets, [](const auto& widget) { return widget->isActive(); });
        return found == widgets.end() ? nullptr : *found;
    }

    void registerMonitors() {
        if (!State::monitorState())
            return;
        const auto& monitors = State::monitorState()->monitors();
        std::erase_if(widgets, [&](const auto& widget) {
            const auto owner = widget->getOwner();
            return !owner || std::ranges::find(monitors, owner) == monitors.end();
        });
        for (const auto& monitor : monitors)
            if (!widgetFor(monitor))
                widgets.push_back(std::make_shared<CCascadeOverview>(monitor->m_id));
    }

    SDispatchResult unavailable() {
        return {.success = false, .error = locked() ? "Cascade is unavailable while the session is locked" : "No monitor is available"};
    }

    enum class Visibility { Toggle, Open, Close };

    SDispatchResult visibility(Visibility action, const std::string& argument) {
        if (locked())
            return unavailable();
        const auto current = currentWidget();
        if (!current)
            return unavailable();
        const bool open = action == Visibility::Open || (action == Visibility::Toggle && !current->isActive());
        const auto apply = [&](const Widget& widget) { open ? widget->show() : widget->hide(); };
        if (argument == "all") {
            for (const auto& widget : widgets)
                apply(widget);
        } else
            apply(current);
        return {};
    }

    SDispatchResult toggle(std::string argument) { return visibility(Visibility::Toggle, argument); }
    SDispatchResult open(std::string argument) { return visibility(Visibility::Open, argument); }
    SDispatchResult close(std::string argument) { return visibility(Visibility::Close, argument); }

    struct Direction { int x, y; bool move; };
    constexpr std::optional<Direction> direction(std::string_view argument) {
        const bool move = argument.starts_with("move-");
        if (move)
            argument.remove_prefix(5);
        if (argument == "left") return Direction{-1, 0, move};
        if (argument == "right") return Direction{1, 0, move};
        if (argument == "up") return Direction{0, -1, move};
        if (argument == "down") return Direction{0, 1, move};
        return std::nullopt;
    }
    static_assert(direction("left")->x == -1 && direction("right")->x == 1 && direction("up")->y == -1 && direction("down")->y == 1);
    static_assert(direction("move-left")->move && direction("move-right")->x == 1 && direction("move-up")->y == -1 && direction("move-down")->y == 1);
    static_assert(!direction("invalid") && !direction("move-move-left"));

    SDispatchResult navigate(std::string argument) {
        if (locked())
            return unavailable();
        const auto parsed = direction(argument);
        if (!parsed)
            return {.success = false, .error = "Expected left, right, up, down or move-left/right/up/down"};
        auto widget = activeWidget();
        if (!widget)
            widget = currentWidget();
        if (!widget)
            return unavailable();
        widget->navigate(parsed->x, parsed->y, parsed->move);
        return {};
    }

    SDispatchResult command(std::string argument) {
        if (locked())
            return unavailable();
        auto widget = activeWidget();
        if (!widget)
            widget = currentWidget();
        if (!widget)
            return unavailable();
        std::string error;
        if (!widget->command(argument, error))
            return {.success = false, .error = error};
        return {};
    }

    template <SDispatchResult (*Dispatcher)(std::string)>
    int luaDispatch(lua_State* state) {
        const auto result = Dispatcher(luaL_optstring(state, 1, ""));
        lua_pushboolean(state, result.success);
        if (!result.success) {
            lua_pushstring(state, result.error.c_str());
            return 2;
        }
        return 1;
    }

    void onButton(const IPointer::SButtonEvent& event, SCallbackInfo& info) {
        if (locked())
            return;
        const bool pressed = event.state == WL_POINTER_BUTTON_STATE_PRESSED;
        const bool swallowed = !pressed && swallowedButtons.erase(event.button) != 0;
        const auto widget = activeWidget();
        if (!widget && !swallowed)
            return;
        info.cancelled = true;
        if (pressed)
            swallowedButtons.insert(event.button);
        if (widget) {
            pointerWidget = widget;
            widget->buttonEvent(pressed, g_pInputManager->getMouseCoordsInternal(), event.button);
        }
        if (swallowedButtons.empty())
            pointerWidget.reset();
    }

    void onAxis(const IPointer::SAxisEvent& event, SCallbackInfo& info) {
        if (locked())
            return;
        if (const auto widget = activeWidget()) {
            info.cancelled = true;
            widget->axisEvent(event.delta, event.axis, g_pInputManager->getMouseCoordsInternal());
        }
    }

    void onMotion(const Vector2D& coordinates, SCallbackInfo& info) {
        if (locked())
            return;
        if (const auto widget = activeWidget()) {
            widget->motionEvent(coordinates);
            info.cancelled = true;
        }
    }

    void onKey(const IKeyboard::SKeyEvent& event, SCallbackInfo& info) {
        if (locked())
            return;
        const bool pressed = event.state == WL_KEYBOARD_KEY_STATE_PRESSED;
        const bool swallowed = !pressed && swallowedKeys.erase(event.keycode) != 0;
        const auto widget = activeWidget();
        if (!widget && !swallowed)
            return;
        info.cancelled = true;
        if (!pressed)
            return;
        swallowedKeys.insert(event.keycode);
        const auto keyboard = g_pSeatManager->m_keyboard.lock();
        if (!widget || !keyboard || !keyboard->m_xkbState)
            return;
        const auto keycode = event.keycode + 8;
        auto* state = keyboard->m_xkbState;
        const auto modifier = [&](const char* name) { return xkb_state_mod_name_is_active(state, name, XKB_STATE_MODS_EFFECTIVE) > 0; };
        auto* symbols = keyboard->m_xkbSymState ? keyboard->m_xkbSymState : state;
        widget->keyEvent(xkb_state_key_get_one_sym(symbols, keycode),
                         modifier(XKB_MOD_NAME_SHIFT), modifier(XKB_MOD_NAME_CTRL), modifier(XKB_MOD_NAME_ALT), modifier(XKB_MOD_NAME_LOGO));
    }

    void onSwipeBegin(const IPointer::SSwipeBeginEvent& event, SCallbackInfo& info) {
        if (locked() || !config.gestures->value() || (event.fingers != 3 && event.fingers != 4))
            return;
        if (const auto previous = swipeWidget.lock()) {
            IPointer::SSwipeEndEvent cancelled;
            cancelled.cancelled = true;
            previous->endSwipe(cancelled);
        }
        swipeWidget.reset();
        if (const auto widget = currentWidget(); widget && widget->beginSwipe(event)) {
            swipeWidget = widget;
            info.cancelled = true;
        }
    }

    void onSwipeUpdate(const IPointer::SSwipeUpdateEvent& event, SCallbackInfo& info) {
        if (locked())
            return;
        if (const auto widget = swipeWidget.lock()) {
            info.cancelled = true;
            widget->updateSwipe(event);
        }
    }

    void onSwipeEnd(const IPointer::SSwipeEndEvent& event, SCallbackInfo& info) {
        if (locked())
            return;
        if (const auto widget = swipeWidget.lock()) {
            info.cancelled = true;
            widget->endSwipe(event);
        }
        swipeWidget.reset();
    }

    void onRender(eRenderStage stage) {
        if (locked() || stage != RENDER_LAST_MOMENT)
            return;
        const auto widget = widgetFor(g_pHyprRenderer->m_renderData.pMonitor.lock());
        if (!widget)
            return;
        if (widget->isActive())
            widget->motionEvent(g_pInputManager->getMouseCoordsInternal());
        widget->draw();
    }

    void syncWidgets() {
        registerMonitors();
        for (const auto& widget : widgets)
            widget->sync();
    }

    void cleanup() {
        listeners.clear();
        emptyRefresh.reset();
        refreshingEmpties = false;
        pointerWidget.reset();
        swipeWidget.reset();
        swallowedButtons.clear();
        swallowedKeys.clear();
        wasLocked = false;
        widgets.clear();
        if (inspectCommand) {
            HyprlandAPI::unregisterHyprCtlCommand(pHandle, inspectCommand);
            inspectCommand.reset();
        }
    }
}

void cascadeRefreshEmptyDesktops() {
    if (emptyRefresh) return;
    // A close event precedes unmap; defer focus and cleanup until the transaction finishes.
    emptyRefresh = g_pEventLoopManager->doLaterLock([] {
        refreshingEmpties = true;
        Hyprutils::Utils::CScopeGuard done([] {
            refreshingEmpties = false;
            emptyRefresh.reset();
        });
        for (const auto& widget : widgets) widget->refreshEmptyDesktop();
    });
}

void cascadeCursorOverride(bool acquire) {
    using namespace Pointer::Cursor;
    if (acquire) {
        if (cursorOwners++ == 0) overrideController->setOverride("left_ptr", CURSOR_OVERRIDE_SPECIAL_ACTION);
    } else if (cursorOwners && --cursorOwners == 0)
        overrideController->unsetOverride(CURSOR_OVERRIDE_SPECIAL_ACTION);
}

std::vector<Cascade::Id> cascadeReservedDesktops() {
    std::vector<Cascade::Id> reserved;
    for (const auto& widget : widgets) {
        const auto desktops = widget->reservedDesktops();
        reserved.insert(reserved.end(), desktops.begin(), desktops.end());
    }
    return reserved;
}

APICALL EXPORT std::string PLUGIN_API_VERSION() { return HYPRLAND_API_VERSION; }

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    pHandle = handle;
    if (std::string_view(__hyprland_api_get_hash()) != __hyprland_api_get_client_hash())
        throw std::runtime_error("Cascade was compiled for a different Hyprland version; rebuild Cascade with the running Hyprland");

    try {
        for (const auto& value : {config.animate, config.gestures, config.reverseSwipe, config.cardWidth, config.rowHeight})
            if (!HyprlandAPI::addConfigValueV2(pHandle, value))
                throw std::runtime_error("Could not register Cascade configuration");

        for (const auto& [name, dispatcher] : std::initializer_list<std::pair<const char*, SDispatchResult (*)(std::string)>>{
                 {"cascade:toggle", toggle}, {"cascade:open", open}, {"cascade:close", close}, {"cascade:navigate", navigate}, {"cascade:command", command}})
            if (!HyprlandAPI::addDispatcherV2(pHandle, name, dispatcher))
                throw std::runtime_error(std::string("Could not register ") + name);

        for (const auto& [name, dispatcher] : std::initializer_list<std::pair<const char*, SDispatchResult (*)(std::string)>>{
                 {"overview:toggle", toggle}, {"overview:open", open}, {"overview:close", close}})
            if (!g_pKeybindManager->m_dispatchers.contains(name))
                HyprlandAPI::addDispatcherV2(pHandle, name, dispatcher);

        if (Config::mgr() && Config::mgr()->type() == Config::CONFIG_LUA) {
            for (const auto& [name, callback] : std::initializer_list<std::pair<const char*, PLUGIN_LUA_FN>>{
                     {"toggle", luaDispatch<toggle>}, {"open", luaDispatch<open>}, {"close", luaDispatch<close>},
                     {"navigate", luaDispatch<navigate>}, {"command", luaDispatch<command>}})
                if (!HyprlandAPI::addLuaFunction(pHandle, "cascade", name, callback))
                    throw std::runtime_error(std::string("Could not register hl.plugin.cascade.") + name);
        }

        inspectCommand = HyprlandAPI::registerHyprCtlCommand(pHandle, {"cascade", true, [](eHyprCtlOutputFormat, std::string) {
            const auto widget = currentWidget();
            return widget ? widget->inspect() : std::string("{}");
        }});
        if (!inspectCommand)
            throw std::runtime_error("Could not register the Cascade hyprctl command");

        const auto& bus = Event::bus();
        listeners.push_back(bus->m_events.render.preChecks.listen([](PHLMONITOR monitor) {
            if (const auto widget = widgetFor(monitor)) widget->beforeRender();
        }));
        listeners.push_back(bus->m_events.render.stage.listen(onRender));
        listeners.push_back(bus->m_events.start.listen(syncWidgets));
        listeners.push_back(bus->m_events.config.reloaded.listen(syncWidgets));
        listeners.push_back(bus->m_events.monitor.added.listen([](PHLMONITOR) { syncWidgets(); }));
        listeners.push_back(bus->m_events.monitor.preRemoved.listen([](PHLMONITOR monitor) {
            // The owner is no longer queryable when monitor.removed destroys its widget.
            if (const auto widget = widgetFor(monitor)) widget->hide(true);
        }));
        listeners.push_back(bus->m_events.monitor.removed.listen([](PHLMONITOR) { syncWidgets(); }));
        listeners.push_back(bus->m_events.monitor.layoutChanged.listen(syncWidgets));
        listeners.push_back(bus->m_events.workspace.active.listen([](PHLWORKSPACE workspace) {
            if (workspace && !refreshingEmpties) syncWidgets();
        }));
        listeners.push_back(bus->m_events.window.openLate.listen([](PHLWINDOW) { cascadeRefreshEmptyDesktops(); }));
        listeners.push_back(bus->m_events.window.moveToWorkspace.listen([](PHLWINDOW, PHLWORKSPACE) { cascadeRefreshEmptyDesktops(); }));
        listeners.push_back(bus->m_events.window.close.listen([](PHLWINDOW) { cascadeRefreshEmptyDesktops(); }));
        listeners.push_back(listenCancellable<IPointer::SButtonEvent>(bus->m_events.input.mouse.button, onButton));
        listeners.push_back(listenCancellable<IPointer::SAxisEvent>(bus->m_events.input.mouse.axis, onAxis));
        listeners.push_back(listenCancellable<Vector2D>(bus->m_events.input.mouse.move, onMotion));
        listeners.push_back(listenCancellable<IKeyboard::SKeyEvent>(bus->m_events.input.keyboard.key, onKey));
        listeners.push_back(listenCancellable<IPointer::SSwipeBeginEvent>(bus->m_events.gesture.swipe.begin, onSwipeBegin));
        listeners.push_back(listenCancellable<IPointer::SSwipeUpdateEvent>(bus->m_events.gesture.swipe.update, onSwipeUpdate));
        listeners.push_back(listenCancellable<IPointer::SSwipeEndEvent>(bus->m_events.gesture.swipe.end, onSwipeEnd));
        syncWidgets();
        HyprlandAPI::reloadConfig();
    } catch (...) {
        cleanup();
        throw;
    }

    return {"Cascade Overview", "Persistent workspace groups with live desktop previews", "KZdkm (Hyprspace), Cascade contributors", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    cleanup();
    pHandle = nullptr;
}
