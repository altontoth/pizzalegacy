/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/stock/DepotIngredientOrderScreen.h"

#include "core/character/Player.h"
#include "core/city/CentralDepot.h"
#include "core/city/City.h"
#include "core/finance/Ledger.h"
#include "core/food/Ingredient.h"
#include "core/food/IngredientCategory.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "core/supply/FoodSupplier.h"
#include "engine/FormatCompat.h"
#include "engine/Logger.h"
#include "engine/geom/Spaces.h"
#include "engine/input/Action.h"
#include "engine/input/InputManager.h"
#include "engine/ui/InteractionEvent.h"
#include "factories/IngredientFactory.h"
#include "gfx/Blit.h"
#include "resources/BlitmapManager.h"
#include "resources/FontManager.h"

#include <algorithm>
#include <array>

constexpr int depot_sell_price_percent = 80;

DepotIngredientOrderScreen::DepotIngredientOrderScreen(
    GameContext& context, GameState& state, const DepotIngredientOrderState& ds)
    : MainPanelView(context, state), status_controller_(state.status()),
      category_(ds.category),
      is_sell_(ds.mode == IngredientScreenMode::CENTRAL_DEPOT_SELL),
      listener_(context.input->register_listener(*this)) {

  auto* city = state_.active_city();
  ActorType supplier_type =
      core::food::CategoryRegistry::supplier_type(category_);

  for (auto* s : city->food_suppliers(supplier_type)) {
    if (s->id() == ds.supplier_id) {
      supplier_ = s;
      break;
    }
  }

  auto& depot = city->central_depot(state_.active_player()->id());
  const core::food::IngredientDemand demand_grams =
      state_.total_ingredient_need_for_player(state_.active_player()->id(),
                                              city->id());

  auto [start_id, end_id] = core::food::CategoryRegistry::id_range(category_);
  for (int ing_id = start_id; ing_id < end_id; ing_id++) {
    auto* ingredient = IngredientFactory::create_ingredient(ing_id);
    if (!ingredient) {
      continue;
    }
    IngredientBatch batch;
    batch.ingredient = ingredient;
    batch.ingredient_type = ingredient->type();
    batch.count = 0;
    Money base_price = calculate_ingredient_price(ingredient->type());
    if (is_sell_) {
      batch.price_per_unit =
          Money{base_price.cents * depot_sell_price_percent / 100};
    }
    else {
      batch.price_per_unit = base_price;
    }
    batch.quality = supplier_ ? supplier_->quality() : 100;

    const auto* stock = depot.get_ingredient_stock(ing_id);
    batch.current_stock = stock ? stock->quantity / 1000 : 0;

    batch.daily_usage = demand_grams[ing_id];
    batch.need = (demand_grams[ing_id] > 0) ? 1 : 0;

    ingredients_.push_back(batch);
  }

  if (ds.saved_ingredients && context_.config->options().qol_improvements) {
    for (auto& batch : ingredients_) {
      for (const auto& saved : *ds.saved_ingredients) {
        if (saved.ingredient_type == batch.ingredient_type) {
          batch.count = saved.count;
          break;
        }
      }
    }
  }

  status_controller_.show_city_default(
      {.state = &state_, .city = state_.active_city()});
  set_up_input_panel();
}

void DepotIngredientOrderScreen::tick() { update_held_button_repeat(); }

void DepotIngredientOrderScreen::draw_icon(const RenderContext& ctx) {
  auto& blitmap1 = context_.blitmap->get<Blitmap1Icon>();
  Blitmap1Icon icon = core::food::CategoryRegistry::icon(category_);
  blitmap1.render_icon(icon, *ctx.surface, ViewPoint{42, 6});
}

