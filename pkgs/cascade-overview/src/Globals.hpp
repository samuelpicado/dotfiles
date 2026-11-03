#pragma once

#include "Model.hpp"
#include <functional>
#include <tuple>
#include <type_traits>

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/config/values/types/IntValue.hpp>

using SCallbackInfo = Event::SCallbackInfo;

template <typename T>
using HyprSignalRefArg = std::conditional_t<std::is_trivially_copyable_v<T>, T, const T&>;

// Hyprutils stores trivially copyable event arguments by value in the emitted tuple.
template <typename EventType, typename Signal>
CHyprSignalListener listenCancellable(Signal& signal, std::function<void(const EventType&, SCallbackInfo&)> handler) {
    struct ListenerAccess : Hyprutils::Signal::CSignalBase {
        using CSignalBase::registerListenerInternal;
    };
    return reinterpret_cast<ListenerAccess&>(signal).registerListenerInternal([handler](void* args) {
        using Tuple = std::tuple<HyprSignalRefArg<EventType>, HyprSignalRefArg<Event::SCallbackInfo&>>;
        auto* tuple = static_cast<Tuple*>(args);
        handler(std::get<0>(*tuple), std::get<1>(*tuple));
    });
}

inline HANDLE pHandle = nullptr;
std::vector<Cascade::Id> cascadeReservedDesktops();
void cascadeCursorOverride(bool acquire);
void cascadeRefreshEmptyDesktops();

struct SConfig {
    SP<Config::Values::CIntValue> animate = makeShared<Config::Values::CIntValue>(
        "plugin:cascade:animate", "Animate Cascade overview transitions", 1, Config::Values::SIntValueOptions{.min = 0, .max = 1});
    SP<Config::Values::CIntValue> gestures = makeShared<Config::Values::CIntValue>(
        "plugin:cascade:gestures", "Enable three and four finger Cascade swipes", 1, Config::Values::SIntValueOptions{.min = 0, .max = 1});
    SP<Config::Values::CIntValue> reverseSwipe = makeShared<Config::Values::CIntValue>(
        "plugin:cascade:reverseSwipe", "Reverse Cascade swipe navigation", 0, Config::Values::SIntValueOptions{.min = 0, .max = 1});
    SP<Config::Values::CIntValue> cardWidth = makeShared<Config::Values::CIntValue>(
        "plugin:cascade:cardWidth", "Desktop card width in logical pixels", 480, Config::Values::SIntValueOptions{.min = 160});
    SP<Config::Values::CIntValue> rowHeight = makeShared<Config::Values::CIntValue>(
        "plugin:cascade:rowHeight", "Group row height in logical pixels", 340, Config::Values::SIntValueOptions{.min = 120});
};

inline SConfig config;
