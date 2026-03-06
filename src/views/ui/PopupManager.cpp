/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/PopupManager.h"
#include "engine/DisplayConstants.h"
#include "engine/ui/InteractionLayers.h"
#include "views/base/ViewDefs.h"

PopupManager::PopupManager(GameContext& gc)
    : context_(gc), interactions_(gc.interaction->make_layer(),
                                  gc.interaction->make_layer()) {}

void PopupManager::origin(ViewPoint p) { pending_origin_ = p; }

void PopupManager::tick() {
  // remove finished popups
  bool did_change = false;
  while (!popups_.empty() && popups_.back().popup->is_finished()) {
    popups_.pop_back();
    // compositor must re-blit now that it's gone
    request_recompose();
    did_change = true;
  }

  // toggle active state only when it really changes
  if (popups_.empty()) {
    if (is_active()) {
      hide();
      did_change = true;
    }
  }
  else {
    if (!is_active()) { // first popup just arrived
      show();
      did_change = true;
    }
  }

  // if any transition happened, mark underlying popup dirty
  if (did_change && !popups_.empty()) {
    popups_.back().popup->mark_dirty();
  }

  if (!popups_.empty()) {
    popups_.back().popup->tick();
  }
}

void PopupManager::render(const RenderContext& ctx) {
  interactions_.begin_frame();
  ctx.surface->palette(palette_);
  if (!popups_.empty()) {
    auto& top = popups_.back();

    // Create backdrop first
    interactions_.create_frame_item(
        ViewRect{0, 0, DisplayConstants::SCREEN_WIDTH,
                 DisplayConstants::SCREEN_HEIGHT},
        "popup_backdrop",
        [](const views::ui::InteractionEvent& ev) { /* No-op */ },
        {}, // no tooltip
        views::ui::InteractionLayer::Exclusive);

    views::ui::InteractionContext ui{
        .layers = interactions_, .invalidate = [this] { mark_self_dirty(); }};
    top.popup->add_interactions(ui, top.origin);

    top.popup->render(*ctx.surface, top.origin);
  }
}

bool PopupManager::is_dirty() const {
  if (!popups_.empty()) {
    return popups_.back().popup->is_dirty();
  }
  return Dirtyable::is_dirty();
}

bool PopupManager::needs_force_recompose() const {
  return pull_recompose_flag();
}

void PopupManager::clean() {
  if (!popups_.empty()) {
    popups_.back().popup->clean();
  }
  else {
    Dirtyable::clean();
  }
}

void PopupManager::mark_dirty() {
  if (!popups_.empty()) {
    popups_.back().popup->mark_dirty();
  }
  else {
    Dirtyable::mark_dirty();
  }
}

bool PopupManager::blocks() const { return !popups_.empty(); }

ViewPoint PopupManager::origin() const {
  return popups_.empty() ? ViewPoint{0, 0} : popups_.back().origin;
}

std::vector<LayerInfo> PopupManager::get_layers() {
  std::vector<LayerInfo> list;
  list.push_back(LayerInfo{
      .view = this,
      .area = {0, 0, DisplayConstants::SCREEN_WIDTH,
               DisplayConstants::SCREEN_HEIGHT},
      .label = "PopupManager",
  });
  return list;
}

void PopupManager::palette_handle(PaletteHandle p) { palette_ = p; }
