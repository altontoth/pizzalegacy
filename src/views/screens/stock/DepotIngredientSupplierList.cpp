/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/stock/DepotIngredientSupplierList.h"

#include "core/character/Actor.h"
#include "core/city/City.h"
#include "core/food/IngredientCategory.h"
#include "core/state/GameState.h"
#include "core/supply/FoodSupplier.h"
#include "engine/FormatCompat.h"
#include "gfx/Blit.h"
#include "resources/BlitmapManager.h"
#include "resources/FontManager.h"

#include <algorithm>

namespace {
ActorType category_to_actor_type(IngredientCategory category) {
  switch (category) {
  case IngredientCategory::VEGETABLES:
    return ActorType::VEGETABLE_SUPPLIER;
  case IngredientCategory::MEAT:
    return ActorType::MEAT_SUPPLIER;
  case IngredientCategory::FRUIT:
    return ActorType::FRUIT_SUPPLIER;
  case IngredientCategory::CHEESE:
    return ActorType::CHEESE_SUPPLIER;
  default:
    return ActorType::VEGETABLE_SUPPLIER;
  }
}
} // namespace

DepotIngredientSupplierList::DepotIngredientSupplierList(
    GameContext& context, GameState& state, IngredientCategory category,
    IngredientScreenMode mode)
    : MainPanelView(context, state), status_controller_(state.status()),
      category_(category), mode_(mode) {

  category_titles_[IngredientCategory::VEGETABLES] = "Vegetables";
  category_titles_[IngredientCategory::MEAT] = "Meat & Fish";
  category_titles_[IngredientCategory::FRUIT] = "Fruit";
  category_titles_[IngredientCategory::CHEESE] = "Cheese & Spices";

  const City* city = state.active_city();
  auto suppliers = city->food_suppliers(category_to_actor_type(category));
  std::ranges::sort(suppliers,
                    [](const FoodSupplier* a, const FoodSupplier* b) {
                      if (a->quality() != b->quality()) {
                        return a->quality() < b->quality();
                      }
                      return a->id() < b->id();
                    });

  renderer_.emplace(
      std::move(suppliers),
      [this](const FoodSupplier* supplier) {
        state_.ui().push<DepotIngredientOrderState>(supplier->id(), category_,
                                                    mode_);
      },
      [this](Surface& surface, const FoodSupplier* supplier, ViewPoint pos) {
        auto player_id = state_.active_player()->id();
        Money spent = supplier->relationship(player_id).total_spent;
        if (spent > 0_usd) {
          std::string spending_text = compat::format("SO FAR: {:w}", spent);
          const int center_x = surface.rect().center().x;
          context_.font->render_text_onto(
              surface, spending_text, ViewPoint{center_x + 16, pos.y + 38},
              FontFace::CSTANDAR, 12, TextAnchor::Middle);
        }
      });

  status_controller_.show_city_default(
      {.state = &state_, .city = state_.active_city()});
  set_up_input_panel();
}

void DepotIngredientSupplierList::draw_icon(const RenderContext& ctx) {
  auto& blitmap1 = context_.blitmap->get<Blitmap1Icon>();
  Blitmap1Icon icon = core::food::CategoryRegistry::icon(category_);
  blitmap1.render_icon(icon, *ctx.surface, ViewPoint{42, 6});
}

void DepotIngredientSupplierList::draw_page_header(const RenderContext& ctx) {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back9"));
  draw_icon(ctx);

  std::string title = category_titles_[category_] + " suppliers";
  const int center_x = ctx.surface->rect().center().x;
  context_.font->render_text_onto(*ctx.surface, title,
                                  ViewPoint{center_x + 40, 16},
                                  FontFace::PIZ_NUMB, 8, TextAnchor::Middle);
}

void DepotIngredientSupplierList::draw_page_screen(const RenderContext& ctx) {
  interactions_.begin_frame();

  renderer_->render_page(ctx, context_, interactions_, current_page_);
  renderer_->render_page_number(ctx, context_, current_page_);
}

void DepotIngredientSupplierList::render(const RenderContext& ctx) {
  draw_page_header(ctx);
  draw_page_screen(ctx);
  ctx.surface->palette({5});
}

void DepotIngredientSupplierList::set_up_input_panel() {
  renderer_->setup_pagination_buttons(
      input_panel_.get(), current_page_, [this] { mark_dirty(); },
      [this] {
        state_.advance_time_blocks(1);
        state_.ui().pop();
      });
}
