/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/stock/IngredientOrderScreen.h"

#include "core/character/Player.h"
#include "core/city/CentralDepot.h"
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

#include "core/status/StatusHub.h"

#include <algorithm>

constexpr float sell_price_ratio = 0.8f;

IngredientOrderScreen::IngredientOrderScreen(
    GameContext& context, GameState& state, uint8_t supplier_id,
    IngredientCategory category, IngredientScreenMode mode,
    std::optional<std::vector<IngredientBatch>> saved_ingredients)
    : StockMain(context, state), category_(category), mode_(mode) {

  // Find the supplier
  auto* city = state_.active_city();
  ActorType supplier_type =
      core::food::CategoryRegistry::supplier_type(category);

  auto suppliers = city->food_suppliers(supplier_type);
  for (const auto* s : suppliers) {
    if (s->id() == supplier_id) {
      supplier_ = s;
      break;
    }
  }

  // Build ingredient list
  auto* restaurant = state_.active_restaurant();
  const bool is_depot = mode != IngredientScreenMode::RESTAURANT;

  // For depot mode, read stock from central depot; for restaurant, from
  // inventory
  core::city::CentralDepot* depot_ptr = nullptr;
  if (is_depot) {
    depot_ptr = &city->central_depot(state_.active_player()->id());
  }
  const auto& inventory = restaurant->inventory();

  auto* player = state_.active_player();
  assert(player);
  core::food::IngredientDemand demand_grams{};
  if (is_depot) {
    demand_grams = state_.total_ingredient_need_for_player(player->id());
  }
  else {
    demand_grams =
        restaurant->calculate_daily_ingredient_demand_grams(player->recipes());
  }

  auto [start_id, end_id] = core::food::CategoryRegistry::id_range(category);
  for (int ing_id = start_id; ing_id < end_id; ing_id++) {
    auto* ingredient = IngredientFactory::create_ingredient(ing_id);
    if (ingredient) {
      IngredientBatch batch;
      batch.ingredient = ingredient;
      batch.ingredient_type = ingredient->type();
      batch.count = 0;
      Money base_price = calculate_ingredient_price(ingredient->type());
      if (mode == IngredientScreenMode::CENTRAL_DEPOT_SELL) {
        batch.price_per_unit =
            Money{static_cast<int64_t>(base_price.cents * sell_price_ratio)};
      }
      else {
        batch.price_per_unit = base_price;
      }
      batch.quality = supplier_ ? supplier_->quality() : 100;

      // Get current stock from depot or restaurant inventory (grams to kilos)
      const auto* stock =
          is_depot ? depot_ptr->get_ingredient_stock(ing_id)
                   : inventory.get_ingredient_stock(ingredient->type());
      batch.current_stock = stock ? stock->quantity / 1000 : 0;

      // Get calculated demand (in grams)
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

  if (mode_ != IngredientScreenMode::RESTAURANT) {
    constexpr uint8_t header_color = 12;
    const bool is_sell = mode_ == IngredientScreenMode::CENTRAL_DEPOT_SELL;
    std::string prefix = is_sell ? "central sale  " : "central purch.  ";
    std::string title = prefix + state_.active_city()->name();
    context_.font->render_text_onto(*ctx.surface, title, ViewPoint{40, 12},
                                    FontFace::PIZ_NUMB, header_color,
                                    AlignX::Center);
  }
  else {
    constexpr uint8_t header_color = 8;
    std::string title = supplier_ ? supplier_->name() : "SUPPLIER";
    context_.font->render_text_onto(*ctx.surface, title, ViewPoint{40, 16},
                                    FontFace::PIZ_NUMB, header_color,
                                    AlignX::Center);
  }
}

void IngredientOrderScreen::register_ingredient_row(ViewRect row_rect,
                                                    size_t i) {
  interactions_.create_frame_item(
      row_rect, compat::format("ingredient_{}", i),
      [this, i](views::ui::InteractionEvent ie) {
        switch (ie.type) {
        case views::ui::InteractionEvent::Type::Enter:
          // Don't change hover if a button is currently held
          if (button_held_index_ < 0) {
            hovered_index_ = i;
            mark_dirty();
          }
          break;
        case views::ui::InteractionEvent::Type::Leave:
          // Don't clear hover if a button is currently held
          if (button_held_index_ < 0 && hovered_index_ == static_cast<int>(i)) {
            hovered_index_ = -1;
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
  if (mode_ == IngredientScreenMode::RESTAURANT) {
    draw_restaurant_ingredient_list(ctx);
  }
  else {
    draw_depot_ingredient_list(ctx);
  }
}

void IngredientOrderScreen::draw_depot_ingredient_list(
    const RenderContext& ctx) {
  const bool is_sell = mode_ == IngredientScreenMode::CENTRAL_DEPOT_SELL;

  constexpr int name_x = 8;
  constexpr int row_width = 334;
  constexpr uint8_t text_color = 8;
  constexpr uint8_t header_color = 12;
  constexpr int line_height = 12;
  constexpr int header_y = 34;
  constexpr int header_price_x = 196;
  constexpr int header_suppl_x = 258;
  constexpr int header_amount_x = 312;
  constexpr int header_store_x = 360;
  constexpr int price_x = 200;
  constexpr int amount_x = 312;
  constexpr int store_x = 360;
  constexpr int sum_x = 300;
  constexpr int footer_y = 320;

  context_.font->render_text_onto(
      *ctx.surface, "price", ViewPoint{header_price_x, header_y},
      FontFace::SUPERSMA, header_color, AlignX::RightEdge);
  context_.font->render_text_onto(
      *ctx.surface, "suppl", ViewPoint{header_suppl_x, header_y},
      FontFace::SUPERSMA, header_color, AlignX::RightEdge);
  context_.font->render_text_onto(
      *ctx.surface, "amount", ViewPoint{header_amount_x, header_y},
      FontFace::SUPERSMA, header_color, AlignX::RightEdge);
  context_.font->render_text_onto(
      *ctx.surface, "store", ViewPoint{header_store_x, header_y},
      FontFace::SUPERSMA, header_color, AlignX::RightEdge);

  int y = header_y + 20;
  for (size_t i = 0; i < ingredients_.size(); i++) {
    auto& batch = ingredients_[i];
    bool supplier_sells = batch.price_per_unit.cents > 0;

    if (hovered_index_ == static_cast<int>(i) ||
        button_held_index_ == static_cast<int>(i)) {
      constexpr int highlight_x = 210;
      constexpr int highlight_w = 160;
      ctx.surface->fill_rect(
          ViewRect{highlight_x, y - 2, highlight_w, line_height + 2}, 165);
    }

    if (supplier_sells) {
      register_ingredient_row({name_x, y - 2, row_width, line_height}, i);
    }

    context_.font->render_text_onto(*ctx.surface, batch.ingredient->name(),
                                    ViewPoint{name_x, y}, FontFace::CSTANDAR,
                                    text_color);

    if (supplier_sells) {
      context_.font->render_text_onto(
          *ctx.surface, compat::format("{:n}", batch.price_per_unit),
          ViewPoint{price_x, y}, FontFace::CSTANDAR, text_color,
          AlignX::RightEdge);
      int city_supply = supplier_ ? supplier_->stock(batch.ingredient_type) : 0;
      context_.font->render_text_onto(
          *ctx.surface, std::to_string(city_supply - batch.count),
          ViewPoint{header_suppl_x, y}, FontFace::CSTANDAR, text_color,
          AlignX::RightEdge);
      context_.font->render_text_onto(
          *ctx.surface, std::to_string(batch.count), ViewPoint{amount_x, y},
          FontFace::CSTANDAR, text_color, AlignX::RightEdge);
      int projected_store = is_sell ? batch.current_stock - batch.count
                                    : batch.current_stock + batch.count;
      context_.font->render_text_onto(
          *ctx.surface, std::to_string(projected_store), ViewPoint{store_x, y},
          FontFace::CSTANDAR, text_color, AlignX::RightEdge);
    }
    else {
      context_.font->render_text_onto(*ctx.surface, "------",
                                      ViewPoint{price_x, y}, FontFace::CSTANDAR,
                                      text_color, AlignX::RightEdge);
    }

    y += line_height;
  }

  Money total_cost{0};
  for (const auto& batch : ingredients_) {
    if (batch.count > 0) {
      total_cost = total_cost + Money{batch.price_per_unit.cents * batch.count};
    }
  }

  context_.font->render_text_onto(
      *ctx.surface,
      is_sell ? "income:" : "sum:", ViewPoint{header_price_x, footer_y},
      FontFace::CSTANDAR, text_color, AlignX::RightEdge);
  context_.font->render_text_onto(
      *ctx.surface, compat::format("{:n}", total_cost),
      ViewPoint{sum_x, footer_y}, FontFace::CSTANDAR,
      is_sell ? uint8_t{40} : uint8_t{20}, AlignX::RightEdge);
}

void IngredientOrderScreen::draw_restaurant_ingredient_list(
    const RenderContext& ctx) {
  const bool is_sell = mode_ == IngredientScreenMode::CENTRAL_DEPOT_SELL;

  const int header_y = 34;
  const uint8_t text_color = 8;
  const uint8_t text_color_days = 12;

  const int name_x = 22;
  const int header_price_x = -178;
  const int price_x = -172;
  const int header_amount_x = -110;
  const int amount_x = -128;
  const int header_stock_x = -66;
  const int stock_x = -72;
  const int header_days_x = -26;
  const int days_x = -32;

  context_.font->render_text_onto(
      *ctx.surface, "PRICE", ViewPoint{header_price_x, header_y},
      FontFace::SUPERSMA, text_color, AlignX::Right);
  context_.font->render_text_onto(
      *ctx.surface, "AMOUNT", ViewPoint{header_amount_x, header_y},
      FontFace::SUPERSMA, text_color, AlignX::Right);
  context_.font->render_text_onto(
      *ctx.surface, "STOCK", ViewPoint{header_stock_x, header_y},
      FontFace::SUPERSMA, text_color, AlignX::Right);
  context_.font->render_text_onto(
      *ctx.surface, "DAYS", ViewPoint{header_days_x, header_y},
      FontFace::SUPERSMA, text_color, AlignX::Right);

  const int line_color = 15;
  const int line_width = 2;
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

    // Check if supplier sells this ingredient (price would be zero if not)
    bool supplier_sells = batch.price_per_unit.cents > 0;

    // Draw gray background on hover or when holding button (only if supplier
    // sells this ingredient)
    if ((hovered_index_ == static_cast<int>(i) ||
         button_held_index_ == static_cast<int>(i))) {
      ctx.surface->fill_rect(
          ViewRect{name_x, y - 2, row_width, line_height + 2}, 165);
    }

    // Create interactive region for the entire row (only if supplier sells this
    // ingredient)
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
                                      text_color, AlignX::Right);
    }
    else {
      context_.font->render_text_onto(*ctx.surface, "------",
                                      ViewPoint{price_x, y}, FontFace::CSTANDAR,
                                      text_color, AlignX::Right);
    }

    // Only show amount, stock, and days for ingredients the supplier sells
    if (supplier_sells) {
      context_.font->render_text_onto(
          *ctx.surface, std::to_string(batch.count), ViewPoint{amount_x, y},
          FontFace::CSTANDAR, text_color, AlignX::Right);

      // Render stock (current +/- order amount depending on buy/sell)
      int projected_stock = is_sell ? batch.current_stock - batch.count
                                    : batch.current_stock + batch.count;
      context_.font->render_text_onto(
          *ctx.surface, std::to_string(projected_stock), ViewPoint{stock_x, y},
          FontFace::CSTANDAR, text_color, AlignX::Right);

      // Render days remaining (show if ingredient has usage on menu)
      // Calculate days based on projected stock (current + order amount)
      if (batch.daily_usage > 0) {
        // usage is in grams per day; enforce minimum of 1kg/day
        int effective_usage = std::max(batch.daily_usage, 1000);
        int days_remaining = (projected_stock * 1000) / effective_usage;

        std::string days_text = std::to_string(days_remaining);
        context_.font->render_text_onto(
            *ctx.surface, days_text, ViewPoint{days_x, y}, FontFace::CSTANDAR,
            text_color_days, AlignX::Right);
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

  // Calculate total cost
  Money total_cost{0};
  for (const auto& batch : ingredients_) {
    if (batch.count > 0) {
      total_cost = total_cost + Money{batch.price_per_unit.cents * batch.count};
    }
  }

  // Total
  std::string total_text = compat::format("{:n}", total_cost);
  context_.font->render_text_onto(
      *ctx.surface,
      is_sell ? "INCOME:" : "TOTAL:", ViewPoint{header_price_x, footer_y},
      FontFace::SUPERSMA, text_color, AlignX::Right);
  context_.font->render_text_onto(*ctx.surface, total_text,
                                  ViewPoint{stock_x, footer_y},
                                  FontFace::SUPERSMA, red_color, AlignX::Right);
}

void IngredientOrderScreen::draw_page_screen(const RenderContext& ctx) {
  draw_ingredient_list(ctx);
}

void IngredientOrderScreen::render(const RenderContext& ctx) {
  draw_page_header(ctx);
  draw_page_screen(ctx);
  // ctx.surface->palette({60});
}

void IngredientOrderScreen::place_order() {
  const bool is_sell = mode_ == IngredientScreenMode::CENTRAL_DEPOT_SELL;

  // Calculate total cost first
  Money total_cost{0};
  int total_items = 0;
  for (const auto& batch : ingredients_) {
    if (batch.count > 0) {
      total_items += batch.count;
      total_cost = total_cost + Money{batch.price_per_unit.cents * batch.count};
    }
  }

  Logger::info("[Order] Total: {} items for {}", total_items, total_cost);

  if (is_sell) {
    // Selling: remove from depot and credit the player
    auto& depot =
        state_.active_city()->central_depot(state_.active_player()->id());
    for (const auto& batch : ingredients_) {
      if (batch.count > 0) {
        depot.consume_ingredient(batch.ingredient_type, batch.count * 1000);

        Logger::info("[Depot Sale] {} x{} @ {} each", batch.ingredient->name(),
                     batch.count, batch.price_per_unit);
      }
    }

    if (total_cost.cents > 0) {
      auto* player = state_.active_player();
      player->financials().transfer(state_.time().current_date(),
                                    Account::Stock, Account::Cash, total_cost,
                                    AccountingScope::Operational);
    }
  }
  else {
    // Buying: deduct money (blocks if insufficient funds)
    if (total_cost.cents > 0 &&
        !state_.spend_cash(Account::Stock, total_cost)) {
      return;
    }

    if (mode_ == IngredientScreenMode::CENTRAL_DEPOT) {
      // Add to central depot (not tied to a restaurant's monthly accounting)
      auto& depot =
          state_.active_city()->central_depot(state_.active_player()->id());
      for (const auto& batch : ingredients_) {
        if (batch.count > 0) {
          auto ingredient_type = batch.ingredient_type;
          depot.add_ingredient(ingredient_type, batch.count * 1000,
                               batch.quality, batch.price_per_unit);

          Logger::info("[Depot Order] {} x{} @ {} (quality {})",
                       batch.ingredient->name(), batch.count,
                       batch.price_per_unit, batch.quality);
        }
      }

      int social_gain = total_items / 1000;
      if (social_gain > 0) {
        state_.adjust_social_score(*state_.active_player(), social_gain);
      }
    }
    else {
      auto* restaurant = state_.active_restaurant();
      // Process purchases
      for (const auto& batch : ingredients_) {
        if (batch.count > 0) {
          auto ingredient_type = batch.ingredient_type;
          restaurant->purchase_ingredients(ingredient_type, batch.count,
                                           batch.quality, batch.price_per_unit);

          Logger::info("[Order] {} x{} @ {} each (quality {})",
                       batch.ingredient->name(), batch.count,
                       batch.price_per_unit, batch.quality);
        }
      }
    }

    // Record vendor spending for this week and update supplier relationship
    if (supplier_ && total_cost.cents > 0) {
      auto* city = state_.active_city();
      auto player_id = state_.active_player()->id();
      city->record_vendor_purchase(player_id, supplier_->id(), total_cost);

      auto& rel = const_cast<FoodSupplier*>(supplier_)->relationship(player_id);
      rel.total_spent += total_cost;
    }
  }

  if (!is_sell && total_items > 0) {
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

  static constexpr float difficulty_multipliers[] = {0.6f, 0.8f, 1.0f, 1.5f,
                                                     1.9f};

  auto* city = state_.active_city();
  Money base_price = city->ingredient_base_price(ingredient_type);

  int difficulty = state_.difficulty_level();

  return supplier_->calculate_price(ingredient_type, base_price,
                                    difficulty_multipliers[difficulty - 1]);
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

  if (mode_ != IngredientScreenMode::RESTAURANT) {
    input_panel_->add_button(Icon::THUMBS_UP, ButtonType::Horizontal,
                             ButtonPosition::BottomLeft, "O.K.",
                             [this] { place_order(); });
  }
  else {
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
  }

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
  int step = shift_held ? FAST_STEP : 1;
  const bool is_sell = mode_ == IngredientScreenMode::CENTRAL_DEPOT_SELL;

  // Perform immediate increment/decrement
  if (is_left_button) {
    if (is_sell) {
      // Check quality: depot stock quality must be >= supplier quality
      auto& depot =
          state_.active_city()->central_depot(state_.active_player()->id());
      auto* stock =
          depot.get_ingredient_stock(static_cast<int>(batch.ingredient_type));
      if (stock && supplier_ &&
          stock->weighted_avg_quality < supplier_->quality()) {
        state_.status().post_direct(StatusMsg{
            .payload = "The quality of this ingredient is lower than expected "
                       "from the supplier selected!",
            .severity = StatusSeverity::Warning,
        });
        return;
      }
      c = std::min(c + step, batch.current_stock);
    }
    else if (mode_ == IngredientScreenMode::CENTRAL_DEPOT) {
      int city_supply = supplier_ ? supplier_->stock(batch.ingredient_type) : 0;
      c = std::min(c + step, city_supply);
    }
    else {
      c = std::min(c + step, max_ingredient_stock_kg);
    }
    left_button_held_ = true;
  }
  else {
    c = std::max(c - step, 0);
    right_button_held_ = true;
  }

  // If this is the first button pressed, set direction and index
  if (button_held_index_ < 0) {
    button_held_index_ = ingredient_index;
    increment_direction_ = is_left_button;
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

  // If both buttons are now released, reset held state
  if (!left_button_held_ && !right_button_held_) {
    button_held_index_ = -1;
    button_held_frames_ = 0;
  }
}

void IngredientOrderScreen::handle_scroll(size_t ingredient_index, int delta) {
  auto& batch = ingredients_.at(ingredient_index);
  auto& c = batch.count;
  int step = std::max(1, std::abs(delta));
  const bool is_sell = mode_ == IngredientScreenMode::CENTRAL_DEPOT_SELL;

  if (delta > 0) {
    if (is_sell) {
      auto& depot =
          state_.active_city()->central_depot(state_.active_player()->id());
      auto* stock =
          depot.get_ingredient_stock(static_cast<int>(batch.ingredient_type));
      if (stock && supplier_ &&
          stock->weighted_avg_quality < supplier_->quality()) {
        state_.status().post_direct(StatusMsg{
            .payload = "The quality of this ingredient is lower than expected "
                       "from the supplier selected!",
            .severity = StatusSeverity::Warning,
        });
        return;
      }
      c = std::min(c + step, batch.current_stock);
    }
    else if (mode_ == IngredientScreenMode::CENTRAL_DEPOT) {
      int city_supply = supplier_ ? supplier_->stock(batch.ingredient_type) : 0;
      c = std::min(c + step, city_supply);
    }
    else {
      c = std::min(c + step, max_ingredient_stock_kg);
    }
  }
  else if (delta < 0) {
    c = std::max(c - step, 0);
  }

  mark_dirty();
}

void IngredientOrderScreen::update_held_button_repeat() {
  // Handle continuous clicking when button is held
  if (button_held_index_ < 0 || (!left_button_held_ && !right_button_held_)) {
    return;
  }

  button_held_frames_++;

  // Determine step size: +/-5 when both buttons held or Shift is down
  bool shift_held = context_.input->modifier_down(input::Mod::Shift);
  bool fast = (left_button_held_ && right_button_held_) || shift_held;
  int step = fast ? FAST_STEP : 1;

  if (button_held_frames_ >= REPEAT_DELAY &&
      (button_held_frames_ - REPEAT_DELAY) % REPEAT_RATE_SINGLE == 0) {
    auto& batch = ingredients_.at(button_held_index_);
    auto& c = batch.count;

    // Continue in the direction set by the first button pressed
    if (increment_direction_) {
      int max_count;
      if (mode_ == IngredientScreenMode::CENTRAL_DEPOT_SELL) {
        max_count = batch.current_stock;
      }
      else if (mode_ == IngredientScreenMode::CENTRAL_DEPOT) {
        max_count = supplier_ ? supplier_->stock(batch.ingredient_type) : 0;
      }
      else {
        max_count = max_ingredient_stock_kg;
      }
      c = std::min(c + step, max_count);
    }
    else {
      c = std::max(c - step, 0);
    }
    mark_dirty();
  }
}
