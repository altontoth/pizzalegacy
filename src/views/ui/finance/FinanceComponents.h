/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <optional>
#include <vector>

#include "core/finance/Money.h"
#include "core/time/GameDate.h"
#include "engine/gfx/Surface.h"
#include "resources/Blitmap.h"
#include "resources/Bob.h"
#include "resources/FontManager.h"

using gfx::Surface;

namespace views::ui::finance {

// Financial report elements
struct MonthOverviewConfig {
  ViewPoint title_pos = {14, 44};
  uint8_t title_color = 11;
  uint8_t line_color = 13;
  uint8_t text_color = 0;
};

void render_month_header(Surface& surface, FontManager& font,
                         const GameDate& date, const std::string& title,
                         const MonthOverviewConfig& config = {});

struct InvoiceLine {
  std::string label;
  std::optional<Money> value;
  // only used when value is nullopt
  TextAnchor align = TextAnchor::Middle;
};

struct InvoiceConfig {
  std::string title = "Invoice";
  std::string subtitle;
  ViewPoint title_pos = {0, 20};
  ViewPoint subtitle_pos = {0, 44};
  FontFace title_font = FontFace::ASTANDAR;
  FontFace subtitle_font = FontFace::CSTANDAR;
  uint8_t outer_color = 14;
  uint8_t inner_border_color = 15;
  uint8_t background_color = 14;
  uint8_t palette_index = 60;
  ViewPoint separator_pos = {0, 56}; // position for '@@@'
  int width = 290;
  int height = 322;

  std::vector<InvoiceLine> lines;
  std::string footer;
  FontFace line_font = FontFace::CSTANDAR;
  int line_start_y = 84;
  int line_spacing = 14;
  int label_x = 16;
  int value_x = 276;
  std::function<void()> on_dismiss;
};

Surface render_invoice(FontManager& font, const InvoiceConfig& config);

} // namespace views::ui::finance