void DepotIngredientOrderScreen::draw_page_header(const RenderContext& ctx) {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back9"));
  draw_icon(ctx);

  constexpr uint8_t header_color = 12;
  std::string_view prefix = is_sell_ ? "central sale  " : "central purch.  ";
  std::string title =
      compat::format("{}{}", prefix, state_.active_city()->name());
  context_.font->render_text_onto(
      *ctx.surface, title, ViewPoint{(ctx.surface->width() / 2) + 40, 12},
      FontFace::PIZ_NUMB, header_color, TextAnchor::Middle);
}

void DepotIngredientOrderScreen::register_ingredient_row(ViewRect row_rect,
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
        case views::ui::InteractionEvent::Type::Scroll: {
          // A sideways wheel orders in big steps; so does shift + wheel, which
          // most platforms report vertically and macOS reports sideways.
          const bool sideways = ie.scroll_delta == 0 && ie.scroll_delta_x != 0;
          const bool shift = context_.input->modifier_down(input::Mod::Shift);
          handle_scroll(i, sideways ? ie.scroll_delta_x : ie.scroll_delta,
                        sideways || shift);
          break;
        }
        default:
          break;
        }
      });
}

void DepotIngredientOrderScreen::draw_ingredient_list(
    const RenderContext& ctx) {
  interactions_.begin_frame();

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
      FontFace::SUPERSMA, header_color, TextAnchor::End);
  context_.font->render_text_onto(
      *ctx.surface, "suppl", ViewPoint{header_suppl_x, header_y},
      FontFace::SUPERSMA, header_color, TextAnchor::End);
  context_.font->render_text_onto(
      *ctx.surface, "amount", ViewPoint{header_amount_x, header_y},
      FontFace::SUPERSMA, header_color, TextAnchor::End);
  context_.font->render_text_onto(
      *ctx.surface, "store", ViewPoint{header_store_x, header_y},
      FontFace::SUPERSMA, header_color, TextAnchor::End);

  int y = header_y + 20;
  for (size_t i = 0; i < ingredients_.size(); i++) {
    auto& batch = ingredients_[i];
    bool supplier_sells = batch.price_per_unit.cents > 0;

    if (button_held_index_ ? button_held_index_ == i : hovered_index_ == i) {
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
          TextAnchor::End);
      int city_supply = supplier_ ? supplier_->stock(batch.ingredient_type) : 0;
      int projected_supply =
          is_sell_ ? city_supply + batch.count : city_supply - batch.count;
      context_.font->render_text_onto(
          *ctx.surface, std::to_string(projected_supply),
          ViewPoint{header_suppl_x, y}, FontFace::CSTANDAR, text_color,
          TextAnchor::End);
      context_.font->render_text_onto(
          *ctx.surface, std::to_string(batch.count), ViewPoint{amount_x, y},
          FontFace::CSTANDAR, text_color, TextAnchor::End);
      int projected_store = is_sell_ ? batch.current_stock - batch.count
                                     : batch.current_stock + batch.count;
      context_.font->render_text_onto(
          *ctx.surface, std::to_string(projected_store), ViewPoint{store_x, y},
          FontFace::CSTANDAR, text_color, TextAnchor::End);
    }
    else {
      context_.font->render_text_onto(*ctx.surface, "------",
                                      ViewPoint{price_x, y}, FontFace::CSTANDAR,
                                      text_color, TextAnchor::End);
    }

    y += line_height;
  }

  Money total_cost = calculate_total_cost();

  context_.font->render_text_onto(
      *ctx.surface,
      is_sell_ ? "income:" : "sum:", ViewPoint{header_price_x, footer_y},
      FontFace::CSTANDAR, text_color, TextAnchor::End);
  context_.font->render_text_onto(
      *ctx.surface, compat::format("{:n}", total_cost),
      ViewPoint{sum_x, footer_y}, FontFace::CSTANDAR,
      is_sell_ ? uint8_t{40} : uint8_t{20}, TextAnchor::End);
}

void DepotIngredientOrderScreen::render(const RenderContext& ctx) {
  draw_page_header(ctx);
  draw_ingredient_list(ctx);
}

