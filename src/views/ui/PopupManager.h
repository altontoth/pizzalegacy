/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <queue>
#include "core/util/GameContext.h"
#include "engine/DebugLabel.h"
#include "engine/DisplayConstants.h"
#include "engine/RenderContext.h"
#include "engine/Renderable.h"
#include "engine/geom/Spaces.h"
#include "engine/ui/Alignment.h"
#include "engine/ui/InteractionLayers.h"
#include "views/base/OverlayBehavior.h"
#include "views/ui/popups/BasePopup.h"

class PopupManager : public Renderable,
                     public OverlayBehavior<PopupManager>,
                     public Dirtyable,
                     public debug::AutoLabel<PopupManager> {
public:
  PopupManager(GameContext& gc);

  // Show a popup. If one is already active, queues it to appear after the
  // current one (and any others already queued) are dismissed.
  template <class Popup, class... Args> void create(Args&&... args) {
    static_assert(std::is_base_of_v<BasePopup, Popup>);
    if (active_popup_) {
      pending_creates_.push(
          [this, t = std::make_tuple(std::forward<Args>(args)...)]() mutable {
            std::apply(
                [this](auto&&... a) {
                  this->template do_create<Popup>(std::move(a)...);
                },
                std::move(t));
          });
      return;
    }
    do_create<Popup>(std::forward<Args>(args)...);
  }

  void origin(ViewPoint p);
  void tick() override;
  void render(const RenderContext& ctx) override;
  bool is_dirty() const override;
  bool needs_force_recompose() const override;
  void clean() override;
  void mark_dirty() override;
  bool freezes_view() const override;
  bool freezes_simulation() const override;
  ViewPoint origin() const;

  std::vector<LayerInfo> get_layers() override;

private:
  template <class Popup, class... Args> void do_create(Args&&... args) {
    active_popup_ =
        std::make_unique<Popup>(context_, std::forward<Args>(args)...);
    active_origin_ = pending_origin_.value_or(center_origin(*active_popup_));
    pending_origin_.reset();
  }

  static ViewPoint center_origin(const BasePopup& popup) {
    auto sz = popup.popup_size();
    if (sz.w == 0 || sz.h == 0) {
      return {0, 0};
    }
    constexpr ViewRect screen{0, 0, DisplayConstants::SCREEN_WIDTH,
                              DisplayConstants::SCREEN_HEIGHT};
    return views::ui::align_in_rect(screen, sz, AlignX::Center, AlignY::Middle);
  }

  const GameContext& context_;
  std::optional<ViewPoint> pending_origin_;
  std::unique_ptr<BasePopup> active_popup_;
  ViewPoint active_origin_;
  std::queue<std::function<void()>> pending_creates_;
  views::ui::InteractionLayers interactions_;
};
