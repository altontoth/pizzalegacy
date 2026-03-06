/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/restaurant/RestaurantMenu.h"

#include <algorithm>

#include "core/city/Property.h"
#include "core/state/GameState.h"
#include "engine/ui/InteractionEvent.h"
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

  // Page up button (only active in menu view)
  input_panel_->add_button(
      Icon::ARROW_UP, ButtonType::Square, ButtonPosition::TopLeft, "Turn page",
      [this] {
        if (current_state_ != MenuViewState::MENU) {
          return; // No-op if not in menu view
        }
        const auto& menu = restaurant_->menu();
        const auto& rows = menu.entries();
        const int page_size = 14;
        const int page_count =
            (static_cast<int>(rows.size()) + page_size - 1) / page_size;

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

  // Page down button (only active in menu view)
  input_panel_->add_button(Icon::ARROW_DOWN, ButtonType::Square,
                           ButtonPosition::MiddleLeft, "Turn page", [this] {
                             if (current_state_ != MenuViewState::MENU) {
                               return; // No-op if not in menu view
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
              "To remove a pizza from|your menu, you must|select it first!");
          return;
        }
        else if (restaurant_->count_menu_items() == 1) {
          context_.popup->origin(ViewPoint{52, 80});
          context_.popup->create<AlertPopup>(
              "At least one pizza must|remain in your menu.");
          return;
        }
        else {
          const auto& rows = restaurant_->menu().entries();
          const auto& entry = rows.at(*selected_row_);
          restaurant_->menu().remove(entry.recipe_id);
          mark_dirty();
          selected_row_.reset();
        }
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
                                    .align = AlignX::Right};

  const int line_h = 14;

  // Pagination
  const int available_h = list_box.h;
  const int page_size = 14;
  const int page_count =
      (static_cast<int>(rows.size()) + page_size - 1) / page_size;

  // Clamp page & selection
  const int last_page = (page_count > 0) ? (page_count - 1) : 0;
  current_page_ = std::clamp(current_page_, 0, last_page);

  const int start_idx = current_page_ * page_size;
  const int end_idx = std::min<int>(start_idx + page_size, rows.size());

  // Draw rows
  int y = text_pos.y;
  for (int i = start_idx; i < end_idx; ++i, y += line_h) {
    const auto& row = rows[i];
    const auto* recipe = recipes.find(row.recipe_id);
    // Defensive: skip missing recipes
    if (!recipe) {
      continue;
    }

    // Selection background (full row width, behind all row text)
    // Fill a band across the list area; leave a small vertical margin so it
    // looks like the original.
    const int pad_y = 1;
    const ViewRect sel_band{list_box.x, // start at number column
                            y - pad_y, 216, 16};
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
    // cents (always two digits)
    auto cents = std::to_string(std::abs(price.fractional()));
    if (cents.size() == 1) {
      cents.insert(cents.begin(), '0');
    }

    context_.font->render_line_onto(
        *ctx.surface, std::to_string(price.whole()) + ".", ViewPoint{-78, y},
        FontFace::CSTANDAR, price_whole_style);
    context_.font->render_line_onto(*ctx.surface, cents,
                                    ViewPoint{price_cents_pos.x, y},
                                    FontFace::CSTANDAR, line_style);
  }

  // Page indicator centered at the bottom ("-N-", 1-based)
  const std::string page_text = "- " + std::to_string(current_page_ + 1) + " -";
  const TextStyle page_style{.color = 11, .align = AlignX::Center};

  // Center within the list_box horizontally; place just below the list area.
  const int page_y =
      list_box.y + list_box.h + 4; // slight offset like the original
  const int page_center_x = 0;
  context_.font->render_line_onto(*ctx.surface, page_text,
                                  ViewPoint{page_center_x, page_y},
                                  FontFace::PIZ_NUMB, page_style);
}

void RestaurantMenu::render_sales_view(const RenderContext& ctx) {
  // Background
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back2"));

  // Title
  context_.font->render_line_onto(*ctx.surface, "Pizza Sales",
                                  ViewPoint{80, 26}, FontFace::RUBY,
                                  {.color = 8});

  // TODO: Implement pizza sales statistics display
  // For now, just show a placeholder message
  context_.font->render_line_onto(
      *ctx.surface, "Sales statistics coming soon...", ViewPoint{100, 150},
      FontFace::CSTANDAR, {.color = 6});
}

void RestaurantMenu::render_customer_ratings_view(const RenderContext& ctx) {
  // Background
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back9"));
  ctx.surface->palette({7});

  // Ensure demographics are calculated for the property
  // This populates potential_customers_ which is needed for accurate
  // calculations
  if (restaurant_->property()) {
    restaurant_->property()->calculate_demographics(restaurant_);
  }

  // Get the menu popularity from the restaurant
  const auto& recipes = state_.active_player()->recipes();
  const auto popularity = restaurant_->calculate_menu_popularity(recipes);

  // Render demographics with title
  views::ui::render_demographics(*ctx.surface, *context_.font,
                                 context_.blitmap->get2(), popularity, "Menu");
}
