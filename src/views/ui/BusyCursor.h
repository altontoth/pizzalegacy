/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <functional>
#include "core/util/GameContext.h"
#include "views/ui/Icon.h"

namespace views::ui {

// Runs work that stalls the game loop (loading a save, initializing audio)
// behind a busy cursor. The work is held back until a frame with the new
// cursor has been shown to the player.
class BusyCursor {
public:
  explicit BusyCursor(GameContext& context) : context_(context) {}

  // Show the busy cursor now and run `work` from a later tick().
  void run(std::function<void()> work);
  void tick();
  bool pending() const { return static_cast<bool>(work_); }

private:
  GameContext& context_;
  std::function<void()> work_;
  uint64_t requested_frame_{0};
};

// Use an icon from as a standalone surface, with the gray background removed.
gfx::Surface icon_surface(VgaManager& vga, Icon icon);

} // namespace views::ui
