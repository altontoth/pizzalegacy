/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>
#include <vector>
#include "core/util/GameContext.h"
#include "engine/DebugLabel.h"
#include "engine/RenderContext.h"
#include "engine/Renderable.h"
#include "engine/geom/Spaces.h"
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
    popups_.emplace_back(PopupEntry{
        std::make_unique<Popup>(context_, std::forward<Args>(args)...),
        pending_origin_,
    });
    pending_origin_ = ViewPoint{0, 0};
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

  const GameContext& context_;
  ViewPoint pending_origin_;
  std::vector<PopupEntry> popups_;
  PaletteHandle palette_{0};
  views::ui::InteractionLayers interactions_;
};
