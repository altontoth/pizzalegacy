/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/stock/IngredientOrderScreen.h"

#include "core/character/Player.h"
#include "core/city/City.h"
#include "core/finance/Ledger.h"
#include "core/food/Ingredient.h"
#include "core/food/IngredientCategory.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "core/supply/FoodSupplier.h"
#include "engine/FormatCompat.h"
#include "engine/Logger.h"
#include "engine/geom/Spaces.h"
#include "engine/ui/InteractionEvent.h"
#include "factories/IngredientFactory.h"
#include "gfx/Blit.h"
#include "gfx/Draw.h"
#include "resources/BlitmapManager.h"
#include "resources/FontManager.h"

#include "engine/input/Action.h"
#include "engine/input/InputManager.h"
#include "engine/input/Key.h"

#include <algorithm>
#include <array>

IngredientOrderScreen::IngredientOrderScreen(
    GameContext& context, GameState& state, uint8_t supplier_id,
    IngredientCategory category,
    std::optional<std::vector<IngredientBatch>> saved_ingredients)
    : StockMain(context, state), category_(category) {

  auto* city = state_.active_city();
  ActorType supplier_type =
      core::food::CategoryRegistry::supplier_type(category);

  for (auto* s : city->food_suppliers(supplier_type)) {
    if (s->id() == supplier_id) {
      supplier_ = s;
      break;
    }
  }

  // Build ingredient list
  auto* restaurant = state_.active_restaurant();
  const auto& inventory = restaurant->inventory();

  const core::food::IngredientDemand& demand_grams =
      restaurant->daily_ingredient_demand_cache();

  auto [start_id, end_id] = core::food::CategoryRegistry::id_range(category);
  for (int ing_id = start_id; ing_id < end_id; ing_id++) {
    auto* ingredient = IngredientFactory::create_ingredient(ing_id);
    if (ingredient) {
      IngredientBatch batch;
      batch.ingredient = ingredient;
      batch.ingredient_type = ingredient->type();
      batch.count = 0;
      batch.price_per_unit = calculate_ingredient_price(ingredient->type());
      batch.quality = supplier_ ? supplier_->quality() : 100;

      const auto* stock = inventory.get_ingredient_stock(ingredient->type());
      batch.current_stock = stock ? stock->quantity / 1000 : 0;

      batch.daily_usage = demand_grams[ing_id];
      batch.need = (demand_grams[ing_id] > 0) ? 1 : 0;

      ingredients_.push_back(batch);
    }
  }

  if (saved_ingredients && context_.config->options().qol_improvements) {
    for (auto& batch : ingredients_) {
      for (const auto& saved : *saved_ingredients) {
        if (saved.ingredient_type == batch.ingredient_type) {
          batch.count = saved.count;
          break;
        }
      }
    }
  }

  status_controller_.show_restaurant(
      {.state = &state_, .rest = state_.active_restaurant(), .label = "ORDER"});
  set_up_input_panel();
}

void IngredientOrderScreen::tick() { update_held_button_repeat(); }

void IngredientOrderScreen::draw_icon(const RenderContext& ctx) {
  auto& blitmap1 = context_.blitmap->get<Blitmap1Icon>();
  Blitmap1Icon icon = core::food::CategoryRegistry::icon(category_);
  blitmap1.render_icon(icon, *ctx.surface, ViewPoint{42, 6});
}

void IngredientOrderScreen::draw_page_header(const RenderContext& ctx) {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back9"));

  draw_icon(ctx);

  constexpr uint8_t header_color = 8;
  std::string title = supplier_ ? supplier_->name() : "SUPPLIER";
  context_.font->render_text_onto(
      *ctx.surface, title, ViewPoint{(ctx.surface->width() / 2) + 40, 16},
      FontFace::PIZ_NUMB, header_color, TextAnchor::Middle);
}

