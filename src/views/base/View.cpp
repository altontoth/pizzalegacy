/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/base/View.h"

#include <cassert>
#include "engine/DisplayConstants.h"
#include "views/base/ViewDefs.h"

View::View(GameContext& context, GameState& state, int interaction_priority)
    : context_(context), state_(state),
      interactions_(context.interaction->make_layer(),
                    context.interaction->make_layer(), interaction_priority) {}

void View::mark_dirty() { is_dirty_ = true; }

bool View::is_dirty() const { return is_dirty_; }

void View::clean() { is_dirty_ = false; }

[[nodiscard]] bool View::needs_force_recompose() const { return false; }

std::vector<LayerInfo> View::get_layers() {
  std::vector<LayerInfo> list;
  list.push_back(LayerInfo{
      .view = this,
      .area = {0, 0, DisplayConstants::SCREEN_WIDTH,
               DisplayConstants::SCREEN_HEIGHT},
      .label = "View",
  });
  return list;
}

void View::interaction_origin(ViewPoint p) { interactions_.origin(p); }
