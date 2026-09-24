/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <string>
#include <vector>

#include "engine/Size.h"
#include "engine/geom/Spaces.h"
#include "engine/gfx/ColorIndex.h"
#include "engine/input/InputListener.h"
#include "engine/ui/InteractionLayers.h"
#include "resources/FontTypes.h"
#include "views/ui/popups/BasePopup.h"

struct ContextMenuItem {
  std::string text;
  std::function<void()> callback;
};

struct ContextMenuStyle {
  FontFace font = FontFace::CSTANDAR;
  gfx::ColorIndex text_color = 10;
  gfx::ColorIndex hover_fill = 10;
  gfx::ColorIndex hover_text_color = 15;
};

// A list of actions on the thing that was right-clicked. Clicking an entry runs
// it and closes the menu; Escape or a click outside closes it.
class ContextMenuPopup : public BasePopup {
public:
  ContextMenuPopup(const GameContext& context,
                   std::vector<ContextMenuItem> items,
                   ContextMenuStyle style = {});
  void add_interactions(views::ui::InteractionContext ictx,
                        ViewPoint origin) override;
  void render(Surface& surface, ViewPoint origin) override;
  Size popup_size() const override { return size_; }
  bool on_action(const input::ActionEvent& ev) override;

private:
  // Five one-pixel lines of frame around the rows.
  static constexpr int frame_border = 5;
  ViewRect row_rect(int i) const;

  std::vector<ContextMenuItem> items_;
  ContextMenuStyle style_;
  Size size_;
  int row_height_{0};
  int hovered_{-1};
};
