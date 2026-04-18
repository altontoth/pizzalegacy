/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/layers/DebugLayer.h"
#include <algorithm>
#include "engine/input/InputManager.h"
#include "gfx/Draw.h"
#include "resources/FontManager.h"

DebugLayer::DebugLayer(GameContext& context, GameState& state)
    : View(context, state), listener_(context.input->register_listener(*this)) {
}

// TODO: Introduce some kind of DebugOverlayItem bus that views can push to and
// we just render that.
void DebugLayer::render(const RenderContext& ctx) {
  if (!is_active()) {
    return;
  }
  ctx.surface->palette({0});
  ctx.surface->fill(0);
  auto items = context_.interaction->items();
  const auto& hot = context_.interaction->hot_item_name();
  const int end =
      std::min(item_offset_ + PAGE_SIZE, static_cast<int>(items.size()));
  for (int i = item_offset_; i < end; ++i) {
    const auto& item = items[static_cast<std::size_t>(i)];
    int color = 71;
    if (item.name == hot) {
      color = 55;
    }
    gfx::draw::box(*ctx.surface, item.rect, color);
    ViewPoint int_pos = item.rect.point() + PxOffset{2, 2};
    context_.font->render_text_onto(*ctx.surface, std::to_string(i), int_pos,
                                    FontFace::MINI, color);
    context_.font->render_text_onto(*ctx.surface, item.name,
                                    int_pos + PxOffset{0, 8}, FontFace::MINI,
                                    color);
  }
}

void DebugLayer::tick() {}

bool DebugLayer::on_text(const input::TextEvent& ev) { return false; }

bool DebugLayer::on_action(const input::ActionEvent& ev) {
  if (!ev.state.pressed) {
    return false;
  }
  if (ev.action == input::Action::ToggleDebug) {
    if (is_active()) {
      hide();
    }
    else {
      show();
      item_offset_ = 0;
    }
    return true;
  }
  if (!is_active()) {
    return false;
  }
  if (ev.action == input::Action::PageDown) {
    const int total = static_cast<int>(context_.interaction->items().size());
    item_offset_ = std::min(item_offset_ + PAGE_SIZE, std::max(0, total - 1));
    return true;
  }
  if (ev.action == input::Action::PageUp) {
    item_offset_ = std::max(0, item_offset_ - PAGE_SIZE);
    return true;
  }
  return false;
}
