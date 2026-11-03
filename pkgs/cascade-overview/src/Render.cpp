#include "Overview.hpp"
#include "Globals.hpp"
#include <hyprland/src/helpers/memory/Memory.hpp>
#include <hyprland/src/config/shared/complex/ComplexDataTypes.hpp>
#include <hyprland/src/render/pass/RectPassElement.hpp>
#include <hyprland/src/render/pass/BorderPassElement.hpp>
#include <hyprland/src/render/pass/SurfacePassElement.hpp>
#include <hyprland/src/render/pass/RendererHintsPassElement.hpp>
#include <hyprland/src/state/WorkspaceState.hpp>
#include <hyprland/src/desktop/state/WindowState.hpp>
#include <hyprlang.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
#include <algorithm>
#include <climits>
#include "Layout.hpp"
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/Texture.hpp>
#include <hyprland/src/render/pass/TexPassElement.hpp>
#include <hyprland/src/managers/fullscreen/FullscreenController.hpp>
#include <hyprland/src/pointer/PointerManager.hpp>


void renderRect(CBox box, CHyprColor color) {
    CRectPassElement::SRectData rectdata;
    rectdata.color = color;
    rectdata.box = box;
    g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(rectdata));
}

void renderRectWithBlur(CBox box, CHyprColor color) {
    CRectPassElement::SRectData rectdata;
    rectdata.color = color;
    rectdata.box = box;
    rectdata.blur = true;
    g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(rectdata));
}

void renderBorder(CBox box, const Config::CGradientValueData& gradient, int size) {
    CBorderPassElement::SBorderData data;
    data.box = box;
    data.grad1 = gradient;
    data.round = 0;
    data.a = 1.f;
    data.borderSize = size;
    g_pHyprRenderer->m_renderPass.add(makeUnique<CBorderPassElement>(data));
}

void cascadeWindow(PHLWINDOW pWindow, PHLMONITOR pMonitor, PHLWORKSPACE pWorkspaceOverride, CBox rectOverride, CBox clipBox, const Time::steady_tp& time, float alpha) {
    if (!pWindow || !pMonitor || !pWorkspaceOverride) return;
    if (!pWindow->m_isMapped || !pWindow->wlSurface() || !pWindow->wlSurface()->resource()) return;

    Render::SRenderModifData renderModif;

    const auto oRealPosition = pWindow->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
    const auto oSize = pWindow->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
    const float    logicalW = std::max((float)oSize.x, 5.F);
    const float    scaleMod = rectOverride.w / std::max(logicalW * pMonitor->m_scale, 5.F);
    if (!(scaleMod > 0.F) || !(rectOverride.w > 0 && rectOverride.h > 0)) return;

    const Vector2D logicalTL = oRealPosition + pWindow->m_floatingOffset;
    const Vector2D scaledTL  = (logicalTL - pMonitor->m_position) * pMonitor->m_scale;
    const Vector2D translate = rectOverride.pos() / scaleMod - scaledTL;

    renderModif.modifs.push_back(std::make_pair(Render::SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, std::any(translate)));
    renderModif.modifs.push_back(std::make_pair(Render::SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, std::any(scaleMod)));
    renderModif.enabled = true;

    g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{.renderModif = renderModif}));
    Hyprutils::Utils::CScopeGuard x([] {
        g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{.renderModif = Render::SRenderModifData{}}));
    });

    g_pHyprRenderer->damageWindow(pWindow);

    CSurfacePassElement::SRenderData renderdata{};
    renderdata.pMonitor = pMonitor;
    renderdata.when = time;
    renderdata.pos                  = oRealPosition + pWindow->m_floatingOffset;
    renderdata.w                    = std::max(oSize.x, 5.0);
    renderdata.h                    = std::max(oSize.y, 5.0);
    renderdata.surface              = pWindow->wlSurface()->resource();
    renderdata.dontRound            = Fullscreen::controller()->isFullscreen(pWindow, Fullscreen::FSMODE_FULLSCREEN);
    renderdata.fadeAlpha            = 1.F;
    renderdata.alpha                = alpha * 0.999F;
    renderdata.decorate             = false;
    renderdata.rounding             = renderdata.dontRound ? 0 : pWindow->rounding() * scaleMod * pMonitor->m_scale;
    renderdata.roundingPower        = renderdata.dontRound ? 2.0F : pWindow->roundingPower();
    renderdata.blur                 = false;
    renderdata.pWindow              = pWindow;
    renderdata.clipBox              = clipBox;
    renderdata.useNearestNeighbor   = false;
    renderdata.squishOversized      = true;
    renderdata.surfaceCounter       = 0;

    pWindow->wlSurface()->resource()->breadthfirst(
        [&renderdata, &pWindow](SP<CWLSurfaceResource> s, const Vector2D& offset, void*) {
            if (!s || !s->m_current.texture)
                return;

            if (s->m_current.size.x < 1 || s->m_current.size.y < 1)
                return;

            renderdata.localPos    = offset;
            renderdata.texture     = s->m_current.texture;
            renderdata.surface     = s;
            renderdata.mainSurface = s == pWindow->wlSurface()->resource();
            g_pHyprRenderer->m_renderPass.add(makeUnique<CSurfacePassElement>(renderdata));
            renderdata.surfaceCounter++;
        },
        nullptr);
}

