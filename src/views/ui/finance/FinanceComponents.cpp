/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/finance/FinanceComponents.h"

#include <cstdlib>
#include "engine/FormatCompat.h"
#include "gfx/Draw.h"

namespace views::ui::finance {

void render_month_header(Surface& surface, FontManager& font,
                         const GameDate& date, const std::string& title,
                         const MonthOverviewConfig& config) {
  // Title section
  font.render_text_onto(surface, title, config.title_pos, FontFace::PIZ_NUMB,
                        config.title_color);

  // Current month
  font.render_text_onto(surface, std::string(date.previous_month_name()),
                        {config.title_pos.x + 178, config.title_pos.y + 4},
                        FontFace::PIZ_NUMB, config.title_color);

  // Previous month
  font.render_text_onto(surface, std::string(date.month_name()),
                        {config.title_pos.x + 280, config.title_pos.y + 4},
                        FontFace::PIZ_NUMB, config.title_color);
}

Surface render_invoice(FontManager& font, const InvoiceConfig& config) {
  constexpr int COLOR_BLACK = 8;
  Surface invoice{config.width, config.height,
                  PaletteHandle{config.palette_index}, config.outer_color};

  ViewRect outer_box{0, 0, config.width, config.height};
  ViewRect inner_box = outer_box.inset(2, 2);
  invoice.fill_rect(inner_box, config.inner_border_color);

  ViewRect very_inner_box = inner_box.inset(8, 8);
  invoice.fill_rect(very_inner_box, config.background_color);

  font.render_text_onto(invoice, config.title, config.title_pos,
                        config.title_font, COLOR_BLACK, AlignX::Center);

  if (!config.subtitle.empty()) {
    font.render_text_onto(invoice, config.subtitle, config.subtitle_pos,
                          config.subtitle_font, COLOR_BLACK, AlignX::Center);
  }

  font.render_text_onto(invoice, "@@@@@@@@@@@@@@@@@@@@@@@",
                        config.separator_pos, FontFace::CSTANDAR, COLOR_BLACK,
                        AlignX::Center);

  // Render line items
  int y = config.line_start_y;
  for (const auto& line : config.lines) {
    bool has_label = !line.label.empty();
    bool has_value = line.value.has_value();

    if (!has_label && !has_value) {
      y += config.line_spacing;
      continue;
    }

    const auto align = has_value ? AlignX::Left : AlignX::Center;
    const auto x_pos = has_value ? config.label_x : 0;
    if (has_label) {
      font.render_text_onto(invoice, line.label, ViewPoint{x_pos, y},
                            config.line_font, COLOR_BLACK, align);
    }

    if (has_value) {
      font.render_text_onto(invoice, compat::format("{}", *line.value),
                            ViewPoint{config.value_x, y}, config.line_font,
                            COLOR_BLACK, AlignX::RightEdge);
    }

    y += config.line_spacing;
  }

  if (!config.footer.empty()) {
    font.render_text_onto(invoice, config.footer, ViewPoint{0, 292},
                          FontFace::CSTANDAR, COLOR_BLACK, AlignX::Center);
  }

  return invoice;
}

} // namespace views::ui::finance
