/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/widgets/RecipeFileInfoBox.h"

#include <filesystem>
#include <string>
#include "core/food/RecipeCodec.h"
#include "engine/FormatCompat.h"
#include "engine/geom/Spaces.h"
#include "engine/gfx/PaletteHandle.h"
#include "engine/gfx/Surface.h"
#include "resources/FontManager.h"
#include "resources/FontTypes.h"

namespace {
constexpr int info_box_w = 350;
constexpr int info_box_h = 150;
constexpr int margin = 6;
constexpr int paragraph_gap = 4;
constexpr int text_color = 240;
constexpr int label_color = 1;
} // namespace

void RecipeFileInfoBox::set_file(const std::string& filename) {
  filename_ = filename;
  summary_ =
      PizzaFactory::read_summary(io::recipe::recipe_path(filename).string());
}

void RecipeFileInfoBox::draw(Surface& screen, ViewPoint p,
                             bool /*is_selected*/) {
  auto block = Surface(info_box_w, info_box_h, PaletteHandle{38}, 15);
  block.fill(14);
  block.fill_rect(block.rect().inset(2, 2), 15);

  const int text_w = info_box_w - text_left_ - margin;
  ViewPoint text_pos{text_left_, margin};
  if (filename_.empty() || !summary_) {
    const char* msg =
        filename_.empty() ? "Select a recipe file" : "Not a recipe file";
    context_.font->render_line_onto(block, msg, text_pos, FontFace::SUPERSMA,
                                    label_color);
    screen.blit_raw(block, p);
    return;
  }

  const auto paragraph = [&](const std::string& text, uint8_t color) {
    const ViewRect rect{text_pos.x, text_pos.y, text_w,
                        info_box_h - text_pos.y - margin};
    text_pos.y += context_.font
                      ->render_into_rect(block, text, rect, FontFace::SUPERSMA,
                                         TextStyle{.color = color})
                      .height +
                  paragraph_gap;
  };

  const std::string stem = std::filesystem::path(filename_).stem().string();
  paragraph(compat::format("File: {}", stem), label_color);
  paragraph(compat::format("Recipe: {}", summary_->name), label_color);

  std::string ingredients;
  for (const auto& name : summary_->ingredients) {
    if (!ingredients.empty()) {
      ingredients += ", ";
    }
    ingredients += name;
  }
  paragraph(ingredients, text_color);

  screen.blit_raw(block, p);
}

Size RecipeFileInfoBox::measure() const { return Size{info_box_w, info_box_h}; }