void CCascadeOverview::text(const std::string& value, double x, double y, double size, CHyprColor color, double maxWidth, bool centered) {
    const auto owner = getOwner();
    if (!owner || value.empty()) return;
    const auto scale = owner->m_scale;
    const auto key = value + std::to_string(size * scale) + std::to_string(color.getAsHex()) + std::to_string(maxWidth * scale);
    if (labels.size() > 256) labels.clear();
    auto& texture = labels[key];
    if (!texture) texture = g_pHyprRenderer->renderText(value, color, std::round(size * scale), false, "sans-serif", std::round(maxWidth * scale));
    if (!texture) return;
    CTexPassElement::SRenderData data;
    data.tex = texture;
    data.box = CBox{x * scale, y * scale, texture->m_size.x, texture->m_size.y};
    if (centered) {
        data.box.x -= texture->m_size.x / 2.;
        data.box.y -= texture->m_size.y / 2.;
    }
    data.a = reveal.value;
    data.clipBox = CBox{0, 0, owner->m_transformedSize.x, owner->m_transformedSize.y};
    g_pHyprRenderer->m_renderPass.add(makeUnique<CTexPassElement>(data));
}

void CCascadeOverview::draw() {
    const auto owner = getOwner();
    if (!owner) return;
    if (!active && reveal.value < .001) return;
    const auto now = std::chrono::steady_clock::now();
    const double dt = std::chrono::duration<double>(now - lastFrame).count();
    lastFrame = now;
    const bool animate = config.animate->value() != 0;
    reveal.tick(dt, animate);
    if (!swiping || swipeFingers != 3 || swipeAxis != 2) row.tick(dt, animate);
    for (auto& [id, column] : columns)
        if (!swiping || swipeFingers != 3 || swipeAxis != 1 || id != model.activeGroup) column.tick(dt, animate);

    const double scale = owner->m_scale;
    const double width = owner->m_transformedSize.x / scale, height = owner->m_transformedSize.y / scale;
    const auto geometry = Cascade::Geometry::make(width, height, config.cardWidth->value(), config.rowHeight->value());
    const CBox screen = {0, 0, owner->m_transformedSize.x, owner->m_transformedSize.y};
    const CBox content = {0, 88 * scale, screen.w, std::max(0., screen.h - 132 * scale)};
    const auto previousClip = g_pHyprRenderer->m_renderData.clipBox;
    g_pHyprRenderer->m_renderData.clipBox = screen;
    Hyprutils::Utils::CScopeGuard restoreClip([&] { g_pHyprRenderer->m_renderData.clipBox = previousClip; });
    hits.clear();
    const CHyprColor foreground(.92, .94, .94, 1), muted(.57, .62, .64, 1), accent(.73, .87, .78, 1), warning(.98, .72, .3, 1);
    const bool hasStatus = now < statusUntil;
    auto rect = [&](CBox box, CHyprColor color, int round = 0, bool clipped = true) {
        box.scale(scale);
        CRectPassElement::SRectData data;
        data.box = box;
        data.color = CHyprColor(color.r, color.g, color.b, color.a * reveal.value);
        data.round = round * scale;
        data.clipBox = clipped ? content : screen;
        g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(data));
    };
    auto hit = [&](Kind kind, CBox box, Cascade::Id group = 0, Cascade::Id desktop = 0, PHLWINDOWREF window = {}) {
        box.x += owner->m_position.x;
        box.y += owner->m_position.y;
        hits.push_back({kind, box, group, desktop, window});
    };
    auto button = [&](const std::string& label, Kind kind, double x, double y, double w, Cascade::Id group) {
        CBox box{x, y, w, 26};
        const auto absolute = CBox{x + owner->m_position.x, y + owner->m_position.y, w, 26};
        const bool hovered = absolute.containsPoint(pointer);
        rect(box, hovered ? CHyprColor(.24, .29, .29, 1) : CHyprColor(.14, .17, .18, 1), 6);
        text(label, x + w / 2, y + 13, 12, hovered ? foreground : muted, 0, true);
        hit(kind, box, group);
    };
    rect({0, 0, width, height}, CHyprColor(.035, .044, .052, .97), 0, false);

    for (size_t groupIndex = 0; groupIndex <= model.groups.size(); ++groupIndex) {
        const double y = geometry.centerY + (double(groupIndex) - row.value) * geometry.pitchY - geometry.cardHeight / 2 + (1 - reveal.value) * 26;
        if (y + geometry.cardHeight < 90 || y - 36 > height - 44) continue;
        if (groupIndex == model.groups.size()) {
            const double x = (width - geometry.cardWidth) / 2;
            rect({x, y, geometry.cardWidth, geometry.cardHeight}, CHyprColor(.09, .12, .13, 1), 12);
            text("+", width / 2, std::clamp(y + geometry.cardHeight / 2, 108., height - 62), 24, accent, 0, true);
            hit(Kind::NewGroup, {x, std::max(y, 90.), geometry.cardWidth, std::min(geometry.cardHeight, height - 46 - std::max(y, 90.))});
            continue;
        }
        const auto& group = model.groups[groupIndex];
        const bool selectedGroup = group.id == model.activeGroup;
        const double titleY = std::clamp(y - 34, 94., height - 72);
        const double controlsX = (width - 102) / 2;
        button("↑", Kind::GroupUp, controlsX, titleY - 2, 30, group.id);
        button("↓", Kind::GroupDown, controlsX + 36, titleY - 2, 30, group.id);
        button("×", Kind::RemoveGroup, controlsX + 72, titleY - 2, 30, group.id);

        for (size_t desktopIndex = 0; desktopIndex < group.desktops.size(); ++desktopIndex) {
            const double x = width / 2 - geometry.cardWidth / 2 + (double(desktopIndex) - columns[group.id].value) * geometry.pitchX;
            if (x + geometry.cardWidth < 0 || x > width) continue;
            const Cascade::Id desktop = group.desktops[desktopIndex];
            const bool selected = selectedGroup && desktop == group.selected;
            const bool remembered = desktop == group.selected;
            const double visibleTop = std::max(y, titleY + 30), visibleBottom = std::min(y + geometry.cardHeight, height - 44);
            if (visibleBottom <= visibleTop) continue;
            const CBox card = {x, y, geometry.cardWidth, geometry.cardHeight};
            if (selected) rect({x - 2, y - 2, geometry.cardWidth + 4, geometry.cardHeight + 4}, accent, 13);
            rect(card, selected ? CHyprColor(.12, .16, .17, 1) : CHyprColor(.075, .09, .10, 1), 11);
            hit(Kind::Desktop, {x, visibleTop, geometry.cardWidth, visibleBottom - visibleTop}, group.id, desktop);
            std::vector<PHLWINDOW> windows;
            for (const auto& window : Desktop::windowState()->windows())
                if (window && window->m_isMapped && window->m_workspace && window->m_workspace->m_id == desktop && !window->isHidden()) windows.push_back(window);
            const auto heading = "Escritorio " + std::to_string(desktopIndex + 1) + "  ·  " + std::to_string(windows.size());
            if (y >= 90) {
                text(heading, x + 14, y + 10, 12, remembered ? accent : muted, geometry.cardWidth - 28);
            }
            const CBox clip = {std::max(0., x + 7) * scale, std::max(visibleTop, y + 36) * scale,
                std::max(0., std::min(width, x + geometry.cardWidth - 7) - std::max(0., x + 7)) * scale,
                std::max(0., visibleBottom - 7 - std::max(visibleTop, y + 36)) * scale};
            const int count = windows.size();
            const int cols = std::max(1, int(std::ceil(std::sqrt(count))));
            const int rows = std::max(1, int(std::ceil(double(count) / cols)));
            const double cellW = (geometry.cardWidth - 24) / cols, cellH = (geometry.cardHeight - 52) / rows;
            for (int index = 0; index < count; ++index) {
                const auto& window = windows[index];
                const auto size = window->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
                const double cellX = x + 12 + (index % cols) * cellW, cellY = y + 38 + (index / cols) * cellH;
                auto fitted = Cascade::fit(cellX + 3, cellY + 3, cellW - 6, std::max(8., cellH - 24), size.x, size.y);
                CBox box{fitted.x, fitted.y, fitted.w, fitted.h};
                const auto global = CBox{box.x + owner->m_position.x, box.y + owner->m_position.y, box.w, box.h};
                if (global.containsPoint(pointer) || keyboardWindow.lock() == window)
                    rect({box.x - 2, box.y - 2, box.w + 4, box.h + 4}, accent, 5);
                const auto pixelBox = CBox{box.x * scale, box.y * scale, box.w * scale, box.h * scale};
                cascadeWindow(window, owner, window->m_workspace, pixelBox, clip, Time::steadyNow(), reveal.value);
                const double hitTop = std::max(box.y, visibleTop), hitBottom = std::min(box.y + box.h, visibleBottom);
                if (hitBottom > hitTop && clip.w > 0 && clip.h > 0) hit(Kind::Window, {box.x, hitTop, box.w, hitBottom - hitTop}, group.id, desktop, window);
                if (cellY + cellH - 20 >= visibleTop && cellY + cellH < visibleBottom)
                    text(window->m_title.empty() ? window->m_class : window->m_title, cellX + 4, cellY + cellH - 19, 10, muted, cellW - 8);
            }
            if (windows.empty()) text("Espacio para empezar", x + 20, std::max(visibleTop + 4, y + geometry.cardHeight / 2), 14, muted, geometry.cardWidth - 40);
            if (selected && hasStatus && visibleBottom - visibleTop >= 24)
                text(statusText, x + 14, visibleBottom - 21, 12, warning, geometry.cardWidth - 28);
        }
    }
    // Keep the clipping margins while leaving the surrounding area free of labels.
    rect({0, 0, width, 88}, CHyprColor(.035, .044, .052, 1), 0, false);
    rect({0, height - 44, width, 44}, CHyprColor(.035, .044, .052, 1), 0, false);
    if (compositionLocked && g_pHyprRenderer->shouldRenderCursor())
        Pointer::mgr()->renderSoftwareCursorsFor(owner, Time::steadyNow(), g_pHyprRenderer->m_renderData.damage);
    g_pHyprRenderer->damageMonitor(owner);
    owner->scheduleFrame();
}