void IngredientOrderScreen::register_ingredient_row(ViewRect row_rect,
                                                    size_t i) {
  interactions_.create_frame_item(
      row_rect, compat::format("ingredient_{}", i),
      [this, i](views::ui::InteractionEvent ie) {
        switch (ie.type) {
        case views::ui::InteractionEvent::Type::Enter:
          hovered_index_ = i;
          mark_dirty();
          break;
        case views::ui::InteractionEvent::Type::Leave:
          if (hovered_index_ == i) {
            hovered_index_.reset();
            mark_dirty();
          }
          break;
        case views::ui::InteractionEvent::Type::ButtonDown: {
          bool is_left = ie.button == views::ui::MouseButton::Left;
          bool is_right = ie.button == views::ui::MouseButton::Right;
          if (is_left || is_right) {
            handle_button_press(i, is_left);
          }
          break;
        }
        case views::ui::InteractionEvent::Type::ButtonUp: {
          bool is_left = ie.button == views::ui::MouseButton::Left;
          bool is_right = ie.button == views::ui::MouseButton::Right;
          if (is_left || is_right) {
            handle_button_release(is_left);
          }
          break;
        }
        case views::ui::InteractionEvent::Type::Scroll:
          handle_scroll(i, ie.scroll_delta);
          break;
        default:
          break;
        }
      });
}

void IngredientOrderScreen::draw_ingredient_list(const RenderContext& ctx) {
  interactions_.begin_frame();

  const int header_y = 34;
  const uint8_t text_color = 8;
  const uint8_t text_color_days = 12;

  const int name_x = 22;
  const int right_edge = ctx.surface->width();
  const int header_price_x = right_edge - 178;
  const int price_x = right_edge - 172;
  const int header_amount_x = right_edge - 110;
  const int amount_x = right_edge - 128;
  const int header_stock_x = right_edge - 66;
  const int stock_x = right_edge - 72;
  const int header_days_x = right_edge - 26;
  const int days_x = right_edge - 32;

  context_.font->render_text_onto(
      *ctx.surface, "PRICE", ViewPoint{header_price_x, header_y},
      FontFace::SUPERSMA, text_color, TextAnchor::End);
  context_.font->render_text_onto(
      *ctx.surface, "AMOUNT", ViewPoint{header_amount_x, header_y},
      FontFace::SUPERSMA, text_color, TextAnchor::End);
  context_.font->render_text_onto(
      *ctx.surface, "STOCK", ViewPoint{header_stock_x, header_y},
      FontFace::SUPERSMA, text_color, TextAnchor::End);
  context_.font->render_text_onto(
      *ctx.surface, "DAYS", ViewPoint{header_days_x, header_y},
      FontFace::SUPERSMA, text_color, TextAnchor::End);

  constexpr int line_color = 15;
  constexpr int line_width = 2;
  // Between price and amount
  ViewPoint line_top{172 + 42, 48};
  ViewPoint line_bottom{172 + 42, 318};
  gfx::draw::line(*ctx.surface, line_top, line_bottom, line_color, line_width);
  // Between amount and stock
  line_top.x += 48;
  line_bottom.x += 48;
  gfx::draw::line(*ctx.surface, line_top, line_bottom, line_color, line_width);
  // Under the last ingredient above the total:
  gfx::draw::line(*ctx.surface, {176, 318}, {320, 318}, 13, line_width);

  // Render ingredients
  int y = header_y + 20;
  const int line_height = 12;
  const int row_width = 300;

  for (size_t i = 0; i < ingredients_.size(); i++) {
    auto& batch = ingredients_[i];

    bool supplier_sells = batch.price_per_unit.cents > 0;

    // Draw gray background on hover or when holding button (only if supplier
    // sells this ingredient)
    if (button_held_index_ ? button_held_index_ == i : hovered_index_ == i) {
      ctx.surface->fill_rect(
          ViewRect{name_x, y - 2, row_width, line_height + 2}, 165);
    }

    // Create interactive item for the entire row
    if (supplier_sells) {
      register_ingredient_row({name_x, y - 2, row_width, line_height}, i);
    }

    // Render ingredient name
    context_.font->render_text_onto(*ctx.surface, batch.ingredient->name(),
                                    ViewPoint{name_x, y}, FontFace::CSTANDAR,
                                    text_color);

    // Render price (or dashes if supplier doesn't sell this ingredient)
    if (supplier_sells) {
      std::string price_text = compat::format("{:n}", batch.price_per_unit);
      context_.font->render_text_onto(*ctx.surface, price_text,
                                      ViewPoint{price_x, y}, FontFace::CSTANDAR,
                                      text_color, TextAnchor::End);
    }
    else {
      context_.font->render_text_onto(*ctx.surface, "------",
                                      ViewPoint{price_x, y}, FontFace::CSTANDAR,
                                      text_color, TextAnchor::End);
    }

    // Only show amount, stock, and days for ingredients the supplier sells
    if (supplier_sells) {
      context_.font->render_text_onto(
          *ctx.surface, std::to_string(batch.count), ViewPoint{amount_x, y},
          FontFace::CSTANDAR, text_color, TextAnchor::End);

      int projected_stock = batch.current_stock + batch.count;
      context_.font->render_text_onto(
          *ctx.surface, std::to_string(projected_stock), ViewPoint{stock_x, y},
          FontFace::CSTANDAR, text_color, TextAnchor::End);

      // Render days remaining (show if ingredient has usage on menu)
      // Calculate days based on projected stock (current + order amount)
      if (batch.daily_usage > 0) {
        // usage is in grams per day; enforce minimum of 1kg/day
        int effective_usage = std::max(batch.daily_usage, 1000);
        int days_remaining = (projected_stock * 1000) / effective_usage;

        std::string days_text = std::to_string(days_remaining);
        context_.font->render_text_onto(
            *ctx.surface, days_text, ViewPoint{days_x, y}, FontFace::CSTANDAR,
            text_color_days, TextAnchor::End);
      }
    }

    y += line_height;
  }

  // Footer section
  const int footer_y = 320;
  const uint8_t red_color = 20;

  // Show supplier quality
  if (supplier_) {
    std::string quality_text =
        compat::format("QUALITY: {}", supplier_->quality());
    context_.font->render_text_onto(*ctx.surface, quality_text,
                                    ViewPoint{name_x + 26, footer_y},
                                    FontFace::SUPERSMA, text_color);
  }

  Money total_cost = calculate_total_cost();

  std::string total_text = compat::format("{:n}", total_cost);
  context_.font->render_text_onto(
      *ctx.surface, "TOTAL:", ViewPoint{header_price_x, footer_y},
      FontFace::SUPERSMA, text_color, TextAnchor::End);
  context_.font->render_text_onto(
      *ctx.surface, total_text, ViewPoint{stock_x, footer_y},
      FontFace::SUPERSMA, red_color, TextAnchor::End);
}

