/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/widgets/PanelDrag.h"

#include "engine/DisplayConstants.h"
#include "engine/geom/TypedRect.h"

std::optional<ViewPoint>
PanelDrag::on_interaction(const views::ui::InteractionEvent& ie,
                          const ViewRect& panel) {
  using Type = views::ui::InteractionEvent::Type;

  if (ie.type == Type::ButtonDown) {
    grab_offset_ = ie.pos - panel.point();
    return std::nullopt;
  }
  if (ie.type != Type::DragStart && ie.type != Type::DragMove) {
    return std::nullopt;
  }
  const ViewRect screen{0, 0, DisplayConstants::SCREEN_WIDTH,
                        DisplayConstants::SCREEN_HEIGHT};
  const ViewPoint to =
      move_inside(ViewRect{ie.pos - grab_offset_, panel.size()}, screen)
          .point();
  if (to == panel.point()) {
    return std::nullopt;
  }
  return to;
}
