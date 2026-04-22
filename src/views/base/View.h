/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <map>
#include "core/util/GameContext.h"
#include "engine/LayerInfo.h"
#include "engine/RenderContext.h"
#include "engine/Renderable.h"
#include "engine/geom/Spaces.h"
#include "engine/ui/InteractionLayers.h"

class GameState;

class View : public Renderable {
public:
  View(GameContext& context, GameState& state);
  virtual ~View() = default;
  void tick() override = 0;
  void render(const RenderContext& ctx) override = 0;
  void mark_dirty() override;
  bool is_dirty() const override;
  void clean() override;
  bool needs_force_recompose() const override;
  std::vector<LayerInfo> get_layers() override;
  void interaction_origin(ViewPoint p);

  // Background preservation control for overlay views
  void set_preserve_background(bool preserve) {
    preserve_background_ = preserve;
  }
  bool preserve_background() const override { return preserve_background_; }

protected:
  uint32_t color(uint8_t, uint8_t, uint8_t);

  GameContext& context_;
  GameState& state_;
  views::ui::InteractionLayers interactions_;
  // Set to true to render over existing content
  bool preserve_background_{false};

private:
  std::map<char, char> shift_key_mapping;
  bool is_dirty_{true};
};
