/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/stock/Contracts.h"

#include "core/character/Player.h"
#include "core/city/City.h"
#include "core/factories/IngredientFactory.h"
#include "core/finance/Money.h"
#include "core/food/IngredientCategory.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "core/supply/ActiveContract.h"
#include "core/supply/FoodSupplier.h"
#include "core/util/LegacyConstants.h"
#include "engine/FormatCompat.h"
#include "engine/geom/Spaces.h"
#include "engine/gfx/Blit.h"
#include "engine/gfx/Draw.h"
#include "engine/ui/InteractionEvent.h"
#include "resources/FontManager.h"
#include "resources/FontTypes.h"
#include "views/base/ViewDefs.h"
#include "views/ui/Icon.h"

namespace {

const FoodSupplier* find_supplier(const ActiveContract& c,
                                  const GameState& state) {
  City* city = state.city(c.city_id);
  if (!city) {
    return nullptr;
  }
  ActorType atype = core::food::CategoryRegistry::supplier_type(c.category);
  for (const auto* s : city->food_suppliers(atype)) {
    if (s->id() == c.supplier_id) {
      return s;
    }
  }
  return nullptr;
}

} // namespace

Contracts::Contracts(GameContext& context, GameState& state)
    : RestaurantSubView(context, state, state.active_restaurant()->id()),
      status_controller_(state.status()) {
  update_contract_list();
  set_up_input_panel();
  status_controller_.show_restaurant(
      {.state = &state_, .rest = state_.active_restaurant(), .label = "STOCK"});
}

void Contracts::tick() {}

void Contracts::update_contract_list() {
  auto* player = state_.active_player();
  auto* restaurant = state_.active_restaurant();
  contract_indices_.clear();
  for (size_t i = 0; i < player->contracts().size(); ++i) {
    if (player->contracts()[i].restaurant_id == restaurant->id()) {
      contract_indices_.push_back(i);
    }
  }
  current_pos_ =
      std::clamp(current_pos_, 0,
                 std::max(0, static_cast<int>(contract_indices_.size()) - 1));
}

void Contracts::render(const RenderContext& ctx) {
  interactions_.begin_frame();

  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back5"));

  if (contract_indices_.empty()) {
    return;
  }

  if (show_back_) {
    render_back(ctx);
  }
  else {
    render_front(ctx);
  }
}

