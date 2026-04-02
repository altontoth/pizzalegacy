/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/popups/OptionsPopup.h"

namespace {
constexpr int popup_width = 374;
constexpr int popup_border = 10;
constexpr int vert_padding = 24;
constexpr int btn_left = 18;
constexpr int btn_size = 32;
constexpr int text_gap = 6;
constexpr int row_height = btn_size + 40;
constexpr int text_color = 10;

constexpr int text_x = btn_left + btn_size + text_gap;
constexpr int btn_y_base = popup_border + vert_padding;
} // namespace

OptionsPopup::OptionsPopup(const GameContext& context,
                           std::vector<Option> options)
    : BasePopup(context), options_(std::move(options)) {
  const int n = static_cast<int>(options_.size());
  popup_height_ = (2 * (popup_border + vert_padding)) + (n * row_height) -
                  (row_height - btn_size);

  for (int i = 0; i < n; ++i) {
    ViewRect rect{btn_left, btn_y_base + (i * row_height), btn_size, btn_size};
    buttons_.emplace_back(rect, "", [this, i]() {
      options_[i].callback();
      finished();
    });
  }
}

void OptionsPopup::add_interactions(views::ui::InteractionContext ictx,
                                    ViewPoint origin) {
  for (auto& btn : buttons_) {
    btn.add_interactions(ictx, origin);
  }
}

void OptionsPopup::render(Surface& surface, ViewPoint origin) {
  auto s = popup_surface(popup_width_, popup_height_);

  for (int i = 0; i < static_cast<int>(options_.size()); ++i) {
    buttons_[i].render_with(*this, s);

    ViewRect text_rect{text_x, btn_y_base + (i * row_height),
                       popup_width - text_x - popup_border, btn_size};
    font_->render_into_rect(s, options_[i].text, text_rect, FontFace::ASTANDAR,
                            TextStyle{.color = text_color});
  }

  surface.blit_raw(s, origin);
}
