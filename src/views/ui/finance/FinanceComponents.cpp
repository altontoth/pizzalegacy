/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/finance/FinanceComponents.h"

#include <cstdlib>
#include "engine/FormatCompat.h"
#include "engine/gfx/Draw.h"

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

  const int center_x = invoice.rect().center().x;

  font.render_text_onto(invoice, config.title,
                        {center_x + config.title_pos.x, config.title_pos.y},
                        config.title_font, COLOR_BLACK, TextAnchor::Middle);

  if (!config.subtitle.empty()) {
    font.render_text_onto(
        invoice, config.subtitle,
        {center_x + config.subtitle_pos.x, config.subtitle_pos.y},
        config.subtitle_font, COLOR_BLACK, TextAnchor::Middle);
  }

  font.render_text_onto(
      invoice, "@@@@@@@@@@@@@@@@@@@@@@@",
      {center_x + config.separator_pos.x, config.separator_pos.y},
      FontFace::CSTANDAR, COLOR_BLACK, TextAnchor::Middle);

  // A label with a value is always left aligned, without a value it can
  // be right or center aligned.
  struct LabelAnchor {
    int x;
    TextAnchor anchor;
  };
  auto label_anchor = [&](const InvoiceLine& line) -> LabelAnchor {
    if (line.value.has_value()) {
      return {.x = config.label_x, .anchor = TextAnchor::Start};
    }
    switch (line.align) {
    case TextAnchor::Middle:
      return {.x = center_x, .anchor = TextAnchor::Middle};
    case TextAnchor::End:
      return {.x = config.value_x, .anchor = TextAnchor::End};
    case TextAnchor::Start:
      break;
    }
    return {.x = config.label_x, .anchor = TextAnchor::Start};
  };

  // Render line items
  int y = config.line_start_y;
  for (const auto& line : config.lines) {
    bool has_label = !line.label.empty();
    bool has_value = line.value.has_value();

    if (!has_label && !has_value) {
      y += config.line_spacing;
      continue;
    }

    if (has_label) {
      const auto label = label_anchor(line);
      font.render_text_onto(invoice, line.label, ViewPoint{label.x, y},
                            config.line_font, COLOR_BLACK, label.anchor);
    }

    if (has_value) {
      font.render_text_onto(invoice, (config.show_cents ? compat::format("{:s}", *line.value)
                                               : compat::format("{}", *line.value)),
                            ViewPoint{config.value_x, y}, config.line_font,
                            COLOR_BLACK, TextAnchor::End);
    }

    y += config.line_spacing;
  }

  if (!config.footer.empty()) {
    font.render_text_onto(invoice, config.footer, ViewPoint{center_x, 292},
                          FontFace::CSTANDAR, COLOR_BLACK, TextAnchor::Middle);
  }

  return invoice;
}

} // namespace views::ui::finance
