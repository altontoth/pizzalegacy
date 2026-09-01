/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/restaurant/RestaurantMenu.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "core/city/Property.h"
#include "core/finance/Money.h"
#include "core/restaurant/Menu.h"
#include "core/restaurant/PizzaSalesAggregator.h"
#include "core/state/GameState.h"
#include "engine/FormatCompat.h"
#include "engine/input/InputManager.h"
#include "engine/ui/InteractionEvent.h"
#include "engine/ui/TextAnchor.h"
#include "gfx/Blit.h"
#include "resources/BlitmapManager.h"
#include "resources/FontManager.h"
#include "views/ui/DemographicRenderer.h"
#include "views/ui/PopupManager.h"
#include "views/ui/popups/AlertPopup.h"

RestaurantMenu::RestaurantMenu(GameContext& context, GameState& state,
                               core::RestaurantId id)
    : MainPanelView(context, state), id_(id),
      restaurant_(state.restaurant(id)) {
  input_ = context.input->register_listener(*this);
  set_up_buttons();
}

void RestaurantMenu::tick() {}

void RestaurantMenu::set_up_buttons() {
  // Menu view button (next to page up)
  input_panel_->add_button(Icon::MENU, ButtonType::Horizontal,
                           ButtonPosition::TopCenter, "Menu", [this] {
                             current_state_ = MenuViewState::MENU;
                             current_page_ = 0;
                             selected_row_.reset();
                             mark_dirty();
                           });

  input_panel_->add_button(Icon::ARROW_UP, ButtonType::Square,
                           ButtonPosition::TopLeft, "Turn page", [this] {
                             if (current_state_ != MenuViewState::MENU &&
                                 current_state_ != MenuViewState::SALES) {
                               return;
                             }
                             const int page_size = 14;
                             const int item_count = static_cast<int>(
                                 restaurant_->menu().entries().size());
                             const int page_count =
                                 (item_count + page_size - 1) / page_size;

                             if (current_page_ < page_count - 1) {
                               current_page_++;
                               mark_dirty();
                             }
                           });

  // Sales view button (next to page down)
  input_panel_->add_button(Icon::CALCULATOR, ButtonType::Square,
                           ButtonPosition::MiddleCenter, "Pizza Sales", [this] {
                             current_state_ = MenuViewState::SALES;
                             current_page_ = 0;
                             selected_row_.reset();
                             mark_dirty();
                           });

  input_panel_->add_button(Icon::ARROW_DOWN, ButtonType::Square,
                           ButtonPosition::MiddleLeft, "Turn page", [this] {
                             if (current_state_ != MenuViewState::MENU &&
                                 current_state_ != MenuViewState::SALES) {
                               return;
                             }
                             if (current_page_ > 0) {
                               current_page_--;
                               mark_dirty();
                             }
                           });

  // Customer ratings button (below sales button)
  input_panel_->add_button(Icon::PODIUM, ButtonType::Square,
                           ButtonPosition::MiddleRight, "Customer Ratings",
                           [this] {
                             current_state_ = MenuViewState::CUSTOMER_RATINGS;
                             current_page_ = 0;
                             selected_row_.reset();
                             mark_dirty();
                           });

  // Back button
  input_panel_->add_button(Icon::ARROW_BACK, ButtonType::Square,
                           ButtonPosition::BottomRight, "Back",
                           [this] { state_.ui().pop(); });

  // Remove pizza button (only works in menu view)
  input_panel_->add_button(
      Icon::THUMBS_DOWN, ButtonType::Horizontal, ButtonPosition::BottomLeft,
      "Remove pizza", [this] {
        if (!selected_row_) {
          context_.popup->origin(ViewPoint{52, 80});
          context_.popup->create<AlertPopup>(
              "To remove a pizza from your menu, you must select it first!");
          return;
        }
        if (restaurant_->count_menu_items() == 1) {
          context_.popup->origin(ViewPoint{52, 80});
          context_.popup->create<AlertPopup>(
              "At least one pizza must remain in your menu.");
          return;
        }
        const auto& rows = restaurant_->menu().entries();
        const auto& entry = rows.at(*selected_row_);
        restaurant_->menu().remove(entry.recipe_id);
        mark_dirty();
        selected_row_.reset();
      });
}

