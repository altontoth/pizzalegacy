/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/depot/DepotExportScreen.h"

#include <algorithm>

#include "core/city/CentralDepot.h"
#include "core/city/City.h"
#include "core/food/IngredientCategory.h"
#include "core/state/GameState.h"
#include "engine/FormatCompat.h"
#include "engine/Logger.h"
#include "engine/geom/Spaces.h"
#include "engine/ui/InteractionEvent.h"
#include "factories/IngredientFactory.h"
#include "gfx/Blit.h"
#include "resources/BlitmapManager.h"
#include "resources/FontManager.h"

DepotExportScreen::DepotExportScreen(GameContext& context, GameState& state,
                                     IngredientCategory category,
                                     core::CityId target_city_id,
                                     Money transport_cost)
    : StockMain(context, state), category_(category),
      target_city_id_(target_city_id), transport_cost_(transport_cost) {

  source_city_ = state_.active_city();

  // Load target city... Shouldn't be needed because you can't export to a city
  // you haven't finished but let's just prevent a crash when I mess with this
  // during debugging and console hackery
  state_.ensure_city_loaded(target_city_id);
  target_city_ = state_.city(target_city_id);

  auto player_id = state_.active_player()->id();
  auto& source_depot = source_city_->central_depot(player_id);
  auto& target_depot = target_city_->central_depot(player_id);

  auto [start_id, end_id] = core::food::CategoryRegistry::id_range(category);
  for (int ing_id = start_id; ing_id < end_id; ing_id++) {
    auto* ingredient = IngredientFactory::create_ingredient(ing_id);
    if (!ingredient) {
      continue;
    }

    const auto* src_stock = source_depot.get_ingredient_stock(ing_id);
    const auto* tgt_stock = target_depot.get_ingredient_stock(ing_id);

    ExportItem item;
    item.ingredient_id = ing_id;
    item.ingredient = ingredient;
    item.source_stock_kg = src_stock ? src_stock->quantity / 1000 : 0;
    item.target_stock_kg = tgt_stock ? tgt_stock->quantity / 1000 : 0;
    item.transfer_amount = 0;
    items_.push_back(item);
  }

  status_controller_.show_restaurant({.state = &state_,
                                      .rest = state_.active_restaurant(),
                                      .label = "EXPORT"});
  set_up_input_panel();
}

void DepotExportScreen::tick() { update_held_button_repeat(); }

void DepotExportScreen::render(const RenderContext& ctx) {
  draw_page_header(ctx);
  draw_ingredient_list(ctx);
}

void DepotExportScreen::draw_page_header(const RenderContext& ctx) {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back9"));

  auto& blitmap1 = context_.blitmap->get<Blitmap1Icon>();
  Blitmap1Icon icon = core::food::CategoryRegistry::icon(category_);
  blitmap1.render_icon(icon, *ctx.surface, ViewPoint{42, 6});

  std::string title = compat::format("export --> {}", target_city_->name());
  const int center_x = ctx.surface->rect().center().x;
  context_.font->render_text_onto(*ctx.surface, title,
                                  ViewPoint{center_x + 40, 12},
                                  FontFace::PIZ_NUMB, 8, TextAnchor::Middle);
}

