/*
 * SPDX-FileCopyrightText: 2016 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/purchasing/CategoryOverviewScreen.h"

#include "core/character/Player.h"
#include "core/finance/Money.h"
#include "core/restaurant/Furniture.h"
#include "core/restaurant/PlacedFurniture.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "core/util/LegacyConstants.h"
#include "engine/FormatCompat.h"
#include "engine/geom/Spaces.h"
#include "engine/ui/InteractionEvent.h"
#include "gfx/Blit.h"
#include "resources/BlitmapManager.h"
#include "resources/FontManager.h"
#include "views/ui/PopupManager.h"
#include "views/ui/popups/AlertPopup.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ranges>

CategoryOverviewScreen::CategoryOverviewScreen(GameContext& context,
                                               GameState& state,
                                               SupplierType stype)
    : PurchasingMain(context, state), supplier_type_(stype),
      status_controller_(state.status()) {

  category_offsets[SupplierType::FURNITURE] = {132, 168, 52, 44};
  category_offsets[SupplierType::KITCHENWARE] = {132 + 52, 168, 58, 44};
  category_offsets[SupplierType::EXTRAS] = {132 + 52 + 60, 168, 56, 42};
  category_offsets[SupplierType::FLOORING] = {64, 168, 66, 42};
  category_titles[SupplierType::FURNITURE] = "equipment";
  category_titles[SupplierType::KITCHENWARE] = "kitchenware";
  category_titles[SupplierType::EXTRAS] = "extras";
  category_titles[SupplierType::FLOORING] = "floor covering";

  set_up_input_panel();
  status_controller_.show_restaurant({.state = &state_,
                                      .rest = state_.active_restaurant(),
                                      .label = "EQUIPMENT"});
}

void CategoryOverviewScreen::tick() {
  // When scrolling is active, update the offset:
  if (scroll_speed_ != 0) {
    if (scroll_offset_ + scroll_speed_ <= max_scroll_height_ &&
        scroll_offset_ + scroll_speed_ >= 0) {
      scroll_offset_ += scroll_speed_;
    }
    if (!scroll_active_ && scroll_offset_ % line_height_ == 0) {
      scroll_speed_ = 0;
    }
    mark_dirty();
  }
}

void CategoryOverviewScreen::render(const RenderContext& ctx) {
  interactions_.begin_frame();
  ctx.surface->palette({60});
  draw_page_header(ctx);
  draw_page_screen(ctx);
}

void CategoryOverviewScreen::draw_icon(const RenderContext& ctx) {
  auto& blitmap1 = context_.blitmap->get<Blitmap1Icon>();
  Blitmap1Icon icon = Blitmap1Icon::END_OF_ICONS;
  switch (this->supplier_type()) {
  case SupplierType::FLOORING:
    icon = Blitmap1Icon::SHOE;
    break;
  case SupplierType::FURNITURE:
    icon = Blitmap1Icon::CHAIR;
    break;
  case SupplierType::KITCHENWARE:
    icon = Blitmap1Icon::POT;
    break;
  case SupplierType::EXTRAS:
    icon = Blitmap1Icon::PLATE;
    break;
  default:
    Logger::debug("No icon for {}", static_cast<int>(this->supplier_type()));
    break;
  }
  blitmap1.render_icon(icon, *ctx.surface, ViewPoint{42, 6});
}

void CategoryOverviewScreen::draw_page_header(const RenderContext& ctx) {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back9"));
  draw_icon(ctx);

  context_.font->render_text_onto(*ctx.surface,
                                  category_titles[this->supplier_type()],
                                  ViewPoint{134, 18}, FontFace::ASTANDAR, 8);
}

void CategoryOverviewScreen::draw_page_screen(const RenderContext& ctx) {
  bool should_draw_inventory_box = false;
  if (this->supplier_type() == SupplierType::FLOORING) {
    draw_flooring_page(ctx);
  }
  else if (this->supplier_type() == SupplierType::FURNITURE) {
    draw_furniture_page(ctx);
    should_draw_inventory_box = true;
  }
  else if (this->supplier_type() == SupplierType::KITCHENWARE) {
    draw_oven_page(ctx);
    should_draw_inventory_box = true;
  }
  else if (this->supplier_type() == SupplierType::EXTRAS) {
    draw_extras_page(ctx);
    should_draw_inventory_box = true;
  }
  if (should_draw_inventory_box) {
    draw_inventory_box(ctx);
  }
}

void CategoryOverviewScreen::draw_info_section(
    const RenderContext& ctx, const std::vector<InfoLine>& lines) {
  ViewPoint base{30, 86};
  constexpr int line_height = 20;
  constexpr uint8_t text_black = 8;

  for (const auto& line : lines) {
    ViewPoint pos = base;
    if (line.label) {
      context_.font->render_text_onto(*ctx.surface, *line.label, pos,
                                      FontFace::PIZ_NUMB, text_black);
    }
    pos.x = ctx.surface->width() - line.value.right_inset;
    context_.font->render_text_onto(*ctx.surface, line.value.text, pos,
                                    FontFace::PIZ_NUMB, text_black,
                                    TextAnchor::End);

    if (line.post_fix) {
      pos.x = line.post_fix->x;
      context_.font->render_text_onto(*ctx.surface, line.post_fix->text, pos,
                                      FontFace::PIZ_NUMB, text_black);
    }

    base.y += line_height;
  }
}

void CategoryOverviewScreen::draw_flooring_page(const RenderContext& ctx) {
  auto* restaurant = state_.active_restaurant();
  const auto& rl = restaurant->layout();
  auto fp = rl->floor_pattern();
  auto rest_size = restaurant->size();
  const int rest_width = rest_size.w;
  const int rest_height = rest_size.h;
  const int tile_count = rest_width * rest_height;

  // Show current flooring:
  int x_pos = 36;
  int y_pos = 80;
  context_.font->render_text_onto(
      *ctx.surface, "value:", ViewPoint{x_pos, y_pos}, FontFace::PIZ_NUMB, 8);
  y_pos += 20;
  context_.font->render_text_onto(
      *ctx.surface, "quality:", ViewPoint{x_pos, y_pos}, FontFace::PIZ_NUMB, 8);
  y_pos += 20;
  context_.font->render_text_onto(*ctx.surface,
                                  "number of tiles:", ViewPoint{x_pos, y_pos},
                                  FontFace::PIZ_NUMB, 8);
  y_pos += 20;
  context_.font->render_text_onto(
      *ctx.surface, "area:", ViewPoint{x_pos, y_pos}, FontFace::PIZ_NUMB, 8);

  x_pos = 180;
  y_pos = 80;
  // Value: total cost of installed floor
  const auto total_value = fp.price_per_tile * tile_count;
  context_.font->render_text_onto(
      *ctx.surface, compat::format("{:w}", total_value),
      ViewPoint{x_pos, y_pos}, FontFace::PIZ_NUMB, 8, TextAnchor::Start);
  y_pos += 20;
  // Quality from supplier
  context_.font->render_text_onto(*ctx.surface, std::to_string(fp.quality),
                                  ViewPoint{x_pos, y_pos}, FontFace::PIZ_NUMB,
                                  8, TextAnchor::Start);
  y_pos += 20;
  // Number of tiles
  std::string tile_count_str =
      compat::format("{} * {} = {}", rest_width, rest_height, tile_count);
  context_.font->render_text_onto(*ctx.surface, tile_count_str,
                                  ViewPoint{x_pos, y_pos}, FontFace::PIZ_NUMB,
                                  8, TextAnchor::Start);
  y_pos += 20;
  // Area
  std::string area = compat::format(
      "{} sqm", static_cast<int>((rest_width / 2) * (rest_height / 2)));
  context_.font->render_text_onto(*ctx.surface, area,
                                  ViewPoint{x_pos - 80, y_pos},
                                  FontFace::PIZ_NUMB, 8, TextAnchor::Start);

  constexpr int preview_cols = 8;
  constexpr int preview_rows = 4;
  const auto preview = create_floor_preview(fp, preview_cols, preview_rows);
  const ViewPoint preview_pos{74, 178};
  ctx.surface->blit_raw(preview, preview_pos);

  if (state_.floor_editing_unlocked()) {
    interactions_.create_frame_item(
        ViewRect{preview_pos.x, preview_pos.y, preview.width(),
                 preview.height()},
        "floor_pattern_edit", [this](views::ui::InteractionEvent ie) {
          if (ie.type != views::ui::InteractionEvent::Type::ButtonDown ||
              ie.button != views::ui::MouseButton::Left) {
            return;
          }
          const int col = ie.local.x / LegacyConstants::RESTAURANT_TILE_WIDTH;
          const int row = ie.local.y / LegacyConstants::RESTAURANT_TILE_HEIGHT;
          cycle_floor_tile(((row % 2) * 2) + (col % 2));
        });
  }
}

void CategoryOverviewScreen::cycle_floor_tile(int quadrant) {
  // Every set holds eight tiles, and each quadrant of the pattern steps
  // through them independently.
  constexpr int tiles_per_set = 8;

  auto* restaurant = state_.active_restaurant();
  const auto& layout = restaurant->layout();
  auto fp = layout->floor_pattern();
  auto& tile = fp.tile_indices[static_cast<std::size_t>(quadrant)];
  tile = static_cast<uint8_t>((tile + 1) % tiles_per_set);
  layout->floor_pattern(fp);
  mark_dirty();
}

void CategoryOverviewScreen::draw_furniture_page(const RenderContext& ctx) {
  auto* restaurant = state_.active_restaurant();
  auto stats = restaurant->calculate_furniture_stats();

  // Build all lines as data
  std::vector<InfoLine> lines = {
      {.label = "in use:",
       .value = InfoValue{.text = std::to_string(stats.seating_utilization),
                          .right_inset = 180},
       .post_fix = InfoPostFix{.text = "%", .x = 226}},
      {.label = "value:",
       .value = InfoValue{.text = compat::format("{:i}", stats.total_value),
                          .right_inset = 180},
       .post_fix = std::nullopt},
      {.label = "quality:",
       .value = InfoValue{.text = std::to_string(stats.avg_quality),
                          .right_inset = 180}},
      {.label = "capacity:",
       .value = InfoValue{.text = std::to_string(stats.seat_count),
                          .right_inset = 180},
       .post_fix = InfoPostFix{.text = "seats", .x = 274}},
      {.label = std::nullopt,
       .value = InfoValue{.text = std::to_string(stats.customers_per_day),
                          .right_inset = 180},
       .post_fix = InfoPostFix{.text = "per day", .x = 274}},
  };
  draw_info_section(ctx, lines);
}

void CategoryOverviewScreen::draw_oven_page(const RenderContext& ctx) {
  auto* restaurant = state_.active_restaurant();
  auto stats = restaurant->calculate_kitchen_equipment_stats();

  auto text_box = Surface(324, 168, PaletteHandle{60});
  uint8_t text_black = 8;
  ViewPoint pos;

  /* First the labels */
  context_.font->render_text_onto(text_box, "ovens:", pos, FontFace::PIZ_NUMB,
                                  text_black);
  pos.y += 20;
  context_.font->render_text_onto(text_box, "bake time/pizza:", pos,
                                  FontFace::PIZ_NUMB, text_black);
  pos.y += 20;
  context_.font->render_text_onto(text_box, "in use:", pos, FontFace::PIZ_NUMB,
                                  text_black);
  pos.y += 20;
  context_.font->render_text_onto(text_box, "value:", pos, FontFace::PIZ_NUMB,
                                  text_black);
  pos.y += 20;
  context_.font->render_text_onto(text_box, "quality:", pos, FontFace::PIZ_NUMB,
                                  text_black);
  pos.y += 20;
  context_.font->render_text_onto(text_box, "baking capacity:", pos,
                                  FontFace::PIZ_NUMB, text_black);

  /* Then the values */
  pos.y = 0;
  pos.x = text_box.width() - 100;

  // ovens count
  context_.font->render_text_onto(text_box, std::to_string(stats.oven_count),
                                  pos, FontFace::PIZ_NUMB, text_black,
                                  TextAnchor::End);
  pos.y += 20;

  // bake time/pizza
  context_.font->render_text_onto(
      text_box, compat::format("{:.2f}", stats.avg_bake_time), pos,
      FontFace::PIZ_NUMB, text_black, TextAnchor::End);
  pos.y += 20;

  // in use %
  context_.font->render_text_onto(
      text_box, std::to_string(stats.utilization_percent), pos,
      FontFace::PIZ_NUMB, text_black, TextAnchor::End);
  pos.y += 20;

  // value
  context_.font->render_text_onto(
      text_box, compat::format("{:i}", stats.total_value), pos,
      FontFace::PIZ_NUMB, text_black, TextAnchor::End);
  pos.y += 20;

  // quality
  context_.font->render_text_onto(text_box, std::to_string(stats.avg_quality),
                                  pos, FontFace::PIZ_NUMB, text_black,
                                  TextAnchor::End);
  pos.y += 20;

  // baking capacity - number
  context_.font->render_text_onto(
      text_box, std::to_string(stats.total_capacity), pos, FontFace::PIZ_NUMB,
      text_black, TextAnchor::End);
  pos.y += 20;

  // baking capacity - per day
  context_.font->render_text_onto(
      text_box, std::to_string(stats.capacity_per_day), pos, FontFace::PIZ_NUMB,
      text_black, TextAnchor::End);

  /* Then the labels after the values */
  pos.y = 20;
  pos.x = 236;
  context_.font->render_text_onto(text_box, "minutes", pos, FontFace::PIZ_NUMB,
                                  text_black);
  pos.y += 20;
  context_.font->render_text_onto(text_box, "%", pos, FontFace::PIZ_NUMB,
                                  text_black);
  pos.y += 60;
  pos.x += 8;
  context_.font->render_text_onto(text_box, "number", pos, FontFace::PIZ_NUMB,
                                  text_black);
  pos.y += 20;
  context_.font->render_text_onto(text_box, "per day", pos, FontFace::PIZ_NUMB,
                                  text_black);

  ctx.surface->blit_masked(text_box, ViewPoint{30, 46});
}