void Contracts::render_front(const RenderContext& ctx) {
  const size_t contract_idx =
      contract_indices_[static_cast<size_t>(current_pos_)];
  const ActiveContract& c = state_.active_player()->contracts()[contract_idx];
  const FoodSupplier* supplier = find_supplier(c, state_);
  const std::string supplier_name = supplier ? supplier->name() : "?";

  namespace LC = LegacyConstants;
  constexpr int heading_x = 124 - LC::WOOD_BORDER_WIDTH_LEFT;
  constexpr int label_x = 80 - LC::WOOD_BORDER_WIDTH_LEFT;
  constexpr int supplier_center_x = 220 - LC::WOOD_BORDER_WIDTH_LEFT;
  constexpr int value_x = 288 - LC::WOOD_BORDER_WIDTH_LEFT;
  constexpr int pct_x = 186 - LC::WOOD_BORDER_WIDTH_LEFT;
  constexpr int charge_x = 192 - LC::WOOD_BORDER_WIDTH_LEFT;
  constexpr int penalty_x = 280 - LC::WOOD_BORDER_WIDTH_LEFT;
  constexpr int amount_text_x = 164 - LC::WOOD_BORDER_WIDTH_LEFT;
  constexpr int quality_x = 246 - LC::WOOD_BORDER_WIDTH_LEFT;
  constexpr int days_x = 252 - LC::WOOD_BORDER_WIDTH_LEFT;
  constexpr int sep_x1 = 222 - LC::WOOD_BORDER_WIDTH_LEFT;
  constexpr int sep_x2 = 288 - LC::WOOD_BORDER_WIDTH_LEFT;

  constexpr int y_contract_with = 70 - LC::WOOD_BORDER_HEIGHT_TOP;
  constexpr int y_supplier = 72 - LC::WOOD_BORDER_HEIGHT_TOP;
  constexpr int y_total = 116 - LC::WOOD_BORDER_HEIGHT_TOP;
  constexpr int y_discount = 138 - LC::WOOD_BORDER_HEIGHT_TOP;
  constexpr int y_separator = 158 - LC::WOOD_BORDER_HEIGHT_TOP;
  constexpr int y_after_discount = 164 - LC::WOOD_BORDER_HEIGHT_TOP;
  constexpr int y_charge = 180 - LC::WOOD_BORDER_HEIGHT_TOP;
  constexpr int y_penalty = 202 - LC::WOOD_BORDER_HEIGHT_TOP;
  constexpr int y_amount = 224 - LC::WOOD_BORDER_HEIGHT_TOP;
  constexpr int y_quality = 246 - LC::WOOD_BORDER_HEIGHT_TOP;
  constexpr int y_remaining = 268 - LC::WOOD_BORDER_HEIGHT_TOP;

  auto text = [&](int x, int row_y, const std::string& s,
                  TextAnchor align = TextAnchor::Start) {
    TextStyle ts{
        .color = 8,
        .align = align,
        .character_spacing = 1,
    };

    context_.font->render_text_onto(*ctx.surface, s, ViewPoint{x, row_y},
                                    FontFace::PIZ_NUMB, ts);
  };

  text(heading_x, y_contract_with, "contract with");
  text(supplier_center_x, y_supplier + 20, supplier_name, TextAnchor::Middle);

  // Total
  text(label_x, y_total, "total:");
  text(value_x, y_total, compat::format("{:w}", c.daily_sum), TextAnchor::End);

  // Discount: label, percent, and negated amount on same row
  Money discount_amount = (c.daily_sum * c.discount_pct) / 100;
  text(label_x, y_discount, "discount:");
  text(pct_x, y_discount, compat::format("{}%", c.discount_pct),
       TextAnchor::End);
  text(value_x, y_discount, compat::format("{:w}", -discount_amount),
       TextAnchor::End);

  // Separator line between discount and after-discount total
  gfx::draw::line(*ctx.surface, ViewPoint{sep_x1, y_separator},
                  ViewPoint{sep_x2, y_separator}, gfx::ColorIndex(12), 2);

  // After-discount total (unlabeled)
  Money after_discount = c.daily_sum - discount_amount;
  text(value_x, y_after_discount, compat::format("{:w}", after_discount),
       TextAnchor::End);

  // Charge
  text(label_x, y_charge, "charge:");
  text(charge_x, y_charge, compat::format("{:w}", c.charge), TextAnchor::End);

  // Penalty
  text(label_x, y_penalty, "penalty clause:");
  text(penalty_x, y_penalty, compat::format("{:w}", c.cancellation_penalty),
       TextAnchor::End);

  // Amount toggle - clickable
  text(label_x, y_amount, "amount:");
  const std::string amount_str =
      c.deliver_as_required ? "as required" : "acc. to contract";
  text(amount_text_x, y_amount, amount_str);
  interactions_.create_frame_item(
      ViewRect{0, 0, views::base::MAIN_PANEL_WIDTH,
               views::base::MAIN_PANEL_HEIGHT},
      "amount_toggle", [this, contract_idx](views::ui::InteractionEvent ie) {
        if (ie.type == views::ui::InteractionEvent::Type::Click) {
          auto& contracts = state_.active_player()->contracts();
          if (contract_idx < contracts.size()) {
            contracts[contract_idx].deliver_as_required =
                !contracts[contract_idx].deliver_as_required;
            mark_dirty();
          }
        }
      });

  // Quality
  if (supplier) {
    text(label_x, y_quality, "quality:");
    text(quality_x, y_quality, compat::format("{}", supplier->quality()),
         TextAnchor::End);
  }

  // Remaining term: number then "day" or "days"
  text(label_x, y_remaining, "remaining term:");
  text(quality_x, y_remaining, compat::format("{}", c.remaining_term),
       TextAnchor::End);
  text(days_x, y_remaining, c.remaining_term == 1 ? "day" : "days");
}

void Contracts::flip_page() {
  show_back_ = !show_back_;
  mark_dirty();
}

