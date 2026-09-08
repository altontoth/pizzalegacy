/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/util/GameContext.h"
#include "engine/Size.h"
#include "engine/gfx/PaletteHandle.h"
#include "engine/gfx/Surface.h"
#include "engine/input/Action.h"
#include "engine/input/InputListener.h"
#include "engine/input/ListenerToken.h"
#include "engine/ui/InteractionLayers.h"
#include "resources/FontManager.h"
#include "resources/VgaManager.h"
#include "views/base/Dirtyable.h"

class PopupButton;

using gfx::Surface;

class BasePopup : public Dirtyable, public input::InputListener {
public:
  BasePopup(const GameContext& context);
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

  // Any shortcut that was registered gets handled here.
  bool on_action(const input::ActionEvent& ev) override;

  // The beveled frame popup_surface() draws is this wide on every side.
  static constexpr int popup_border = 10;
  Surface popup_surface(int width, int height);
  Surface text_button(const std::string& text, int width, int height,
                      bool mouse_down);

protected:
  // Fire button->trigger() when action arrives
  void add_shortcut(input::Action action, PopupButton* button);

  FontManager* font_;
  VgaManager* vga_;
  BobManager* bob_;

private:
  Surface button_surface(int width, int height, bool mouse_down);
  input::ListenerToken listener_token_;
  std::vector<std::pair<input::Action, PopupButton*>> shortcuts_;
  bool finished_{false};
};