template <typename Container>
auto filter_furniture_view(
    const Container& items,
    std::initializer_list<FurnitureCategory> allowed_list) {
  // Copy to a vector to ensure safe lifetime
  auto allowed = std::vector<FurnitureCategory>(allowed_list);
  return items |
         std::views::filter([allowed = std::move(allowed)](const auto& pf) {
           auto cat = pf->furniture()->category();
           return std::ranges::find(allowed, cat) != allowed.end();
         });
}

void CategoryOverviewScreen::draw_inventory_box(const RenderContext& ctx) {
  auto* restaurant = state_.active_restaurant();
  std::map<std::string, int> furniture_counter;

  auto supplier = this->supplier_type();
  auto& layout = *restaurant->layout();

  auto furniture_view = [&]() {
    switch (supplier) {
    case SupplierType::FURNITURE:
      return filter_furniture_view(
          layout.placed_furniture(),
          {FurnitureCategory::TABLE, FurnitureCategory::CHAIR});
    case SupplierType::KITCHENWARE:
      return filter_furniture_view(layout.placed_kitchen_furniture(),
                                   {FurnitureCategory::KITCHENWARE});
    case SupplierType::EXTRAS:
      return filter_furniture_view(layout.placed_furniture(),
                                   {FurnitureCategory::DECOR,
                                    FurnitureCategory::ENTERTAINMENT,
                                    FurnitureCategory::SLOT_MACHINES});
    default:
      Logger::warn("Called furniture_view with an unknown thing!");
      std::terminate();
    }
  }(); // auto deduces type

  // Count filtered items
  for (const auto& pf : furniture_view) {
    furniture_counter[pf->furniture()->name()]++;
  }

  int box_x = 30;
  int box_y = 204;

  std::vector<std::string> furniture_list;
  furniture_list.reserve(furniture_counter.size());
  for (const auto& [name, count] : furniture_counter) {
    furniture_list.push_back(compat::format("{}    {}", count, name));
  }

  draw_scrollable_text_box(ctx, ViewPoint{box_x, box_y}, furniture_list,
                           scroll_offset_);
}

