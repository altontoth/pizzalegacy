/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/PopupManager.h"
#include "engine/DisplayConstants.h"
#include "engine/ui/ICursorManager.h"
#include "engine/ui/InteractionEvent.h"
#include "engine/ui/InteractionLayers.h"

PopupManager::PopupManager(GameContext& gc)
    : context_(gc),
      interactions_(gc.interaction->make_layer(), gc.interaction->make_layer(),
                    views::ui::priority_popup) {}

void PopupManager::origin(ViewPoint p) { pending_origin_ = p; }

void PopupManager::anchor(ViewPoint p) { pending_anchor_ = p; }

void PopupManager::tick() {
  bool did_change = false;

  if (active_popup_ && active_popup_->is_finished()) {
    active_popup_.reset();
    request_recompose();
    did_change = true;

    if (!pending_creates_.empty()) {
      auto fn = std::move(pending_creates_.front());
      pending_creates_.pop();
      fn();
    }
  }

  if (!active_popup_) {
    if (is_active()) {
      hide();
      context_.cursor->suppress_custom_cursor(false);
      did_change = true;
    }
  }
  else {
    if (!is_active()) {
      show();
      context_.cursor->suppress_custom_cursor(true);
      did_change = true;
    }
  }

  if (did_change && active_popup_) {
    active_popup_->mark_dirty();
  }

  if (active_popup_) {
    active_popup_->tick();
  }
}

void PopupManager::render(const RenderContext& ctx) {
  interactions_.begin_frame();
  if (active_popup_) {
    auto mode_palette = (context_.ui_mode == UiMode::Menu) ? PaletteHandle{38}
                                                           : PaletteHandle{0};
    ctx.surface->palette(
        active_popup_->palette_override().value_or(mode_palette));

    // Create backdrop first
    interactions_.create_frame_item(
        ViewRect{0, 0, DisplayConstants::SCREEN_WIDTH,
                 DisplayConstants::SCREEN_HEIGHT},
        "popup_backdrop",
        [](const views::ui::InteractionEvent& ev) { /* No-op */ },
        {.layer = views::ui::InteractionLayer::Exclusive});
    // Before the popup's own items, so they sit on top of it.
    interactions_.create_frame_item(
        ViewRect{active_origin_, active_popup_->popup_size()}, "popup_frame",
        [this](const views::ui::InteractionEvent& ev) { on_frame_event(ev); },
        {.drag_button = views::ui::MouseButton::Left});

    views::ui::InteractionContext ui{
        .layers = interactions_, .invalidate = [this] { mark_self_dirty(); }};
    active_popup_->render(*ctx.surface, active_origin_);

    active_popup_->add_interactions(ui, active_origin_);
  }
}

void PopupManager::on_frame_event(const views::ui::InteractionEvent& ev) {
  if (!active_popup_) {
    return;
  }
  const auto to = drag_.on_interaction(
      ev, ViewRect{active_origin_, active_popup_->popup_size()});
  if (!to) {
    return;
  }
  active_origin_ = *to;
  request_recompose();
  mark_dirty();
}

bool PopupManager::is_dirty() const {
  if (active_popup_) {
    return active_popup_->is_dirty();
  }
  return Dirtyable::is_dirty();
}

bool PopupManager::needs_force_recompose() const {
  return pull_recompose_flag();
}

void PopupManager::clean() {
  if (active_popup_) {
    active_popup_->clean();
  }
  else {
    Dirtyable::clean();
  }
}

void PopupManager::mark_dirty() {
  if (active_popup_) {
    active_popup_->mark_dirty();
  }
  else {
    Dirtyable::mark_dirty();
  }
}

bool PopupManager::freezes_view() const { return active_popup_ != nullptr; }

bool PopupManager::freezes_simulation() const {
  return active_popup_ != nullptr;
}

ViewPoint PopupManager::origin() const {
  return active_popup_ ? active_origin_ : ViewPoint{0, 0};
}

std::optional<ViewRect> PopupManager::active_rect() const {
  if (!active_popup_) {
    return std::nullopt;
  }
  return ViewRect{active_origin_, active_popup_->popup_size()};
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
