/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include "core/util/GameContext.h"
#include "views/ui/Icon.h"

class FontManager;

namespace views::ui {

// Runs work that stalls the game loop (loading a save, initializing audio)
// behind a busy cursor. The work is held back until a frame with the new
// cursor has been shown to the player.
class BusyCursor {
public:
  explicit BusyCursor(GameContext& context) : context_(context) {}

  // Show the busy cursor now and run `work` from a later tick(). The label
  // is drawn by render_onto() until the work has run; mark the owning view
  // dirty so the label reaches the screen before the work stalls the loop.
  void run(std::function<void()> work, std::string_view label = {});
  void tick();
  bool pending() const { return static_cast<bool>(work_); }
  // Draw the label caption; call from the owning view's render().
  void render_onto(gfx::Surface& surface) const;

private:
  GameContext& context_;
  std::function<void()> work_;
  std::string label_;
  uint64_t requested_frame_{0};
};

// Use an icon from as a standalone surface, with the gray background removed.
gfx::Surface icon_surface(VgaManager& vga, Icon icon);

// Draw a short activity caption ("Loading...", "Ending turn...") in the
// bottom left corner.
void draw_activity_caption(FontManager& font, gfx::Surface& surface,
                           std::string_view text);

} // namespace views::ui