void CategoryOverviewScreen::draw_extras_page(const RenderContext& ctx) {
  int decor_count = 0;
  int media_count = 0;
  int games_count = 0;
  int others_count = 0;

  auto* restaurant = state_.active_restaurant();
  const auto& layout = restaurant->layout();
  const auto& furniture = layout->placed_furniture();
  for (const auto& item : furniture) {
    switch (item->furniture()->category()) {
    case FurnitureCategory::DECOR:
      decor_count++;
      break;
    case FurnitureCategory::ENTERTAINMENT:
      media_count++;
      break;
    case FurnitureCategory::GAMES:
      games_count++;
      break;
    case FurnitureCategory::SLOT_MACHINES:
      others_count++;
      break;
    case FurnitureCategory::OTHER:
    case FurnitureCategory::TABLE:
    case FurnitureCategory::CHAIR:
    case FurnitureCategory::UNKNOWN:
    case FurnitureCategory::KITCHENWARE:
    case FurnitureCategory::UNKNOWN3:
      break;
    }
  }

  // Build all lines as data
  std::vector<InfoLine> lines = {
      {.label = "decor:",
       .value =
           InfoValue{.text = std::to_string(decor_count), .right_inset = 120}},
      {.label = "entertainment media:",
       .value =
           InfoValue{.text = std::to_string(media_count), .right_inset = 120}},
      {.label = "arcade games:",
       .value =
           InfoValue{.text = std::to_string(games_count), .right_inset = 120}},
      {.label = "others:",
       .value =
           InfoValue{.text = std::to_string(others_count), .right_inset = 120}},
  };
  draw_info_section(ctx, lines);
}

