/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include "core/util/LegacyConstants.h"
#include "engine/ui/InteractionLayers.h"
#include "views/base/ViewDefs.h"
#include "views/helpers/camera/CameraController.h"

struct ScrollEdgeRects {
  ViewRect left;
  ViewRect right;
  ViewRect top;
  ViewRect bottom;
};

class ScrollEdges {
public:
  explicit ScrollEdges(CameraController& sc, ScrollEdgeRects ser)
      : sc_(sc), edges_(ser) {}

  void add_interactions(views::ui::InteractionLayers& layers) const {
    auto cb = [this](ScrollEdge e, const views::ui::InteractionEvent& ev) {
      switch (ev.type) {
      case views::ui::InteractionEvent::Type::ButtonDown:
        if (ev.button == views::ui::MouseButton::Left) {
          sc_.edge_active(e, true);
        }
        break;
      case views::ui::InteractionEvent::Type::Cancelled:
      case views::ui::InteractionEvent::Type::ButtonUp:
        if (ev.button == views::ui::MouseButton::Left) {
          sc_.edge_active(e, false);
        }
        break;
      default:
        break;
      }
    };

    // local rects; origin already applied by InteractionLayers
    layers.create_persistent_item(edges_.left, "scroll_edge_left",
                                  [cb](auto ev) { cb(ScrollEdge::Left, ev); });
    layers.create_persistent_item(edges_.right, "scroll_edge_right",
                                  [cb](auto ev) { cb(ScrollEdge::Right, ev); });
    layers.create_persistent_item(edges_.top, "scroll_edge_top",
                                  [cb](auto ev) { cb(ScrollEdge::Top, ev); });
    layers.create_persistent_item(
        edges_.bottom, "scroll_edge_bottom",
        [cb](auto ev) { cb(ScrollEdge::Bottom, ev); });
  }

private:
  CameraController& sc_;
  ScrollEdgeRects edges_;
};

constexpr ScrollEdgeRects wood_edge_rects(ViewPoint panel_origin) {
  using namespace views::base;
  using namespace LegacyConstants;

  const int left = WOOD_BORDER_WIDTH_LEFT;
  const int right = WOOD_BORDER_WIDTH_RIGHT;
  const int top = WOOD_BORDER_HEIGHT_TOP;
  const int bottom = WOOD_BORDER_HEIGHT_BOTTOM;

  const int panel_x = panel_origin.x; // top-left of main panel on screen
  const int panel_y = panel_origin.y;

  ViewRect rLeft{panel_x - left, panel_y, left,
                 top + MAIN_PANEL_HEIGHT + bottom};
  ViewRect rRight{panel_x + MAIN_PANEL_WIDTH, panel_y, right,
                  top + MAIN_PANEL_HEIGHT + bottom};
  ViewRect rTop{panel_x, panel_y - top, MAIN_PANEL_WIDTH, top};
  ViewRect rBot{panel_x, panel_y + MAIN_PANEL_HEIGHT, MAIN_PANEL_WIDTH, bottom};

  return {.left = rLeft, .right = rRight, .top = rTop, .bottom = rBot};
}
