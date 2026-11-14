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
#include <set>
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

void CCascadeOverview::text(const std::string& value, double x, double y, double size, CHyprColor color, double maxWidth, bool centered, double opacity, double zoom) {
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
    data.box = CBox{x * scale, y * scale, texture->m_size.x * zoom, texture->m_size.y * zoom};
    if (centered) {
        data.box.x -= data.box.w / 2.;
        data.box.y -= data.box.h / 2.;
    }
    data.a = std::clamp(reveal.value, 0., 1.) * opacity;
    data.clipBox = CBox{0, 0, owner->m_transformedSize.x, owner->m_transformedSize.y};
    g_pHyprRenderer->m_renderPass.add(makeUnique<CTexPassElement>(data));
}

void CCascadeOverview::draw() {
    const auto owner = getOwner();
    if (!owner || (!active && reveal.value == 0)) return;
    const auto now = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now - lastFrame).count();
    if (dt > .1) dt = 1. / std::max(30., double(owner->m_refreshRate));
    lastFrame = now;
    const bool animate = config.animate->value() != 0;
    reveal.tick(dt, animate);
    if (!swiping || swipeFingers != 3 || swipeAxis != 2) row.tick(dt, animate);
    for (auto& [id, column] : columns)
        if (!swiping || swipeFingers != 3 || swipeAxis != 1 || id != model.activeGroup) column.tick(dt, animate);
    for (auto& [id, card] : cards) { card.x.tick(dt, animate); card.y.tick(dt, animate); }
    for (auto& [id, group] : groupRows) group.tick(dt, animate);
    if (dragPreview) {
        const auto window = dragPreview->source.window.lock();
        if (dragPreview->source.kind == Kind::Window ? !window || !window->m_isMapped : !model.groupFor(dragPreview->source.desktop))
            dragPreview.reset();
    }
    if (dragPreview && dragPreview->settling) {
        dragPreview->progress.tick(dt, animate);
        if (dragPreview->progress.value == 1) dragPreview.reset();
    }

    const double alpha = std::clamp(reveal.value, 0., 1.);
    const double scale = owner->m_scale;
    const double width = owner->m_transformedSize.x / scale, height = owner->m_transformedSize.y / scale;
    const auto geometry = Cascade::Geometry::make(width, height, config.cardWidth->value(), config.rowHeight->value());
    const CBox screen = {0, 0, owner->m_transformedSize.x, owner->m_transformedSize.y};
    const CBox content = {0, 88 * scale, screen.w, std::max(0., screen.h - 132 * scale)};
    const auto previousClip = g_pHyprRenderer->m_renderData.clipBox;
    g_pHyprRenderer->m_renderData.clipBox = screen;
    Hyprutils::Utils::CScopeGuard restoreClip([&] { g_pHyprRenderer->m_renderData.clipBox = previousClip; });
    const CHyprColor foreground(.20, .25, .29, 1), muted(.38, .43, .47, 1), silver(.72, .77, .82, 1), warning(.78, .42, .08, 1);
    const bool hasStatus = now < statusUntil;
    const auto hovered = hitAt(pointer, !dragging);
    const auto hover = hovered ? std::optional<Hit>(*hovered) : std::nullopt;
    hits.clear();
    std::map<Cascade::Id, std::vector<PHLWINDOW>> desktopWindows;
    for (const auto& window : Desktop::windowState()->windows())
        if (window && window->m_isMapped && window->m_workspace && !window->isHidden())
            desktopWindows[window->m_workspace->m_id].push_back(window);
    std::set<void*> renderedSurfaces;
    auto rect = [&](CBox box, CHyprColor color, int round = 0, bool clipped = true, bool blur = false) {
        box.scale(scale);
        CRectPassElement::SRectData data;
        data.box = box;
        data.color = CHyprColor(color.r, color.g, color.b, color.a * alpha);
        data.round = round * scale;
        data.clipBox = clipped ? content : screen;
        data.blur = blur;
        data.blurA = alpha;
        g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(data));
    };
    auto hit = [&](Kind kind, CBox box, Cascade::Id group = 0, Cascade::Id desktop = 0, PHLWINDOWREF window = {}) {
        box.x += owner->m_position.x;
        box.y += owner->m_position.y;
        hits.push_back({kind, box, group, desktop, window});
    };
    auto glass = [&](CBox box, bool selected, bool pointed, double opacity = 1., bool clipped = true) {
        rect({box.x - 3, box.y + 4, box.w + 6, box.h + 2}, CHyprColor(.25, .30, .35, .08 * opacity), 16, clipped);
        rect({box.x - 1, box.y - 1, box.w + 2, box.h + 2},
             CHyprColor(selected || pointed ? .62 : .85, selected || pointed ? .68 : .89, selected || pointed ? .74 : .93, .85 * opacity), 14, clipped);
        rect(box, CHyprColor(.88, .91, .94, (selected || pointed ? .76 : .55) * opacity), 13, clipped);
        rect({box.x + 14, box.y + 1, std::max(0., box.w - 28), 1}, CHyprColor(1., 1., 1., .75 * opacity), 0, clipped);
    };
    auto button = [&](const std::string& label, Kind kind, double x, double y, double w, Cascade::Id group) {
        CBox box{x, y, w, 26};
        const bool pointed = CBox{x + owner->m_position.x, y + owner->m_position.y, w, 26}.containsPoint(pointer);
        rect(box, CHyprColor(.96, .97, .99, pointed ? .78 : .38), 8);
        text(label, x + w / 2, y + 13, 12, pointed ? foreground : muted, 0, true);
        hit(kind, box, group);
    };
    auto windowPreview = [&](PHLWINDOW window, CBox box, CBox clip, double opacity) {
        if (!window || !window->wlSurface() || !window->wlSurface()->resource()) return;
        const CBox pixels{box.x * scale, box.y * scale, box.w * scale, box.h * scale};
        if (clip.w <= 0 || clip.h <= 0 || pixels.x + pixels.w <= clip.x || pixels.y + pixels.h <= clip.y || pixels.x >= clip.x + clip.w || pixels.y >= clip.y + clip.h) return;
        window->wlSurface()->resource()->breadthfirst([&](SP<CWLSurfaceResource> surface, const Vector2D&, void*) {
            if (!surface) return;
            renderedSurfaces.insert(surface.get());
            if (!previewListeners.contains(surface.get())) {
                previewListeners.emplace(surface.get(), surface->m_events.commit.listen([this] {
                    if (active) if (const auto monitor = getOwner()) { g_pHyprRenderer->damageMonitor(monitor); monitor->scheduleFrame(); }
                }));
            }
            // Native frame callbacks cover active workspaces; visible previews need them too.
            surface->frame(Time::steadyNow());
        }, nullptr);
        cascadeWindow(window, owner, window->m_workspace, pixels, clip, Time::steadyNow(), alpha * opacity);
    };
    auto desktopContents = [&](Cascade::Id desktop, CBox card, double opacity, bool interactive) {
        const double unit = card.w / geometry.cardWidth;
        const double visibleTop = interactive ? std::max(card.y, 90.) : card.y;
        const double visibleBottom = interactive ? std::min(card.y + card.h, height - 44) : card.y + card.h;
        const auto& windows = desktopWindows[desktop];
        const auto group = model.group(model.groupFor(desktop));
        const auto index = group ? std::find(group->desktops.begin(), group->desktops.end(), desktop) - group->desktops.begin() + 1 : 1;
        if (card.y >= (interactive ? 90 : 0))
            text("Escritorio " + std::to_string(index) + "  ·  " + std::to_string(windows.size()), card.x + 14 * unit, card.y + 10 * unit,
                 12, foreground, geometry.cardWidth - 28, false, opacity, unit);
        const CBox clip = {std::max(0., card.x + 7 * unit) * scale, std::max(visibleTop, card.y + 36 * unit) * scale,
            std::max(0., std::min(width, card.x + card.w - 7 * unit) - std::max(0., card.x + 7 * unit)) * scale,
            std::max(0., visibleBottom - 7 * unit - std::max(visibleTop, card.y + 36 * unit)) * scale};
        const int count = windows.size();
        const int cols = std::max(1, int(std::ceil(std::sqrt(count))));
        const int rows = std::max(1, int(std::ceil(double(count) / cols)));
        const double cellW = (card.w - 24 * unit) / cols, cellH = (card.h - 52 * unit) / rows;
        for (int i = 0; i < count; ++i) {
            const auto& window = windows[i];
            const auto size = window->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
            const double cellX = card.x + 12 * unit + (i % cols) * cellW, cellY = card.y + 38 * unit + (i / cols) * cellH;
            const auto fitted = Cascade::fit(cellX + 3 * unit, cellY + 3 * unit, cellW - 6 * unit, std::max(8., cellH - 24 * unit), size.x, size.y);
            const CBox box{fitted.x, fitted.y, fitted.w, fitted.h};
            const auto global = CBox{box.x + owner->m_position.x, box.y + owner->m_position.y, box.w, box.h};
            if (interactive && (global.containsPoint(pointer) || keyboardWindow.lock() == window))
                rect({box.x - 2, box.y - 2, box.w + 4, box.h + 4}, silver, 5);
            double windowOpacity = opacity;
            if (interactive && dragPreview && dragPreview->source.kind == Kind::Window && dragPreview->source.window.lock() == window)
                windowOpacity *= dragPreview->settling ? dragPreview->progress.value : .35;
            windowPreview(window, box, clip, windowOpacity);
            const double hitTop = std::max(box.y, visibleTop), hitBottom = std::min(box.y + box.h, visibleBottom);
            if (interactive && hitBottom > hitTop && clip.w > 0 && clip.h > 0) hit(Kind::Window, {box.x, hitTop, box.w, hitBottom - hitTop}, group->id, desktop, window);
            if (cellY + cellH - 20 * unit >= visibleTop && cellY + cellH < visibleBottom)
                text(window->m_title.empty() ? window->m_class : window->m_title, cellX + 4 * unit, cellY + cellH - 19 * unit, 10, muted, cellW / unit - 8, false, opacity, unit);
        }
        if (windows.empty()) text("Espacio para empezar", card.x + 20 * unit, std::max(visibleTop + 4, card.y + card.h / 2), 14, muted, geometry.cardWidth - 40, false, opacity, unit);
    };

    // One native backdrop blur, shared by every glass card and miniature.
    rect({0, 0, width, height}, CHyprColor(.80, .83, .86, .68), 0, false, true);
    for (size_t groupIndex = 0; groupIndex <= model.groups.size(); ++groupIndex) {
        const double groupY = groupIndex < model.groups.size() ? groupRows[model.groups[groupIndex].id].value : double(groupIndex);
        const double y = geometry.centerY + (groupY - row.value) * geometry.pitchY - geometry.cardHeight / 2 + (1 - alpha) * 24;
        if (groupIndex == model.groups.size()) {
            if (y + geometry.cardHeight < 90 || y > height - 44) continue;
            const double x = (width - geometry.cardWidth) / 2;
            glass({x, y, geometry.cardWidth, geometry.cardHeight}, false, hover && hover->kind == Kind::NewGroup);
            text("+", width / 2, std::clamp(y + geometry.cardHeight / 2, 108., height - 62), 24, muted, 0, true);
            hit(Kind::NewGroup, {x, std::max(y, 90.), geometry.cardWidth, std::min(geometry.cardHeight, height - 46 - std::max(y, 90.))});
            continue;
        }
        const auto& group = model.groups[groupIndex];
        if (y + geometry.cardHeight >= 90 && y - 36 <= height - 44) {
            const double titleY = std::clamp(y - 34, 94., height - 72), controlsX = (width - 102) / 2;
            button("↑", Kind::GroupUp, controlsX, titleY - 2, 30, group.id);
            button("↓", Kind::GroupDown, controlsX + 36, titleY - 2, 30, group.id);
            button("×", Kind::RemoveGroup, controlsX + 72, titleY - 2, 30, group.id);
        }
        for (const auto desktop : group.desktops) {
            const auto& position = cards[desktop];
            const double x = width / 2 - geometry.cardWidth / 2 + (position.x.value - columns[group.id].value) * geometry.pitchX;
            const double cardY = geometry.centerY + (position.y.value - row.value) * geometry.pitchY - geometry.cardHeight / 2 + (1 - alpha) * 24;
            if (x + geometry.cardWidth < 0 || x > width || cardY + geometry.cardHeight < 90 || cardY > height - 44) continue;
            const bool selected = group.id == model.activeGroup && desktop == group.selected;
            const bool pointed = hover && hover->desktop == desktop && (dragging || hover->kind == Kind::Desktop);
            const double opacity = dragPreview && dragPreview->source.kind == Kind::Desktop && dragPreview->source.desktop == desktop
                ? (dragPreview->settling ? .5 + .5 * dragPreview->progress.value : .5) : 1.;
            const CBox card{x, cardY, geometry.cardWidth, geometry.cardHeight};
            glass(card, selected, pointed, opacity);
            const double visibleTop = std::max(cardY, 90.), visibleBottom = std::min(cardY + geometry.cardHeight, height - 44);
            hit(Kind::Desktop, {x, visibleTop, geometry.cardWidth, visibleBottom - visibleTop}, group.id, desktop);
            desktopContents(desktop, card, opacity, true);
            if (selected && hasStatus && visibleBottom - visibleTop >= 24)
                text(statusText, x + 14, visibleBottom - 21, 12, warning, geometry.cardWidth - 28);
        }
    }
    if (dragPreview) {
        auto& preview = *dragPreview;
        const auto window = preview.source.window.lock();
        const bool desktop = preview.source.kind == Kind::Desktop;
        CBox box = preview.box;
        double opacity = 1.;
        if (preview.settling) {
            CBox destination = preview.source.box;
            for (const auto& target : hits)
                if (desktop ? target.kind == Kind::Desktop && target.desktop == preview.source.desktop
                            : target.kind == Kind::Window && target.window.lock() == window) { destination = target.box; break; }
            destination.x -= owner->m_position.x;
            destination.y -= owner->m_position.y;
            const double t = preview.progress.value;
            box = {box.x + (destination.x - box.x) * t, box.y + (destination.y - box.y) * t,
                   box.w + (destination.w - box.w) * t, box.h + (destination.h - box.h) * t};
            opacity = 1 - t;
        }
        glass(box, true, true, opacity, false);
        if (desktop) desktopContents(preview.source.desktop, box, opacity, false);
        else if (window) {
            const CBox image{box.x + 6, box.y + 6, std::max(1., box.w - 12), std::max(1., box.h - 32)};
            windowPreview(window, image, screen, opacity);
            text(window->m_title.empty() ? window->m_class : window->m_title, box.x + 8, box.y + box.h - 20, 10, foreground, std::max(1., preview.box.w - 16), false, opacity);
        }
    }
    std::erase_if(previewListeners, [&](const auto& entry) { return !renderedSurfaces.contains(entry.first); });
    if (compositionLocked && g_pHyprRenderer->shouldRenderCursor())
        Pointer::mgr()->renderSoftwareCursorsFor(owner, Time::steadyNow(), g_pHyprRenderer->m_renderData.damage);
    bool moving = reveal.value != reveal.target || ((!swiping || swipeFingers != 3 || swipeAxis != 2) && row.value != row.target) || (dragPreview && dragPreview->settling) || hasStatus;
    for (const auto& [id, column] : columns)
        if (!swiping || swipeFingers != 3 || swipeAxis != 1 || id != model.activeGroup) moving |= column.value != column.target;
    for (const auto& [id, card] : cards) moving |= card.x.value != card.x.target || card.y.value != card.y.target;
    for (const auto& [id, group] : groupRows) moving |= group.value != group.target;
    const auto local = pointer - owner->m_position;
    moving |= dragging && (local.x < 48 || local.x > width - 48 || local.y < 120 || local.y > height - 72);
    // A final frame releases the software cursor outside the renderer after closing.
    if (moving || (!active && compositionLocked)) { g_pHyprRenderer->damageMonitor(owner); owner->scheduleFrame(); }
}