void CategoryOverviewScreen::back_out() {
  state_.advance_time_blocks(1);
  state_.ui().pop();
}

void CategoryOverviewScreen::alert_no_selection() {
  context_.popup->origin(ViewPoint{52, 80});
  context_.popup->create<AlertPopup>(
      "Which items do you want to buy? You have not yet made a selection!",
      [this] { back_out(); });
}

void CategoryOverviewScreen::set_up_input_panel() {
  input_panel_->delete_buttons();

  input_panel_->add_button(Icon::CHAIR, ButtonType::Square,
                           ButtonPosition::TopLeft, "Furniture", [this]() {
                             supplier_type_ = SupplierType::FURNITURE;
                             mark_dirty();
                           });

  input_panel_->add_button(Icon::OVEN, ButtonType::Square,
                           ButtonPosition::TopCenter, "Kitchenware", [this]() {
                             supplier_type_ = SupplierType::KITCHENWARE;
                             mark_dirty();
                           });

  input_panel_->add_button(Icon::PLANT, ButtonType::Square,
                           ButtonPosition::MiddleLeft, "Extras", [this]() {
                             supplier_type_ = SupplierType::EXTRAS;
                             mark_dirty();
                           });

  input_panel_->add_button(Icon::FOOT, ButtonType::Square,
                           ButtonPosition::MiddleCenter, "Flooring", [this]() {
                             supplier_type_ = SupplierType::FLOORING;
                             mark_dirty();
                           });

  input_panel_->add_button(Icon::ARROW_BACK, ButtonType::Square,
                           ButtonPosition::BottomRight, "Back",
                           [this]() { back_out(); });

  input_panel_->add_button(Icon::SHOPPING_CART, ButtonType::Horizontal,
                           ButtonPosition::BottomLeft, "Shop", [this]() {
                             // The visit costs energy whether or
                             // not anything is bought.
                             if (auto* player = state_.active_player()) {
                               player->on_supplier_visit();
                             }
                             state_.ui().push<SupplierListState>(
                                 supplier_type_);
                             mark_dirty();
                           });

  input_panel_->add_button(
      Icon::GRAPH, ButtonType::Vertical, ButtonPosition::TopRight,
      "Customer ratings", [this]() {
        state_.ui().replace_top<PurchasingDemographicState>(supplier_type_);
        mark_dirty();
      });
}