void RestaurantMenu::render(const RenderContext& ctx) {
  interactions_.begin_frame();

  // Dispatch to appropriate view based on current state
  switch (current_state_) {
  case MenuViewState::MENU:
    render_menu_view(ctx);
    break;
  case MenuViewState::SALES:
    render_sales_view(ctx);
    break;
  case MenuViewState::CUSTOMER_RATINGS:
    render_customer_ratings_view(ctx);
    break;
  }
}

void RestaurantMenu::render_menu_view(const RenderContext& ctx) {
  // Background + title
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back2"));
  context_.font->render_line_onto(*ctx.surface, "Menu", ViewPoint{80, 26},
                                  FontFace::RUBY, {.color = 8});

  const ViewPoint nr_pos{54, 60};
  const ViewPoint text_pos{84, 60};
  const ViewRect list_box{54, 58, 260, 214};
  const ViewPoint price_cents_pos{312, 60};
  const ViewPoint price_whole_pos{296, 60};
  const uint8_t text_color = 6;

  const auto& menu = restaurant_->menu();
  const auto& rows = menu.entries();
  const auto& recipes = state_.active_player()->recipes();

  const TextStyle line_style{.color = text_color};
  const TextStyle price_whole_style{.color = text_color,
                                    .align = TextAnchor::End};

  const int line_h = 14;

  // Pagination
  const int available_h = list_box.h;
  constexpr int page_size = 14;
  const int page_count =
      (static_cast<int>(rows.size()) + page_size - 1) / page_size;

  // Clamp page & selection
  const int last_page = (page_count > 0) ? (page_count - 1) : 0;
  current_page_ = std::clamp(current_page_, 0, last_page);

  const int start_idx = current_page_ * page_size;
  const int end_idx =
      std::min<int>(start_idx + page_size, static_cast<int>(rows.size()));
  const int sel_row_width = 216;
  const int price_box_width = 70;
  const int sel_row_height = 16;

  // Draw rows
  int y = text_pos.y;
  for (int i = start_idx; i < end_idx; ++i, y += line_h) {
    const auto& row = rows[i];
    const auto* recipe = recipes.find(row.recipe_id);
    // skip missing recipes
    if (!recipe) {
      continue;
    }

    // Selection background (full row width, behind all row text)
    const int pad_y = 1;
    const ViewRect sel_band{list_box.x, y - pad_y, sel_row_width,
                            sel_row_height};
    if (selected_row_ && i == *selected_row_) {
      ctx.surface->fill_rect(sel_band, 14);
    }
    interactions_.create_frame_item(
        sel_band, std::string(recipe->display_name()) + "-" + std::to_string(i),
        [this, i](const views::ui::InteractionEvent& ie) {
          if (ie.type != views::ui::InteractionEvent::Type::Click ||
              ie.button != views::ui::MouseButton::Left) {
            return;
          }
          selected_row_ = i;
          mark_dirty();
        });

    // Number (1-based)
    context_.font->render_line_onto(*ctx.surface, std::to_string(i + 1),
                                    ViewPoint{nr_pos.x, y}, FontFace::CSTANDAR,
                                    line_style);

    // Name
    context_.font->render_line_onto(*ctx.surface, recipe->display_name(),
                                    ViewPoint{text_pos.x, y},
                                    FontFace::CSTANDAR, line_style);

    // Price (whole . cents)
    const auto price = row.price;
    const std::string cents = compat::format("{:02}", price.fractional());

    const ViewRect price_box{list_box.x + sel_row_width, y - pad_y,
                             price_box_width, sel_row_height};
    interactions_.create_frame_item(
        price_box,
        std::string(recipe->display_name()) + "-" + std::to_string(i) +
            "-price",
        [this, i](const views::ui::InteractionEvent& ie) {
          if (ie.type != views::ui::InteractionEvent::Type::Click ||
              ie.button != views::ui::MouseButton::Left) {
            return;
          }
          begin_price_edit(i);
        });

    if (editable_price_row_ && i == *editable_price_row_) {
      constexpr uint8_t edit_bg = 15;
      ctx.surface->fill_rect(price_box, edit_bg);
      context_.font->render_line_onto(*ctx.surface, new_price_ + "_",
                                      ViewPoint{price_box.x + 2, y},
                                      FontFace::CSTANDAR, line_style);
    }
    else {
      context_.font->render_line_onto(*ctx.surface,
                                      std::to_string(price.whole()) + ".",
                                      ViewPoint{ctx.surface->width() - 78, y},
                                      FontFace::CSTANDAR, price_whole_style);
      context_.font->render_line_onto(*ctx.surface, cents,
                                      ViewPoint{price_cents_pos.x, y},
                                      FontFace::CSTANDAR, line_style);
    }
  }

  const std::string page_text = "- " + std::to_string(current_page_ + 1) + " -";
  const TextStyle page_style{.color = 11, .align = TextAnchor::Middle};

  const int page_y = list_box.y + list_box.h + 4;
  const int page_center_x = ctx.surface->width() / 2;
  context_.font->render_line_onto(*ctx.surface, page_text,
                                  ViewPoint{page_center_x, page_y},
                                  FontFace::PIZ_NUMB, page_style);
}