void DepotExportScreen::draw_ingredient_list(const RenderContext& ctx) {
  interactions_.begin_frame();

  const int header_y = 34;
  const uint8_t text_color = 8;

  // Column headers: source city (truncated), AMOUNT, target city (truncated)
  std::string_view src_name = source_city_->name().substr(0, 5);
  std::string_view tgt_name = target_city_->name().substr(0, 5);

  const int right_edge = ctx.surface->rect().right();
  const int src_x = right_edge - 170;
  const int amount_x = right_edge - 110;
  const int tgt_x = right_edge - 50;

  context_.font->render_text_onto(
      *ctx.surface, src_name, ViewPoint{src_x, header_y}, FontFace::SUPERSMA,
      text_color, TextAnchor::End);
  context_.font->render_text_onto(
      *ctx.surface, "AMOUNT", ViewPoint{amount_x, header_y}, FontFace::SUPERSMA,
      text_color, TextAnchor::End);
  context_.font->render_text_onto(
      *ctx.surface, tgt_name, ViewPoint{tgt_x, header_y}, FontFace::SUPERSMA,
      text_color, TextAnchor::End);

  const int name_x = 22;
  int y = header_y + 20;
  const int line_height = 12;
  const int row_width = 300;

  for (size_t i = 0; i < items_.size(); i++) {
    auto& item = items_[i];

    if (hovered_index_ == static_cast<int>(i) ||
        button_held_index_ == static_cast<int>(i)) {
      ctx.surface->fill_rect(
          ViewRect{name_x, y - 2, row_width, line_height + 2}, 165);
    }

    interactions_.create_frame_item(
        ViewRect{name_x, y - 2, row_width, line_height},
        compat::format("export_{}", i),
        [this, i](views::ui::InteractionEvent ie) {
          switch (ie.type) {
          case views::ui::InteractionEvent::Type::Enter:
            if (button_held_index_ < 0) {
              hovered_index_ = static_cast<int>(i);
              mark_dirty();
            }
            break;
          case views::ui::InteractionEvent::Type::Leave:
            if (button_held_index_ < 0 &&
                hovered_index_ == static_cast<int>(i)) {
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
          default:
            break;
          }
        });

    // Ingredient name
    context_.font->render_text_onto(*ctx.surface, item.ingredient->name(),
                                    ViewPoint{name_x, y}, FontFace::CSTANDAR,
                                    text_color);

    // Source stock
    int effective_source = item.source_stock_kg - item.transfer_amount;
    context_.font->render_text_onto(
        *ctx.surface, std::to_string(effective_source), ViewPoint{src_x, y},
        FontFace::CSTANDAR, text_color, TextAnchor::End);

    // Transfer amount
    context_.font->render_text_onto(*ctx.surface,
                                    std::to_string(item.transfer_amount),
                                    ViewPoint{amount_x, y}, FontFace::CSTANDAR,
                                    text_color, TextAnchor::End);

    // Target stock
    int effective_target = item.target_stock_kg + item.transfer_amount;
    context_.font->render_text_onto(
        *ctx.surface, std::to_string(effective_target), ViewPoint{tgt_x, y},
        FontFace::CSTANDAR, text_color, TextAnchor::End);

    y += line_height;
  }

  // Footer: sum of transferred goods value
  const int footer_y = 320;
  auto player_id = state_.active_player()->id();
  auto& source_depot = source_city_->central_depot(player_id);

  Money total_value{0};
  for (const auto& item : items_) {
    if (item.transfer_amount > 0) {
      const auto* stock = source_depot.get_ingredient_stock(item.ingredient_id);
      if (stock) {
        total_value = total_value + Money{stock->weighted_avg_cost.cents *
                                          item.transfer_amount};
      }
    }
  }

  context_.font->render_text_onto(*ctx.surface,
                                  "SUM:", ViewPoint{name_x, footer_y},
                                  FontFace::SUPERSMA, text_color);
  context_.font->render_text_onto(
      *ctx.surface, compat::format("{:n}", total_value),
      ViewPoint{tgt_x, footer_y}, FontFace::SUPERSMA, 20, TextAnchor::End);
}

void DepotExportScreen::set_up_input_panel() {
  input_panel_->delete_buttons();

  input_panel_->add_button(Icon::THUMBS_UP, ButtonType::Horizontal,
                           ButtonPosition::BottomLeft, "O.K.",
                           [this] { execute_export(); });

  input_panel_->add_button(Icon::ARROW_BACK, ButtonType::Square,
                           ButtonPosition::BottomRight, "Back", [this] {
                             charge_exit();
                             state_.ui().pop();
                           });
}

void DepotExportScreen::charge_exit() { state_.advance_time_blocks(2); }

void DepotExportScreen::execute_export() {
  // Check if there's anything to transfer
  bool has_transfer = false;
  for (const auto& item : items_) {
    if (item.transfer_amount > 0) {
      has_transfer = true;
      break;
    }
  }
  if (!has_transfer) {
    charge_exit();
    state_.ui().pop();
    return;
  }

  // Pay transport costs
  if (transport_cost_ > 0_usd &&
      !state_.spend_cash(Account::Stock, transport_cost_)) {
    return;
  }

  auto player_id = state_.active_player()->id();
  auto& source_depot = source_city_->central_depot(player_id);
  auto& target_depot = target_city_->central_depot(player_id);

  for (const auto& item : items_) {
    if (item.transfer_amount <= 0) {
      continue;
    }
    auto type = static_cast<core::food::IngredientType>(item.ingredient_id);
    int qty_grams = item.transfer_amount * 1000;

    // Read source quality and cost before consuming
    const auto* stock = source_depot.get_ingredient_stock(item.ingredient_id);
    int quality = stock ? stock->weighted_avg_quality : 100;
    Money cost = stock ? stock->weighted_avg_cost : Money{0};

    source_depot.consume_ingredient(type, qty_grams);
    target_depot.add_ingredient(type, qty_grams, quality, cost);
  }

  charge_exit();
  state_.ui().pop();
}

void DepotExportScreen::handle_button_press(size_t ingredient_index,
                                            bool is_left_button) {
  auto& item = items_.at(ingredient_index);

  if (is_left_button) {
    item.transfer_amount = std::max(item.transfer_amount - 1, 0);
    left_button_held_ = true;
  }
  else {
    item.transfer_amount =
        std::min(item.transfer_amount + 1, item.source_stock_kg);
    right_button_held_ = true;
  }

  if (button_held_index_ < 0) {
    button_held_index_ = static_cast<int>(ingredient_index);
    increment_direction_ = !is_left_button;
    button_held_frames_ = 0;
  }

  mark_dirty();
}

void DepotExportScreen::handle_button_release(bool is_left_button) {
  if (is_left_button) {
    left_button_held_ = false;
  }
  else {
    right_button_held_ = false;
  }

  if (!left_button_held_ && !right_button_held_) {
    button_held_index_ = -1;
    button_held_frames_ = 0;
  }
}

void DepotExportScreen::update_held_button_repeat() {
  if (button_held_index_ < 0 || (!left_button_held_ && !right_button_held_)) {
    return;
  }

  button_held_frames_++;

  int repeat_rate = (left_button_held_ && right_button_held_)
                        ? REPEAT_RATE_DUAL
                        : REPEAT_RATE_SINGLE;

  if (button_held_frames_ >= REPEAT_DELAY &&
      (button_held_frames_ - REPEAT_DELAY) % repeat_rate == 0) {
    auto& item = items_.at(button_held_index_);

    if (increment_direction_) {
      item.transfer_amount =
          std::min(item.transfer_amount + 1, item.source_stock_kg);
    }
    else {
      item.transfer_amount = std::max(item.transfer_amount - 1, 0);
    }
    mark_dirty();
  }
}