Surface CategoryOverviewScreen::create_floor_preview(
    const FloorPattern& pattern, int width_tiles, int height_tiles) {
  Surface surface(width_tiles * LegacyConstants::RESTAURANT_TILE_WIDTH,
                  height_tiles * LegacyConstants::RESTAURANT_TILE_HEIGHT);
  // Compose the filename using the vendor id.
  std::string k_filename = compat::format("gfx/k{}", pattern.vendor_id + 1);

  // Load the VGA file that holds the tiles.
  const auto& k_tiles = context_.vga->get(k_filename);

  // Each tile in k_tiles is 24x22 pixels.
  constexpr int tile_width = LegacyConstants::RESTAURANT_TILE_WIDTH;
  constexpr int tile_height = LegacyConstants::RESTAURANT_TILE_HEIGHT;
  // k_tiles is 192x44 pixels (8 tiles per row, 2 rows),
  // but we only use the first row (y = 0) for the preview.

  // Our preview box is 192 x 88 pixels:
  // 192 / 24 = 8 columns, 88 / 22 = 4 rows.
  int preview_cols = surface.width() / tile_width;
  int preview_rows = surface.height() / tile_height;

  // We want to lay out the floor pattern in a repeating 2x2 arrangement.
  // That is, if the floor pattern tile indices are {4, 5, 6, 7},
  // then the preview should be:
  // Row 0:  4 5 4 5 4 5 4 5
  // Row 1:  6 7 6 7 6 7 6 7
  // Row 2:  4 5 4 5 4 5 4 5
  // Row 3:  6 7 6 7 6 7 6 7

  for (int row = 0; row < preview_rows; ++row) {
    for (int col = 0; col < preview_cols; ++col) {
      // Determine which quadrant of the pattern we are in:
      // For even rows use pattern[0] for even cols and pattern[1] for odd cols.
      // For odd rows  use pattern[2] for even cols and pattern[3] for odd cols.
      int pattern_index =
          (row % 2 == 0) ? ((col % 2 == 0) ? 0 : 1) : ((col % 2 == 0) ? 2 : 3);
      uint8_t tile_index = pattern.tile_indices[pattern_index];

      // Calculate the source rectangle in k_tiles.
      // Since the indices refer to the first row of k_tiles,
      // the y offset is zero.
      ViewRect src_rect = {tile_index *
                               tile_width, // x: tile index * tile width
                           0,              // y: first row
                           tile_width, tile_height};

      // Calculate the destination rectangle in preview_box.
      ViewPoint dst_pos = {col * tile_width, // x coordinate in the preview box
                           row * tile_height}; // y coordinate in the preview
                                               // box tile_width, tile_height};

      // Blit the chosen tile from k_tiles to preview_box.
      surface.blit(k_tiles, src_rect, dst_pos);
    }
  }
  return surface;
}

