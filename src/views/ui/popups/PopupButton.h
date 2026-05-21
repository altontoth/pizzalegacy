/*
 * SPDX-FileCopyrightText: 2019 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>

#include "engine/geom/Spaces.h"
#include "engine/ui/InteractionLayers.h"

class PopupButton {
public:
  using Callback = std::function<void()>;
  using Painter =
      std::function<void(Surface& target, ViewPoint top_left, bool pressed)>;

  PopupButton(ViewRect local_rect, std::string text, Callback on_click)
      : rect_(local_rect), text_(std::move(text)),
        on_click_(std::move(on_click)) {}

  void add_interactions(views::ui::InteractionContext ctx,
                        ViewPoint popup_origin);

  template <class BasePopupLike>
  void render_with(BasePopupLike& popup_helpers, Surface& target) const {
    auto s = popup_helpers.text_button(text_, rect_.w, rect_.h, pressed_);
    target.blit_raw(s, rect_.point());

    if (painter_) {
      painter_(target, rect_.point(), pressed_);
    }
  }

  void set_painter(Painter p) { painter_ = std::move(p); }
  void set_rect(ViewRect r) { rect_ = r; }

private:
  ViewRect rect_;
  std::string text_;
  Callback on_click_;
  Painter painter_;

  bool hovered_ = false;
  bool pressed_ = false;
};