void DepotIngredientOrderScreen::place_order() {
  Money total_cost = calculate_total_cost();
  int total_items = 0;
  for (const auto& batch : ingredients_) {
    total_items += batch.count;
  }

  Logger::info("[Order] Total: {} items for {}", total_items, total_cost);

  if (is_sell_) {
    auto& depot =
        state_.active_city()->central_depot(state_.active_player()->id());
    Money book_value{0};
    for (const auto& batch : ingredients_) {
      if (batch.count > 0) {
        const auto& stock = depot.get_stock(batch.ingredient_type);
        book_value =
            book_value + Money{stock.weighted_avg_cost.cents * batch.count};
        depot.consume_ingredient(batch.ingredient_type, batch.count * 1000);
        Logger::info("[Depot Sale] {} x{} @ {} each", batch.ingredient->name(),
                     batch.count, batch.price_per_unit);
      }
    }

    // Supplies is relieved of the full book value; the difference against the
    // proceeds is the gain or loss on the disposal.
    auto* player = state_.active_player();
    auto date = state_.time().current_date();
    Money relieved{std::min(book_value.cents, total_cost.cents)};
    if (relieved.cents > 0) {
      player->financials().transfer(date, Account::Stock, Account::Cash,
                                    relieved, AccountingScope::Operational);
    }
    if (book_value.cents > total_cost.cents) {
      player->financials().transfer(date, Account::Stock,
                                    Account::ProfitAndLoss,
                                    Money{book_value.cents - total_cost.cents},
                                    AccountingScope::Operational);
    }
    else if (total_cost.cents > book_value.cents) {
      player->financials().transfer(date, Account::ProfitAndLoss, Account::Cash,
                                    Money{total_cost.cents - book_value.cents},
                                    AccountingScope::Operational);
    }
  }
  else {
    if (total_cost.cents > 0 &&
        !state_.spend_cash(Account::Stock, total_cost)) {
      return;
    }

    auto& depot =
        state_.active_city()->central_depot(state_.active_player()->id());
    for (const auto& batch : ingredients_) {
      if (batch.count > 0) {
        depot.add_ingredient(batch.ingredient_type, batch.count * 1000,
                             batch.quality, batch.price_per_unit);
        if (supplier_) {
          int remaining = std::max(0, supplier_->stock(batch.ingredient_type) -
                                          batch.count);
          supplier_->set_stock(batch.ingredient_type, remaining);
        }
        Logger::info("[Depot Order] {} x{} @ {} (quality {})",
                     batch.ingredient->name(), batch.count,
                     batch.price_per_unit, batch.quality);
      }
    }

    int social_gain = total_items / 1000;
    if (social_gain > 0) {
      state_.adjust_social_score(*state_.active_player(), social_gain);
    }

    if (supplier_ && total_cost.cents > 0) {
      auto* city = state_.active_city();
      auto player_id = state_.active_player()->id();
      city->record_vendor_purchase(player_id, supplier_->id(), total_cost);

      auto& rel = supplier_->relationship(player_id);
      rel.total_spent += total_cost;
    }
  }

  if (!is_sell_ && total_items > 0) {
    if (auto* player = state_.active_player()) {
      player->on_food_shop_visit();
    }
  }

  state_.ui().pop();
}

Money DepotIngredientOrderScreen::calculate_ingredient_price(
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

Money DepotIngredientOrderScreen::calculate_total_cost() const {
  Money total{0};
  for (const auto& batch : ingredients_) {
    if (batch.count > 0) {
      total += Money{batch.price_per_unit.cents * batch.count};
    }
  }
  return total;
}

bool DepotIngredientOrderScreen::check_depot_quality(
    const IngredientBatch& batch) const {
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
    return false;
  }
  return true;
}

