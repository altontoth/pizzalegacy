/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/widgets/StatBar.h"

#include "gfx/Surface.h"
#include "resources/BlitmapIcons.h"

#include <algorithm>

StatBar::StatBar(BlitmapManager* bm, FontManager* f)
    : blitmap3_(bm->get<Blitmap3Icon>()), font_(f), bar_width_([this] {
        auto r = blitmap3_.rect(Blitmap3Icon::BAR_EMPTY);
        return r.w;
      }()),
      bar_height_([this] {
        auto r = blitmap3_.rect(Blitmap3Icon::BAR_EMPTY);
        return r.h;
      }()),
      bar_empty_([this] {
        gfx::Surface s(bar_width_, bar_height_, blitmap3_.palette());
        blitmap3_.render_icon(Blitmap3Icon::BAR_EMPTY, s, {0, 0});
        return s;
      }()),
      bar_red_([this] {
        gfx::Surface s(bar_width_, bar_height_, blitmap3_.palette());
        blitmap3_.render_icon(Blitmap3Icon::BAR_RED, s, {0, 0});
        return s;
      }()),
      bar_yellow_([this] {
        gfx::Surface s(bar_width_, bar_height_, blitmap3_.palette());
        blitmap3_.render_icon(Blitmap3Icon::BAR_YELLOW, s, {0, 0});
        return s;
      }()),
      bar_green_([this] {
        gfx::Surface s(bar_width_, bar_height_, blitmap3_.palette());
        blitmap3_.render_icon(Blitmap3Icon::BAR_GREEN, s, {0, 0});
        return s;
      }()) {}

gfx::Surface StatBar::create_stat_bar(const std::string_view& label,
                                      int value) {
  constexpr int BUFFER_PIXELS = 4;
  constexpr int MAX_VALUE = 100;

  value = std::min(value, MAX_VALUE);

  gfx::Surface final_surf = bar_empty_.clone();

  // choose red/yellow/green bar and compute fill width
  const gfx::Surface& color_surf = pick_color_surface(value);
  int max_fill = bar_width_ - BUFFER_PIXELS * 2;
  int fill_width = (max_fill * value) / 100;
  fill_width = std::clamp(fill_width, 0, max_fill);

  if (fill_width > 0) {
    ViewRect src{BUFFER_PIXELS, 0, fill_width, bar_height_};
    final_surf.blit(color_surf, src, {BUFFER_PIXELS, 0});
  }

  // render the label and the numeric value
  const auto text_idx = 240;
  font_->render_text_onto(final_surf, label, {BUFFER_PIXELS * 3, 4},
                          FontFace::SUPERSMA, text_idx);
  font_->render_text_onto(final_surf, std::to_string(value),
                          {final_surf.width() - (BUFFER_PIXELS * 2), 4},
                          FontFace::SUPERSMA, text_idx, TextAnchor::End);

  // remove some artifact pixels by turning them transparent
  uint8_t c1 = final_surf.get({0, 0});
  uint8_t c2 = final_surf.get({2, 2});
  final_surf.replace_colors({c1, c2}, gfx::TRANSPARENT);

  return final_surf;
}

Surface& StatBar::pick_color_surface(int value) {
  if (value > 70) {
    return bar_green_;
  }
  if (value > 30) {
    return bar_yellow_;
  }
  return bar_red_;
}