void RestaurantMenu::render_sales_view(const RenderContext& ctx) {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back9"));

  context_.font->render_line_onto(*ctx.surface, "Sales Figures",
                                  ViewPoint{42, 22}, FontFace::RUBY,
                                  {.color = 240});

  context_.font->render_line_onto(*ctx.surface, "sales", ViewPoint{256, 44},
                                  FontFace::SUPERSMA, {.color = 8});

  context_.font->render_into_rect(*ctx.surface, "turn over",
                                  ViewRect{312, 32, 40, 34}, FontFace::SUPERSMA,
                                  {.color = 8});

  constexpr int position_x = 34;
  constexpr int pizza_name_x = 50;
  constexpr int price_x = 250;
  constexpr int sales_x = 288;
  constexpr int turnover_x = 356;
  constexpr int start_y = 58;
  constexpr int row_h = 16;
  constexpr int page_size = 14;
  constexpr int row_text_color = 8;
  constexpr FontFace row_font = FontFace::SUPERSMA;

  const auto pizza_sales = core::restaurant::aggregate_weekly_pizza_sales(
      *restaurant_, state_.time().current_date());
  const auto& recipes = state_.active_player()->recipes();
  const auto& menu_entries = restaurant_->menu().entries();

  const int item_count = static_cast<int>(pizza_sales.size());
  const int page_count = (item_count + page_size - 1) / page_size;
  const int last_page = (page_count > 0) ? (page_count - 1) : 0;
  current_page_ = std::clamp(current_page_, 0, last_page);

  const int start_idx = current_page_ * page_size;
  const int end_idx = std::min(start_idx + page_size, item_count);

  int total_sales = 0;
  Money total_turnover{0};

  int y = start_y;
  for (int i = start_idx; i < end_idx; ++i, y += row_h) {
    const auto& [recipe_id, sales] = pizza_sales[i];
    const auto& menu_entry = menu_entries[i];
    assert(menu_entry.recipe_id == recipe_id);
    const auto* recipe = recipes.find(recipe_id);

    context_.font->render_line_onto(
        *ctx.surface, std::to_string(i + 1), ViewPoint{position_x, y},
        FontFace::CSTANDAR,
        {.color = row_text_color, .align = TextAnchor::End});

    if (recipe) {
      context_.font->render_line_onto(*ctx.surface, recipe->display_name(),
                                      ViewPoint{pizza_name_x, y}, row_font,
                                      {.color = row_text_color});
    }

    context_.font->render_line_onto(
        *ctx.surface, compat::format("{:n}", menu_entry.price),
        ViewPoint{price_x, y}, row_font,
        {.color = row_text_color, .align = TextAnchor::End});

    context_.font->render_line_onto(
        *ctx.surface, std::to_string(sales.count_sold), ViewPoint{sales_x, y},
        row_font, {.color = row_text_color, .align = TextAnchor::End});

    context_.font->render_line_onto(
        *ctx.surface, compat::format("{:n}", sales.revenue),
        ViewPoint{turnover_x, y}, row_font,
        {.color = row_text_color, .align = TextAnchor::End});

    total_sales += sales.count_sold;
    total_turnover = total_turnover + sales.revenue;
  }

  constexpr int total_y = 290;

  context_.font->render_line_onto(
      *ctx.surface, std::to_string(total_sales), ViewPoint{sales_x, total_y},
      row_font, {.color = row_text_color, .align = TextAnchor::End});

  context_.font->render_line_onto(
      *ctx.surface, compat::format("{:n}", total_turnover),
      ViewPoint{turnover_x, total_y}, row_font,
      {.color = row_text_color, .align = TextAnchor::End});

  const std::string page_text = "- " + std::to_string(current_page_ + 1) + " -";
  context_.font->render_line_onto(
      *ctx.surface, page_text, ViewPoint{ctx.surface->width() / 2, 316},
      FontFace::PIZ_NUMB, {.color = 8, .align = TextAnchor::Middle});
}

