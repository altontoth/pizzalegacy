/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <string>

#include "core/util/GameContext.h"
#include "engine/Size.h"
#include "engine/ui/InteractionLayers.h"
#include "gfx/PaletteHandle.h"
#include "gfx/Surface.h"
#include "resources/FontManager.h"
#include "resources/VgaManager.h"
#include "views/base/Dirtyable.h"

using gfx::Surface;

class BasePopup : public Dirtyable {
public:
  BasePopup(const GameContext& context)
      : font_(context.font), vga_(context.vga), bob_(context.bob) {}
  virtual ~BasePopup() = default;
  virtual void tick() {}
  virtual void render(Surface& surface, ViewPoint origin) = 0;
  virtual Size popup_size() const { return {}; }

  // nullopt means we use current UI mode
  virtual std::optional<gfx::PaletteHandle> palette_override() const {
    return std::nullopt;
  }

  bool is_finished() const { return finished_; };
  void finished() { finished_ = true; };
  virtual void add_interactions(views::ui::InteractionContext, ViewPoint) = 0;

  Surface popup_surface(int width, int height);
  Surface text_button(const std::string& text, int width, int height,
                      bool mouse_down);

protected:
  FontManager* font_;
  VgaManager* vga_;
  BobManager* bob_;

private:
  Surface button_surface(int width, int height, bool mouse_down);
  bool finished_{false};
};