void IngredientOrderScreen::draw_page_screen(const RenderContext& ctx) {
  draw_ingredient_list(ctx);
}

void IngredientOrderScreen::render(const RenderContext& ctx) {
  draw_page_header(ctx);
  draw_page_screen(ctx);
}

void IngredientOrderScreen::place_order() {
  Money total_cost = calculate_total_cost();
  int total_items = 0;
  for (const auto& batch : ingredients_) {
    total_items += batch.count;
  }

  Logger::info("[Order] Total: {} items for {}", total_items, total_cost);

  if (total_cost.cents > 0 && !state_.spend_cash(Account::Stock, total_cost)) {
    return;
  }

  auto* restaurant = state_.active_restaurant();
  for (const auto& batch : ingredients_) {
    if (batch.count > 0) {
      restaurant->purchase_ingredients(batch.ingredient_type, batch.count,
                                       batch.quality, batch.price_per_unit);
      if (supplier_) {
        int remaining =
            std::max(0, supplier_->stock(batch.ingredient_type) - batch.count);
        supplier_->set_stock(batch.ingredient_type, remaining);
      }

      Logger::info("[Order] {} x{} @ {} each (quality {})",
                   batch.ingredient->name(), batch.count, batch.price_per_unit,
                   batch.quality);
    }
  }

  // Record vendor spending for this week and update supplier relationship
  if (supplier_ && total_cost.cents > 0) {
    auto* city = state_.active_city();
    auto player_id = state_.active_player()->id();
    city->record_vendor_purchase(player_id, supplier_->id(), total_cost);

    auto& rel = supplier_->relationship(player_id);
    rel.total_spent += total_cost;
  }

  if (total_items > 0) {
    if (auto* player = state_.active_player()) {
      player->on_food_shop_visit();
    }
  }

  // Return to supplier list
  state_.ui().pop();
}

Money IngredientOrderScreen::calculate_ingredient_price(
    core::food::IngredientType ingredient_type) const {
  if (!supplier_) {
    return Money{0};
  }

  static constexpr std::array<float, 5> difficulty_multipliers = {
      0.6f, 0.8f, 1.0f, 1.5f, 1.9f};

  auto* city = state_.active_city();
  Money base_price = city->ingredient_base_price(ingredient_type);

  int difficulty = state_.difficulty_level();

  return supplier_->calculate_price(ingredient_type, base_price,
                                    difficulty_multipliers[difficulty - 1]);
}

