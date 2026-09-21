/*
 * SPDX-FileCopyrightText: 2014 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/DebugLabel.h"
#include "engine/DisplayConstants.h"
#include "engine/gfx/Draw.h"
#include "engine/ui/TooltipManager.h"
#include "resources/FontManager.h"
#include "views/base/OverlayBehavior.h"
#include "views/base/View.h"

class TooltipLayer : public View,
                     public OverlayBehavior<TooltipLayer>,
                     public debug::AutoLabel<TooltipLayer> {
public:
  TooltipLayer(GameContext& context, GameState& state, TooltipManager& mgr)
      : View(context, state), mgr_(mgr) {}

  void render(const RenderContext& ctx) override {
    rendered_revision_ = mgr_.revision();
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

    ViewPoint box;
    ViewPoint text_center;
    switch (req.anchor) {
    case TooltipAnchor::Fixed:
      box = {req.pos.x - (w / 2), req.pos.y - req.padding};
      text_center = req.pos;
      break;
    case TooltipAnchor::Mouse: {
      // position just above the cursor, or below it at the top of the
      // screen
      int bx = req.pos.x;
      if (bx + w > DisplayConstants::SCREEN_WIDTH) {
        bx = DisplayConstants::SCREEN_WIDTH - w;
      }
      int by = req.pos.y - h - 2;
      if (by < 0) {
        by = req.pos.y + 2;
      }
      box = {bx, by};
      text_center = {box.x + (w / 2), box.y + req.padding};
      break;
    }
    }

    ctx.surface->fill_rect({box.x, box.y, w, h}, req.bg_color);

    if (req.border_color != 0) {
      gfx::draw::box(*ctx.surface, {box.x, box.y, w, h}, req.border_color);
    }

    context_.font->render_line_onto(*ctx.surface, txt, text_center, req.font,
                                    req.text_color, TextAnchor::Middle);
  }
  void tick() override {}
  bool is_dirty() const override { return View::is_dirty() || changed(); }
  bool needs_force_recompose() const override {
    return pull_recompose_flag() || changed();
  }

private:
  bool changed() const { return mgr_.revision() != rendered_revision_; }

  TooltipManager& mgr_;
  uint64_t rendered_revision_{0};
};