int DepotIngredientOrderScreen::max_increment_for(
    size_t ingredient_index) const {
  const auto& batch = ingredients_.at(ingredient_index);
  if (is_sell_) {
    return batch.current_stock;
  }
  int city_supply = supplier_ ? supplier_->stock(batch.ingredient_type) : 0;
  auto& depot =
      state_.active_city()->central_depot(state_.active_player()->id());
  int pending = 0;
  for (size_t i = 0; i < ingredients_.size(); i++) {
    if (i != ingredient_index) {
      pending += ingredients_[i].count;
    }
  }
  int depot_remaining = std::max(0, depot.capacity() - depot.used() - pending);
  return std::min(city_supply, depot_remaining);
}

bool DepotIngredientOrderScreen::on_action(const input::ActionEvent& ev) {
  return ev.action == input::Action::NextRestaurant ||
         ev.action == input::Action::PreviousRestaurant;
}

void DepotIngredientOrderScreen::set_up_input_panel() {
  input_panel_->delete_buttons();

  input_panel_->add_button(Icon::THUMBS_UP, ButtonType::Horizontal,
                           ButtonPosition::BottomLeft, "O.K.",
                           [this] { place_order(); });

  input_panel_->add_button(Icon::ARROW_BACK, ButtonType::Square,
                           ButtonPosition::BottomRight, "Back", [this] {
                             state_.advance_time_blocks(2);
                             state_.ui().pop();
                           });
}

int DepotIngredientOrderScreen::step_for_modifiers(bool dual_button) const {
  const bool shift = context_.input->modifier_down(input::Mod::Shift);
  const bool ctrl = context_.input->modifier_down(input::Mod::Ctrl);

  if (shift && ctrl) {
    return FASTEST_STEP;
  }
  if (ctrl) {
    return FASTER_STEP;
  }
  if (shift || dual_button) {
    return FAST_STEP;
  }
  return NORMAL_STEP;
}

void DepotIngredientOrderScreen::handle_button_press(size_t ingredient_index,
                                                     bool is_left_button) {
  auto& batch = ingredients_.at(ingredient_index);
  auto& c = batch.count;
  int step = step_for_modifiers(left_button_held_ && right_button_held_);

  if (is_left_button) {
    if (is_sell_ && !check_depot_quality(batch)) {
      return;
    }
    c = std::min(c + step, max_increment_for(ingredient_index));
    left_button_held_ = true;
  }
  else {
    c = std::max(c - step, 0);
    right_button_held_ = true;
  }

  if (!button_held_index_) {
    button_held_index_ = ingredient_index;
    hold_direction_ =
        is_left_button ? HoldDirection::Increment : HoldDirection::Decrement;
    button_held_frames_ = 0;
  }

  mark_dirty();
}

void DepotIngredientOrderScreen::handle_button_release(bool is_left_button) {
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

void DepotIngredientOrderScreen::handle_scroll(size_t ingredient_index,
                                               int delta, bool fast) {
  auto& batch = ingredients_.at(ingredient_index);
  auto& c = batch.count;
  int step = std::max(1, std::abs(delta)) * (fast ? FASTEST_STEP : NORMAL_STEP);

  if (delta > 0) {
    if (is_sell_ && !check_depot_quality(batch)) {
      return;
    }
    c = std::min(c + step, max_increment_for(ingredient_index));
  }
  else if (delta < 0) {
    c = std::max(c - step, 0);
  }

  mark_dirty();
}

void DepotIngredientOrderScreen::update_held_button_repeat() {
  if (!button_held_index_ || (!left_button_held_ && !right_button_held_)) {
    return;
  }

  button_held_frames_++;

  int step = step_for_modifiers(left_button_held_ && right_button_held_);

  if (button_held_frames_ >= REPEAT_DELAY &&
      (button_held_frames_ - REPEAT_DELAY) % REPEAT_RATE_SINGLE == 0) {
    auto& batch = ingredients_.at(*button_held_index_);
    auto& c = batch.count;

    if (hold_direction_ == HoldDirection::Increment) {
      c = std::min(c + step, max_increment_for(*button_held_index_));
    }
    else {
      c = std::max(c - step, 0);
    }
    mark_dirty();
  }
}