// Add this method to CategoryOverviewScreen in CategoryOverviewScreen.cpp

void CategoryOverviewScreen::draw_scrollable_text_box(
    const RenderContext& ctx, ViewPoint p,
    const std::vector<std::string>& lines, int scroll_offset) {
  // This is both padding and line thickness :-/
  const int padding = 2;
  // Overall box size:
  const int box_width = 316;
  const int box_height = 116;

  // The title box at the top:
  const int title_box_width = box_width - (2 * padding);
  const int title_box_height = 20;

  // The inner box where the text goes:
  const int inner_box_width = 296;
  const int inner_box_height = box_height - title_box_height - (3 * padding);

  const int scroll_button_width = box_width - inner_box_width - (3 * padding);
  const int scroll_button_height = ((inner_box_height - padding) / 2);

  std::string title = "num  name";

  // Border color: dark red
  uint8_t border_color = 9;
  //  Title bar and scroll button background
  uint8_t title_bg_color = 13;
  //  Main background for the text list
  uint8_t main_bg_color = 14;
  //  Title text color (same as border)
  uint8_t title_text_color = 10;
  //  List text color:
  uint8_t list_text_color = 11;

  // Fill the entire box with the border color:
  ViewRect outer_rect = {p.x, p.y, box_width, box_height};
  ctx.surface->fill_rect(outer_rect, border_color);

  // Then draw the title box inside:
  ViewRect title_rect = {p.x + padding, p.y + padding, title_box_width,
                         title_box_height};
  ctx.surface->fill_rect(title_rect, title_bg_color);

  // Main box (below title)
  ViewRect main_area_rect = {p.x + padding,
                             p.y + title_box_height + (2 * padding),
                             inner_box_width, inner_box_height};
  ctx.surface->fill_rect(main_area_rect, main_bg_color);

  ViewRect scroll_rect = {p.x + inner_box_width + (2 * padding),
                          p.y + title_box_height + (2 * padding),
                          scroll_button_width, scroll_button_height};

  // Fill scroll button area with the same background as the title bar.
  ctx.surface->fill_rect(scroll_rect, title_bg_color);

  interactions_.create_frame_item(
      scroll_rect, "scroll_up", [this](views::ui::InteractionEvent ui) {
        if (ui.type == views::ui::InteractionEvent::Type::ButtonDown &&
            ui.button == views::ui::MouseButton::Left) {
          scroll_active_ = true;
          scroll_speed_ = -1;
        }
        else if (ui.type == views::ui::InteractionEvent::Type::ButtonUp &&
                 ui.button == views::ui::MouseButton::Left) {
          scroll_active_ = false;
        }
      });

  scroll_rect.y += scroll_rect.h + padding;
  ctx.surface->fill_rect(scroll_rect, title_bg_color);

  interactions_.create_frame_item(
      scroll_rect, "scroll_down", [this](views::ui::InteractionEvent ui) {
        if (ui.type == views::ui::InteractionEvent::Type::ButtonDown &&
            ui.button == views::ui::MouseButton::Left) {
          scroll_active_ = true;
          scroll_speed_ = 1;
        }
        else if (ui.type == views::ui::InteractionEvent::Type::ButtonUp &&
                 ui.button == views::ui::MouseButton::Left) {
          scroll_active_ = false;
        }
      });

  // Render the title text with a bit of padding
  int title_text_x = title_rect.x + 4;
  int title_text_y = title_rect.y + 6;
  context_.font->render_text_onto(*ctx.surface, title,
                                  ViewPoint{title_text_x, title_text_y},
                                  FontFace::CSTANDAR, title_text_color);

  // If we have nothing to draw then we're done here
  if (lines.empty()) {
    return;
  }
  int text_area_x = main_area_rect.x + padding;
  int text_area_y = main_area_rect.y + (3 * padding);

  int text_x = 0;
  int text_y = 0;
  auto text_surface =
      Surface(inner_box_width - (2 * padding),
              static_cast<int>(lines.size() * line_height_) + text_y,
              PaletteHandle{60});

  for (const auto& line : lines) {
    context_.font->render_text_onto(text_surface, line,
                                    ViewPoint{text_x, text_y},
                                    FontFace::CSTANDAR, list_text_color);
    text_y += line_height_;
  }
  // Calculate the visible height (in case the text_surface is shorter than
  // inner_box_height)
  int visible_height = std::min(inner_box_height - (3 * padding),
                                text_surface.height() - scroll_offset);
  max_scroll_height_ =
      std::max(0, text_surface.height() - inner_box_height + (3 * padding));

  // Set up the source rectangle (portion of text_surface to blit)
  ViewRect src_rect = {0, scroll_offset, text_surface.width(), visible_height};

  // Set up the destination rectangle (where on screen to blit the text)
  ViewPoint dst_point = {text_area_x, text_area_y}; //, text_surface->w,
                                                    // visible_height};

  // Blit the visible portion of text_surface onto screen
  ctx.surface->blit(text_surface, src_rect, dst_point);
}

SupplierType CategoryOverviewScreen::supplier_type() const {
  return supplier_type_;
}

void CategoryOverviewScreen::navigate_to_restaurant(core::RestaurantId new_id) {
  state_.active_restaurant(new_id);
  state_.ui().replace_top<CategoryOverviewState>(supplier_type_);
}
