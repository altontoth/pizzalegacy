/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>

#include "engine/geom/Spaces.h"
#include "engine/ui/InteractionEvent.h"

// Lets the player drag a panel by any part that is not one of its own items.
// The entire panel has to be on the screen.
class PanelDrag {
public:
  // The panel's new top-left corner after a user moved it.
  std::optional<ViewPoint> on_interaction(const views::ui::InteractionEvent& ie,
                                          const ViewRect& panel);

private:
  // Mouse position relative to the panel's corner when it was grabbed.
  ViewDelta grab_offset_;
};