void RestaurantMenu::begin_price_edit(int row) {
  end_price_edit();
  editable_price_row_ = row;
  new_price_.clear();
  text_session_ = context_.input->start_text_input(*this);
  mark_dirty();
}

void RestaurantMenu::end_price_edit() {
  editable_price_row_.reset();
  new_price_.clear();
  text_session_.reset();
  mark_dirty();
}

void RestaurantMenu::commit_price_edit() {
  if (editable_price_row_ && !new_price_.empty()) {
    const auto& entries = restaurant_->menu().entries();
    const auto recipe_id = entries[*editable_price_row_].recipe_id;
    constexpr double price_min = 1.0;
    constexpr double price_cap = 50.0;
    char* end = nullptr;
    double val = std::strtod(new_price_.c_str(), &end);
    if (end != new_price_.c_str()) {
      val = std::clamp(val, price_min, price_cap);
      Money price{static_cast<int64_t>(std::round(val * 100))};
      restaurant_->menu().set_price(recipe_id, price);
    }
  }
  end_price_edit();
}

bool RestaurantMenu::on_text(const input::TextEvent& ev) {
  if (!editable_price_row_) {
    return false;
  }
  for (char c : ev.text) {
    if (c == ',') {
      c = '.';
    }
    if (std::isdigit(static_cast<unsigned char>(c))) {
      if (new_price_.size() < max_price_input_length) {
        new_price_.push_back(c);
      }
    }
    else if (c == '.') {
      if (new_price_.find('.') == std::string::npos &&
          new_price_.size() < max_price_input_length) {
        new_price_.push_back(c);
      }
    }
  }
  mark_dirty();
  return true;
}

bool RestaurantMenu::on_action(const input::ActionEvent& ev) {
  if (!editable_price_row_ || !ev.state.pressed) {
    return false;
  }
  switch (ev.action) {
  case input::Action::SubmitInput:
    commit_price_edit();
    return true;
  case input::Action::CancelInput:
    end_price_edit();
    return true;
  case input::Action::Backspace:
    if (!new_price_.empty()) {
      new_price_.pop_back();
      mark_dirty();
    }
    return true;
  case input::Action::EraseLine:
    new_price_.clear();
    mark_dirty();
    return true;
  default:
    return false;
  }
}

void RestaurantMenu::render_customer_ratings_view(const RenderContext& ctx) {
  // Background
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back9"));
  ctx.surface->palette({7});

  // Ensure demographics are calculated for the property
  // This populates potential_customers_ which is needed for accurate
  // calculations
  restaurant_->property().calculate_demographics(restaurant_);

  // Get the menu popularity from the restaurant
  const auto& recipes = state_.active_player()->recipes();
  const auto popularity = restaurant_->calculate_menu_popularity(recipes);

  // Render demographics with title
  views::ui::render_demographics(*ctx.surface, *context_.font,
                                 context_.blitmap->get2(), popularity, "Menu");
}