Money IngredientOrderScreen::calculate_total_cost() const {
  Money total{0};
  for (const auto& batch : ingredients_) {
    if (batch.count > 0) {
      total += Money{batch.price_per_unit.cents * batch.count};
    }
  }
  return total;
}

bool IngredientOrderScreen::on_action(const input::ActionEvent& ev) {
  if (ev.action == input::Action::NextRestaurant ||
      ev.action == input::Action::PreviousRestaurant) {
    return true;
  }
  return RestaurantSubView::on_action(ev);
}

void IngredientOrderScreen::set_up_input_panel() {
  input_panel_->delete_buttons();

  input_panel_->add_button(Icon::THUMBS_UP, ButtonType::Horizontal,
                           ButtonPosition::TopLeft, "Single Order",
                           [this] { place_order(); });

  input_panel_->add_button(
      Icon::SHAKING_HANDS, ButtonType::Horizontal, ButtonPosition::MiddleLeft,
      "Make Contract", [this] {
        Money total{0};
        for (const auto& batch : ingredients_) {
          if (batch.count > 0) {
            total += Money{batch.price_per_unit.cents * batch.count};
          }
        }
        if (total.cents == 0) {
          return;
        }

        // Persist current counts so they survive the round-trip through
        // the conversation and can be restored if no contract is made.
        if (auto* ios = state_.ui().top_as<IngredientOrderState>()) {
          ios->saved_ingredients = ingredients_;
        }

        state_.ui().push<SupplierContractState>(supplier_->id(), category_,
                                                ingredients_, total);
      });

  input_panel_->add_button(Icon::ARROW_BACK, ButtonType::Square,
                           ButtonPosition::BottomRight, "Back", [this] {
                             state_.advance_time_blocks(2);
                             state_.ui().pop();
                           });
}

void IngredientOrderScreen::handle_button_press(size_t ingredient_index,
                                                bool is_left_button) {
  auto& batch = ingredients_.at(ingredient_index);
  auto& c = batch.count;
  bool shift_held = context_.input->modifier_down(input::Mod::Shift);
  int step = shift_held ? FAST_STEP : NORMAL_STEP;

  // Immediate increment/decrement
  if (is_left_button) {
    int headroom = std::max(0, max_ingredient_stock_kg - batch.current_stock);
    c = std::min(c + step, headroom);
    left_button_held_ = true;
  }
  else {
    c = std::max(c - step, 0);
    right_button_held_ = true;
  }

  // If this is the first button pressed, set direction and index
  if (!button_held_index_) {
    button_held_index_ = ingredient_index;
    hold_direction_ =
        is_left_button ? HoldDirection::Increment : HoldDirection::Decrement;
    button_held_frames_ = 0;
  }

  mark_dirty();
}

void IngredientOrderScreen::handle_button_release(bool is_left_button) {
  if (is_left_button) {
    left_button_held_ = false;
  }
  else {
    right_button_held_ = false;
  }

  if (!left_button_held_ && !right_button_held_) {
    button_held_index_.reset();
    button_held_frames_ = 0;
  }
}

void IngredientOrderScreen::handle_scroll(size_t ingredient_index, int delta) {
  auto& batch = ingredients_.at(ingredient_index);
  auto& c = batch.count;
  int step = std::max(1, std::abs(delta));

  if (delta > 0) {
    int headroom = std::max(0, max_ingredient_stock_kg - batch.current_stock);
    c = std::min(c + step, headroom);
  }
  else if (delta < 0) {
    c = std::max(c - step, 0);
  }

  mark_dirty();
}

void IngredientOrderScreen::update_held_button_repeat() {
  if (!button_held_index_ || (!left_button_held_ && !right_button_held_)) {
    return;
  }

  button_held_frames_++;

  bool shift_held = context_.input->modifier_down(input::Mod::Shift);
  bool fast = (left_button_held_ && right_button_held_) || shift_held;
  int step = fast ? FAST_STEP : NORMAL_STEP;

  if (button_held_frames_ >= REPEAT_DELAY &&
      (button_held_frames_ - REPEAT_DELAY) % REPEAT_RATE_SINGLE == 0) {
    auto& batch = ingredients_.at(*button_held_index_);
    auto& c = batch.count;

    if (hold_direction_ == HoldDirection::Increment) {
      int headroom = std::max(0, max_ingredient_stock_kg - batch.current_stock);
      c = std::min(c + step, headroom);
    }
    else {
      c = std::max(c - step, 0);
    }
    mark_dirty();
  }
}
