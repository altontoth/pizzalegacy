/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <memory>
#include <optional>

#include "core/food/Pizza.h"
#include "core/food/StandardPizza.h"
#include "engine/Size.h"
#include "engine/geom/Spaces.h"
#include "engine/gfx/PaletteHandle.h"
#include "engine/input/Action.h"
#include "engine/ui/InteractionLayers.h"
#include "views/base/BobRenderer.h"
#include "views/ui/popups/BasePopup.h"
#include "views/ui/popups/PopupButton.h"

// A cook book page: a standard pizza's ingredients, and pictures of three of
// its stages.
class CookBookPopup : public BasePopup {
public:
  // Called with the pizza on the page whenever it changes, and with nullopt
  // when the page closes.
  using PageShown =
      std::function<void(std::optional<core::food::StandardPizza>)>;

  CookBookPopup(const GameContext& context, core::food::StandardPizza pizza,
                PageShown page_shown = nullptr);

  void render(Surface& surface, ViewPoint origin) override;
  Size popup_size() const override { return size; }
  void add_interactions(views::ui::InteractionContext ictx,
                        ViewPoint origin) override;
  bool on_action(const input::ActionEvent& ev) override;
  std::optional<gfx::PaletteHandle> palette_override() const override;

private:
  void turn(int pages);
  void close();

  static constexpr Size size{592, 360};

  core::food::StandardPizza pizza_id_;
  std::unique_ptr<core::food::Pizza> pizza_;
  PageShown page_shown_;
  BobRenderer renderer_;
  PopupButton back_;
  PopupButton forward_;
  PopupButton close_;
};
