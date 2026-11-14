// Test fixture: emit actual compositor gesture events, only in an isolated QA session.
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/state/WorkspaceState.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <cstdlib>
#include <stdexcept>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

APICALL EXPORT std::string PLUGIN_API_VERSION() { return HYPRLAND_API_VERSION; }

static uint64_t renderedFrames = 0;
static CHyprSignalListener frameListener;
static int frameCount(lua_State* state) {
    lua_pushinteger(state, renderedFrames);
    return 1;
}

static int swipe(lua_State* state) {
    const auto fingers = static_cast<uint32_t>(luaL_checkinteger(state, 1));
    const double x = luaL_checknumber(state, 2), y = luaL_checknumber(state, 3);
    const bool cancelled = lua_toboolean(state, 4);
    Event::SCallbackInfo begin;
    Event::bus()->m_events.gesture.swipe.begin.emit(IPointer::SSwipeBeginEvent{.fingers = fingers}, begin);
    for (int frame = 0; frame < 8; ++frame) {
        Event::SCallbackInfo update;
        Event::bus()->m_events.gesture.swipe.update.emit(IPointer::SSwipeUpdateEvent{.fingers = fingers, .delta = {x / 8, y / 8}}, update);
    }
    Event::SCallbackInfo end;
    Event::bus()->m_events.gesture.swipe.end.emit(IPointer::SSwipeEndEvent{.cancelled = cancelled}, end);
    lua_pushboolean(state, begin.cancelled);
    return 1;
}

static int hideCursor(lua_State*) {
    g_pInputManager->processMouseRequest(CSeatManager::SSetCursorEvent{});
    return 0;
}

static int cursorVisible(lua_State* state) {
    lua_pushboolean(state, g_pHyprRenderer->shouldRenderCursor());
    return 1;
}

static int swipeBegin(lua_State* state) {
    Event::SCallbackInfo info;
    Event::bus()->m_events.gesture.swipe.begin.emit(IPointer::SSwipeBeginEvent{.fingers = static_cast<uint32_t>(luaL_checkinteger(state, 1))}, info);
    lua_pushboolean(state, info.cancelled);
    return 1;
}

static int swipeUpdate(lua_State* state) {
    Event::SCallbackInfo info;
    Event::bus()->m_events.gesture.swipe.update.emit(IPointer::SSwipeUpdateEvent{
        .fingers = static_cast<uint32_t>(luaL_checkinteger(state, 1)), .delta = {luaL_checknumber(state, 2), luaL_checknumber(state, 3)}}, info);
    return 0;
}

static int swipeEnd(lua_State* state) {
    Event::SCallbackInfo info;
    Event::bus()->m_events.gesture.swipe.end.emit(IPointer::SSwipeEndEvent{.cancelled = static_cast<bool>(lua_toboolean(state, 1))}, info);
    return 0;
}

static int workspace(lua_State* state) {
    const auto ws = State::workspaceState()->query().id(luaL_checkinteger(state, 1)).run();
    if (!ws) { lua_pushnil(state); return 1; }
    lua_newtable(state);
    for (const auto& [name, value] : {std::pair{"x", ws->m_renderOffset->value().x}, {"y", ws->m_renderOffset->value().y},
            {"goalX", ws->m_renderOffset->goal().x}, {"goalY", ws->m_renderOffset->goal().y}, {"alpha", double(ws->m_alpha->value())}}) {
        lua_pushnumber(state, value);
        lua_setfield(state, -2, name);
    }
    lua_pushboolean(state, ws->m_forceRendering);
    lua_setfield(state, -2, "forced");
    lua_pushstring(state, ws->m_renderOffset->getBezierName().c_str());
    lua_setfield(state, -2, "bezier");
    return 1;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    const auto runtime = std::getenv("XDG_RUNTIME_DIR");
    if (!runtime || !std::string(runtime).starts_with("/tmp/cascade-"))
        throw std::runtime_error("Gesture test driver only runs in an isolated Cascade QA session");
    HyprlandAPI::addLuaFunction(handle, "cascade_test", "swipe", swipe);
    HyprlandAPI::addLuaFunction(handle, "cascade_test", "hide_cursor", hideCursor);
    HyprlandAPI::addLuaFunction(handle, "cascade_test", "cursor_visible", cursorVisible);
    HyprlandAPI::addLuaFunction(handle, "cascade_test", "begin", swipeBegin);
    HyprlandAPI::addLuaFunction(handle, "cascade_test", "update", swipeUpdate);
    HyprlandAPI::addLuaFunction(handle, "cascade_test", "finish", swipeEnd);
    HyprlandAPI::addLuaFunction(handle, "cascade_test", "workspace", workspace);
    HyprlandAPI::addLuaFunction(handle, "cascade_test", "frames", frameCount);
    frameListener = Event::bus()->m_events.render.stage.listen([](eRenderStage stage) {
        if (stage == RENDER_LAST_MOMENT) ++renderedFrames;
    });
    return {"Cascade gesture test driver", "Isolated native gesture-event fixture", "Cascade contributors", "1.0"};
}
APICALL EXPORT void PLUGIN_EXIT() { frameListener = {}; }
