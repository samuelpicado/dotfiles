#pragma once

#include "Model.hpp"
#include "Layout.hpp"
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/devices/IPointer.hpp>
#include <chrono>
#include <filesystem>
#include <map>

namespace Render { class ITexture; }

// Native rendering and surface transforms derive from Hyprspace (GPL-2.0).
class CCascadeOverview {
public:
    explicit CCascadeOverview(int64_t owner);
    ~CCascadeOverview();
    PHLMONITOR getOwner();
    bool isActive() const { return active; }
    bool isSwiping() const { return swiping; }
    void show();
    void hide(bool immediate = false);
    void beforeRender();
    void draw();
    void sync();
    void refreshEmptyDesktop();
    void save();
    void navigate(int dx, int dy, bool moveWindow = false);
    bool command(const std::string& command, std::string& error);
    std::string inspect();
    std::vector<Cascade::Id> reservedDesktops() const;
    bool buttonEvent(bool pressed, Vector2D coordinates, uint32_t button);
    bool axisEvent(double delta, wl_pointer_axis axis, Vector2D coordinates);
    void motionEvent(Vector2D coordinates);
    bool keyEvent(uint32_t keysym, bool shift, bool control, bool alt = false, bool super = false);
    bool beginSwipe(const IPointer::SSwipeBeginEvent& event);
    bool updateSwipe(const IPointer::SSwipeUpdateEvent& event);
    bool endSwipe(const IPointer::SSwipeEndEvent& event);

private:
    // Preserve the remaining hit IDs exposed by hyprctl cascade.
    enum class Kind { Desktop, Window, NewGroup = 3, GroupUp = 6, GroupDown, RemoveGroup };
    struct Hit {
        Kind kind;
        CBox box;
        Cascade::Id group = 0, desktop = 0;
        PHLWINDOWREF window;
    };
    struct CardMotion {
        Cascade::Motion x, y;
        Cascade::Id group = 0;
    };
    struct DragPreview {
        Hit source;
        CBox box;
        Cascade::Motion progress{0, 1};
        bool settling = false;
    };
    int64_t ownerID;
    Cascade::Model model;
    std::filesystem::path statePath;
    std::vector<Hit> hits;
    bool active = false, swiping = false, dragging = false, compositionLocked = false;
    bool saveFailed = false, readOnlyState = false;
    std::string statusText;
    std::chrono::steady_clock::time_point statusUntil{};
    std::optional<Hit> pressedHit;
    std::optional<DragPreview> dragPreview;
    Vector2D pressPosition = {0, 0}, pointer = {0, 0}, swipeDelta = {0, 0};
    int swipeAxis = 0;
    uint32_t swipeFingers = 0;
    double wheelX = 0, wheelY = 0;
    Cascade::Motion reveal, row;
    std::map<Cascade::Id, Cascade::Motion> columns;
    std::map<Cascade::Id, Cascade::Motion> groupRows;
    std::map<Cascade::Id, CardMotion> cards;
    std::map<void*, CHyprSignalListener> previewListeners;
    std::chrono::steady_clock::time_point lastFrame = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point lastWheel = lastFrame;
    std::map<std::string, SP<Render::ITexture>> labels;
    PHLWINDOWREF keyboardWindow;
    PHLWORKSPACE desktopSwipeSource, desktopSwipeTarget;
    int desktopSwipeDirection = 0;
    double desktopSwipeDelta = 0;
    bool desktopSwipePending = false;
    std::vector<Cascade::Id> occupied() const;
    std::vector<Cascade::Id> nonempty() const;
    void targets(bool warp = false);
    void focus(Cascade::Id desktop, bool close = false);
    bool moveWindow(PHLWINDOW window, Cascade::Id desktop);
    void activate(const Hit& hit);
    void drop(const Hit& from, const Hit& to);
    CBox dragBox(const Hit& source, Vector2D coordinates);
    void notify(const std::string& message);
    void releaseComposition();
    void updateDesktopSwipe();
    void finishDesktopSwipe(bool commit, bool immediate = false);
    void cancelDesktopSwipe();
    const Hit* hitAt(Vector2D coordinates, bool includeWindows = true) const;
    void text(const std::string& value, double x, double y, double size, CHyprColor color, double maxWidth = 0, bool centered = false, double opacity = 1, double zoom = 1);
};