void Contracts::render_back(const RenderContext& ctx) {
  const size_t contract_idx =
      contract_indices_[static_cast<size_t>(current_pos_)];
  const ActiveContract& c = state_.active_player()->contracts()[contract_idx];

  namespace LC = LegacyConstants;
  constexpr int heading_x = 124 - LC::WOOD_BORDER_WIDTH_LEFT;
  constexpr int heading_y = 70 - LC::WOOD_BORDER_HEIGHT_TOP;
  constexpr int ingredient_x = 84 - LC::WOOD_BORDER_WIDTH_LEFT;
  constexpr int kg_num_x = 268 - LC::WOOD_BORDER_WIDTH_LEFT;
  constexpr int kg_label_x = 272 - LC::WOOD_BORDER_WIDTH_LEFT;
  constexpr int start_y = 96 - LC::WOOD_BORDER_HEIGHT_TOP;
  constexpr int ingredient_line_h = 12;

  // Staircase "stacked pages" decoration at the bottom of the panel.
  // Three filled rectangles at palette color 172, decreasing in size
  // left-to-right.
  constexpr gfx::ColorIndex page_color{172};
  ctx.surface->fill_rect({90 - LC::WOOD_BORDER_WIDTH_LEFT,
                          286 - LC::WOOD_BORDER_HEIGHT_TOP, 96, 74},
                         page_color);
  ctx.surface->fill_rect({186 - LC::WOOD_BORDER_WIDTH_LEFT,
                          290 - LC::WOOD_BORDER_HEIGHT_TOP, 90, 34},
                         page_color);
  ctx.surface->fill_rect({276 - LC::WOOD_BORDER_WIDTH_LEFT,
                          300 - LC::WOOD_BORDER_HEIGHT_TOP, 20, 18},
                         page_color);

  auto text = [&](int x, int row_y, const std::string& s,
                  TextAnchor align = TextAnchor::Start) {
    context_.font->render_text_onto(*ctx.surface, s, ViewPoint{x, row_y},
                                    FontFace::CSTANDAR, 8, align);
  };

  context_.font->render_text_onto(
      *ctx.surface, "delivery amounts:", ViewPoint{heading_x, heading_y},
      FontFace::PIZ_NUMB);

  int y = start_y;
  for (const auto& item : c.items) {
    if (item.kg_per_day <= 0) {
      continue;
    }
    auto* ingredient =
        IngredientFactory::create_ingredient(static_cast<int>(item.type));
    std::string name = ingredient ? ingredient->name() : "?";
    text(ingredient_x, y, name);
    text(kg_num_x, y, compat::format("{}", item.kg_per_day), TextAnchor::End);
    text(kg_label_x, y, "kg");
    y += ingredient_line_h;
  }
}

void Contracts::cancel_current() {
  if (contract_indices_.empty()) {
    return;
  }
  size_t contract_idx = contract_indices_[static_cast<size_t>(current_pos_)];
  state_.active_player()->cancel_contract(state_, contract_idx);
  update_contract_list();
  show_back_ = false;
  mark_dirty();
}

void Contracts::set_up_input_panel() {

  input_panel_->add_button(Icon::ARROW_UP, ButtonType::Horizontal,
                           ButtonPosition::TopLeft, "Turn page", [this] {
                             if (!contract_indices_.empty()) {
                               current_pos_ = std::max(current_pos_ - 1, 0);
                               mark_dirty();
                             }
                           });

  input_panel_->add_button(
      Icon::ARROW_DOWN, ButtonType::Horizontal, ButtonPosition::MiddleLeft,
      "Next", [this] {
        if (!contract_indices_.empty()) {
          current_pos_ = std::min(
              current_pos_ + 1, static_cast<int>(contract_indices_.size()) - 1);
          mark_dirty();
        }
      });

  input_panel_->add_button(Icon::LEFT_RIGHT_ARROW, ButtonType::Vertical,
                           ButtonPosition::TopRight, "Flip page",
                           [this] { flip_page(); });

  input_panel_->add_button(Icon::THUMBS_DOWN, ButtonType::Horizontal,
                           ButtonPosition::BottomLeft, "Cancel contract",
                           [this] { cancel_current(); });

  input_panel_->add_button(Icon::ARROW_BACK, ButtonType::Square,
                           ButtonPosition::BottomRight, "Back", [this] {
                             state_.advance_time_blocks(1);
                             state_.ui().pop();
                           });
}

void Contracts::navigate_to_restaurant(core::RestaurantId new_id) {
  state_.active_restaurant(new_id);
  state_.ui().replace_top<ContractsViewState>();
}
