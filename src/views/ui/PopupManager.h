/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>
#include <optional>
#include <vector>
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

  template <class Popup, class... Args> void create(Args&&... args) {
    static_assert(std::is_base_of_v<BasePopup, Popup>);
    auto popup = std::make_unique<Popup>(context_, std::forward<Args>(args)...);
    ViewPoint org = pending_origin_.value_or(center_origin(*popup));
    popups_.emplace_back(PopupEntry{std::move(popup), org});
    pending_origin_.reset();
  }

  void origin(ViewPoint p);
  void tick() override;
  void render(const RenderContext& ctx) override;
  bool is_dirty() const override;
  bool needs_force_recompose() const override;
  void clean() override;
  void mark_dirty() override;
  bool blocks() const override;
  ViewPoint origin() const;

  std::vector<LayerInfo> get_layers() override;
  void palette_handle(PaletteHandle p);

private:
  struct PopupEntry {
    std::unique_ptr<BasePopup> popup;
    ViewPoint origin;
  };

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
  std::vector<PopupEntry> popups_;
  PaletteHandle palette_{0};
  views::ui::InteractionLayers interactions_;
};
