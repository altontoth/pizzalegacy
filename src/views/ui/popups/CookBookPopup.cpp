/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/popups/CookBookPopup.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "core/factories/PizzaFactory.h"
#include "core/food/Ingredient.h"
#include "core/food/Topping.h"
#include "core/food/ToppingSize.h"
#include "core/util/GameContext.h"
#include "engine/FormatCompat.h"
#include "engine/gfx/Draw.h"
#include "engine/input/InputManager.h"
#include "engine/ui/InteractionEvent.h"
#include "engine/ui/TextAnchor.h"
#include "resources/BobManager.h"
#include "resources/FontManager.h"
#include "resources/FontTypes.h"
#include "resources/VgaManager.h"
#include "views/screens/pizza/HalfSizePizza.h"

namespace {
constexpr uint8_t page_color = 143;
constexpr uint8_t edge_color = 4;
constexpr uint8_t title_color = 4;
constexpr uint8_t text_color = 240;
constexpr uint8_t arrow_color = 9;
constexpr int edge = 4;
constexpr ViewPoint title_at{24, 20};
// Right-aligned, so the names line up.
constexpr int weights_right = 92;
constexpr int names_x = 102;
constexpr int ingredients_y = 76;
constexpr int ingredient_spacing = 22;
constexpr std::array<ViewPoint, 3> picture_at{
    {{232, 172}, {346, 60}, {460, 172}}};
constexpr ViewRect back_rect{24, 312, 44, 30};
constexpr ViewRect forward_rect{152, 312, 44, 30};
constexpr ViewRect close_rect{480, 312, 88, 30};
constexpr ViewPoint page_number_at{
    (back_rect.x + back_rect.w + forward_rect.x) / 2, 320};
// Even, so the chevron's double pixels line up with the button's.
constexpr int arrow_half_width = 4;
constexpr int arrow_half_height = 8;
enum class Pointing { Left, Right };

void draw_chevron(Surface& target, ViewPoint top_left, Size button,
                  Pointing pointing, bool pressed) {
  const int direction = pointing == Pointing::Right ? 1 : -1;
  const int shift = pressed ? 2 : 0;
  const ViewPoint center{top_left.x + button.w / 2 + shift,
                         top_left.y + button.h / 2 + shift};
  const int tip = direction * arrow_half_width;
  // Two strokes side by side, for a bolder line.
  for (const int dx : {0, -2 * direction}) {
    const ViewPoint point{center.x + tip + dx, center.y};
    gfx::draw::line_2x(target,
                       {center.x - tip + dx, center.y - arrow_half_height},
                       point, arrow_color);
    gfx::draw::line_2x(target, point,
                       {center.x - tip + dx, center.y + arrow_half_height},
                       arrow_color);
  }
}
} // namespace

CookBookPopup::CookBookPopup(const GameContext& context,
                             core::food::StandardPizza pizza,
                             PageShown page_shown)
    : BasePopup(context), pizza_id_(pizza), page_shown_(std::move(page_shown)),
      renderer_("bob/zutaten.bob", context.bob),
      back_(back_rect, "", [this] { turn(-1); }),
      forward_(forward_rect, "", [this] { turn(1); }),
      close_(close_rect, "Close", [this] { close(); }) {
  back_.id("cook_book_back");
  forward_.id("cook_book_forward");
  back_.set_painter([](Surface& target, ViewPoint top_left, bool pressed) {
    draw_chevron(target, top_left, back_rect.size(), Pointing::Left, pressed);
  });
  forward_.set_painter([](Surface& target, ViewPoint top_left, bool pressed) {
    draw_chevron(target, top_left, forward_rect.size(), Pointing::Right,
                 pressed);
  });
  add_shortcut(input::Action::SubmitInput, &close_);
  turn(0);
}

std::optional<gfx::PaletteHandle> CookBookPopup::palette_override() const {
  return vga_->get("gfx/back1").palette();
}

void CookBookPopup::turn(int pages) {
  int index = core::food::to_index(pizza_id_) + pages;
  if (index < 0) {
    index = core::food::STANDARD_PIZZA_COUNT - 1;
  }
  if (index >= core::food::STANDARD_PIZZA_COUNT) {
    index = 0;
  }
  pizza_id_ = static_cast<core::food::StandardPizza>(index);
  pizza_ =
      PizzaFactory::read_pizza(pizza_id_, core::food::topping_size_fn(*bob_));
  if (page_shown_) {
    page_shown_(pizza_id_);
  }
  mark_dirty();
}

void CookBookPopup::close() {
  if (page_shown_) {
    page_shown_(std::nullopt);
  }
  finished();
}

void CookBookPopup::render(Surface& surface, ViewPoint origin) {
  Surface page(size.w, size.h, vga_->get("gfx/back1").palette());
  page.fill(page_color);
  gfx::draw::box(page, ViewRect{0, 0, size.w, size.h}, edge_color, edge);

  font_->render_text_onto(page, std::string{core::food::to_string(pizza_id_)},
                          title_at, FontFace::RUBY, title_color);
  if (pizza_) {
    int y = ingredients_y;
    for (const auto& grams : pizza_->ingredient_grams()) {
      font_->render_text_onto(page, compat::format("{} g", grams.grams),
                              ViewPoint{weights_right, y}, FontFace::PIZ_NUMB,
                              text_color, TextAnchor::End);
      font_->render_text_onto(page, grams.ingredient->name(),
                              ViewPoint{names_x, y}, FontFace::PIZ_NUMB,
                              text_color);
      y += ingredient_spacing;
    }

    const auto& stages = core::food::stages_for(pizza_id_);
    const std::array<std::size_t, 3> counts{static_cast<std::size_t>(stages[0]),
                                            static_cast<std::size_t>(stages[1]),
                                            pizza_->toppings().size()};
    for (std::size_t i = 0; i < counts.size(); ++i) {
      pizza_design::draw_half_size_pizza(page, picture_at[i], *pizza_,
                                         counts[i], *vga_, renderer_,
                                         page_color);
    }
  }

  back_.render_with(*this, page);
  forward_.render_with(*this, page);
  font_->render_text_onto(
      page,
      compat::format("{} / {}", core::food::to_index(pizza_id_) + 1,
                     core::food::STANDARD_PIZZA_COUNT),
      page_number_at, FontFace::PIZ_NUMB, text_color, TextAnchor::Middle);
  close_.render_with(*this, page);

  surface.blit_raw(page, origin);
}

void CookBookPopup::add_interactions(views::ui::InteractionContext ictx,
                                     ViewPoint origin) {
  back_.add_interactions(ictx, origin);
  forward_.add_interactions(ictx, origin);
  close_.add_interactions(ictx, origin);
}

bool CookBookPopup::on_action(const input::ActionEvent& ev) {
  if (BasePopup::on_action(ev)) {
    return true;
  }
  switch (ev.action) {
  case input::Action::MoveLeft:
  case input::Action::MoveRight:
    if (ev.state.pressed) {
      turn(ev.action == input::Action::MoveLeft ? -1 : 1);
    }
    return true;
  case input::Action::ScrollLeft:
  case input::Action::ScrollRight:
    return true;
  case input::Action::Exit:
    if (ev.state.pressed && !is_finished()) {
      close();
    }
    return true;
  default:
    return false;
  }
}
