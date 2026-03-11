/*
 * SPDX-FileCopyrightText: 2014 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/DebugLabel.h"
#include "engine/DisplayConstants.h"
#include "engine/ui/TooltipManager.h"
#include "gfx/Draw.h"
#include "resources/FontManager.h"
#include "views/base/OverlayBehavior.h"
#include "views/base/View.h"
#include "views/base/ViewDefs.h"

class TooltipLayer : public View,
                     public OverlayBehavior<TooltipLayer>,
                     public debug::AutoLabel<TooltipLayer> {
public:
  TooltipLayer(GameContext& context, GameState& state, TooltipManager& mgr)
      : View(context, state), mgr_(mgr) {}

  void render(const RenderContext& ctx) override {
    ctx.surface->fill(0);
    if (!mgr_.visible()) {
      return;
    }

    auto const& req = mgr_.current();
    ctx.surface->palette(PaletteHandle{req.palette});
    std::string txt = req.text;

    // measure text first so Mouse anchor can position above cursor
    auto text_size = context_.font->size_text(txt, req.font);
    int w = text_size.width + (2 * req.padding);
    int h = text_size.height + (2 * req.padding);

    // Compute box top-left and text center separately per anchor.
    // For InputPanel the target x IS the fixed center column; the box is
    // placed symmetrically around it so the text is always at the same x
    // regardless of string length.
    // For Mouse the target is the box top-left; the text center is derived
    // from the box.
    ViewPoint box;
    ViewPoint text_center;
    switch (req.anchor) {
    case TooltipAnchor::InputPanel:
      box = {views::base::INPUT_PANEL_OFFSET_X +
                 (views::base::INPUT_PANEL_WIDTH / 2) - (w / 2),
             222};
      text_center = {views::base::INPUT_PANEL_OFFSET_X +
                         (views::base::INPUT_PANEL_WIDTH / 2),
                     box.y + req.padding};
      break;
    case TooltipAnchor::Mouse: {
      // position just above the cursor; screenPos kept current by
      // InteractionManager::handle_tooltip on every Hover event
      int bx = req.screenPos.x;
      if (bx + w > DisplayConstants::SCREEN_WIDTH) {
        bx = DisplayConstants::SCREEN_WIDTH - w;
      }
      box = {bx, req.screenPos.y - h - 2};
      text_center = {box.x + (w / 2), box.y + req.padding};
      break;
    }
    }

    ctx.surface->fill_rect({box.x, box.y, w, h}, req.bg_color);

    if (req.border_color != 0) {
      gfx::draw::box(*ctx.surface, {box.x, box.y, w, h}, req.border_color);
    }

    // draw line (no wrapping)
    context_.font->render_line_onto(*ctx.surface, txt, text_center,
                                    FontFace::CSTANDAR, req.text_color,
                                    AlignX::CenterPoint);
  }
  void tick() override {
    auto rev = mgr_.revision();
    if (rev != seen_revision_) {
      seen_revision_ = rev;
      request_recompose();
      mark_dirty();
    }
  };
  bool needs_force_recompose() const override { return pull_recompose_flag(); }

private:
  TooltipManager& mgr_;
  uint64_t seen_revision_{0};
};
