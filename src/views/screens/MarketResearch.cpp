/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/MarketResearch.h"
#include "core/Demographics.h"
#include "core/character/Player.h"
#include "core/city/City.h"
#include "core/city/ControlMap.h"
#include "core/city/District.h"
#include "core/city/PizzaCharts.h"
#include "core/city/Property.h"
#include "core/state/GameState.h"
#include "core/util/Constants.h"
#include "engine/geom/Spaces.h"
#include "engine/ui/Alignment.h"
#include "engine/ui/InteractionEvent.h"
#include "gfx/Blit.h"
#include "gfx/Draw.h"
#include "resources/BlitmapIcons.h"
#include "resources/BlitmapManager.h"
#include "views/base/ViewDefs.h"
#include "views/ui/PageDecoration.h"

#include <algorithm>
#include <cstdlib>
#include <span>

constexpr int text_color = 10;

void MarketResearch::render_bar_chart(
    const RenderContext& ctx, BarChartMode mode,
    const std::array<float, NUMBER_OF_PLAYERS>& values) {
  ctx.surface->fill({15});
  constexpr int edge_offset = 6;
  views::ui::render_decorated_corners(
      *ctx.surface, stanbobs_,
      ViewRect{{edge_offset, edge_offset},
               {views::base::MAIN_PANEL_WIDTH - (edge_offset * 2),
                views::base::MAIN_PANEL_HEIGHT - (edge_offset * 2)}},
      4);

  // Title
  ctx.surface->palette(PaletteHandle{62});
  context_.font->render_line_onto(
      *ctx.surface, "chain of restaurants",
      ViewPoint{views::base::MAIN_PANEL_WIDTH / 2, 14}, FontFace::ASTANDAR,
      text_color, AlignX::CenterPoint);

  // Compute scale and subtitle based on mode
  int scale_unit = 1;
  std::string subtitle;
  if (mode == BarChartMode::Percentage) {
    subtitle = "market shares in %";
  }
  else {
    int max_val =
        static_cast<int>(*std::max_element(values.begin(), values.end()));
    // Original game starts at $10K chunks; qol_improvements allows finer $1K
    const bool fine_scale = context_.config->options().qol_improvements;
    constexpr std::array<int, 5> original_units = {10'000, 100'000, 1'000'000,
                                                   10'000'000, 100'000'000};
    constexpr std::array<int, 6> improved_units = {
        1'000, 10'000, 100'000, 1'000'000, 10'000'000, 100'000'000};
    const auto& units = fine_scale ? std::span<const int>(improved_units)
                                   : std::span<const int>(original_units);
    scale_unit = units.back();
    for (int u : units) {
      if (max_val <= u * 10) {
        scale_unit = u;
        break;
      }
    }
    subtitle = "turnover in $" + std::to_string(scale_unit);
  }

  context_.font->render_line_onto(
      *ctx.surface, subtitle, ViewPoint{views::base::MAIN_PANEL_WIDTH / 2, 32},
      FontFace::ASTANDAR, text_color, AlignX::CenterPoint);

  // Graph area
  constexpr int grid_color = 14;
  constexpr ViewRect graph_box{28, 52, 324, 244};
  gfx::draw::grid(*ctx.surface, graph_box, grid_color, 14);

  constexpr int y_label_offset = 20;
  constexpr ViewPoint label_y_start{28, 284};
  for (int i = 10; i > 0; --i) {
    context_.font->render_line_onto(*ctx.surface, std::to_string(i),
                                    label_y_start -
                                        PxOffset{0, i * y_label_offset},
                                    FontFace::CSTANDAR, 240, AlignX::RightEdge);
  }

  constexpr int graph_bottom = 52 + 244;
  constexpr int bar_area_height = 200;

  // Bar gradient colors from original (each drawn twice for 2x rendering)
  constexpr std::array<uint8_t, 10> bar_colors{0x9, 0xA, 0xB, 0xC, 0xD,
                                               0xE, 0xF, 0xE, 0xC, 0x9};
  constexpr int bar_width = 20;
  constexpr int slot_width = graph_box.w / NUMBER_OF_PLAYERS;
  // Shop frontend tiles: same encoding as City::type() for owned premises
  constexpr uint8_t icon_row = 0xA;
  constexpr int icon_size =
      LegacyConstants::BASE_MAP_TILE_WIDTH * BASE_MAP_ZOOM_MULTIPLIER;

  const auto& landsym = context_.vga->get("gfx/landsym");

  for (int i = 0; i < NUMBER_OF_PLAYERS; ++i) {
    int center_x = graph_box.x + (i * slot_width) + (slot_width / 2);
    // Show a at least a tiny bar by default, so just bump everything by one
    const float value = values[i] + 1.0f;

    // Compute bar height
    int bar_height;
    if (mode == BarChartMode::Percentage) {
      bar_height = static_cast<int>(value * 2.0f);
    }
    else {
      bar_height = static_cast<int>((value * 20.0f) / std::max(scale_unit, 1));
    }
    bar_height = std::clamp(bar_height, 0, bar_area_height);

    // Draw bar: 10 original colors, each drawn twice for 2x rendering
    int bar_x = center_x - (bar_width / 2);
    for (int col = 0; col < 10; ++col) {
      if (bar_height > 0) {
        int x = bar_x + (col * 2);
        gfx::draw::line(*ctx.surface, ViewPoint{x, graph_bottom},
                        ViewPoint{x, graph_bottom - bar_height},
                        bar_colors[col]);
        gfx::draw::line(*ctx.surface, ViewPoint{x + 1, graph_bottom},
                        ViewPoint{x + 1, graph_bottom - bar_height},
                        bar_colors[col]);
      }
    }

    // Restaurant icon below the graph has transparency so needs a background
    int icon_x = center_x - (icon_size / 2);
    int icon_y = graph_bottom + 6;
    ctx.surface->fill_rect(ViewRect{icon_x, icon_y, icon_size, icon_size}, 240);
    uint8_t tile_type = ((i & 0xF) << 4) | icon_row;
    ViewRect src = landsym_source_rect(tile_type, BASE_MAP_ZOOM_MULTIPLIER);
    ctx.surface->blit(landsym, src, ViewPoint{icon_x, icon_y});

    // Hover tooltip showing actual value
    if (context_.config->options().qol_improvements) {
      float display_value = values[i];
      std::string tip;
      if (mode == BarChartMode::Percentage) {
        tip = compat::format("{:.1f}%", display_value);
      }
      else {
        tip = "$" + std::to_string(static_cast<int>(display_value));
      }
      interactions_.create_frame_item(
          {icon_x, icon_y, icon_size, icon_size},
          "bar_chart_" + std::to_string(i),
          [](const views::ui::InteractionEvent&) {},
          [tip](const views::ui::InteractionEvent& ie) {
            return TooltipRequest{
                .text = tip,
                .anchor = TooltipAnchor::Mouse,
                .screenPos = ie.pos,
                .palette = 62,
                .bg_color = 15,
                .text_color = 240,
                .border_color = 240,
                .padding = 4,
            };
          });
    }
  }
}

MarketResearch::MarketResearch(GameContext& context, GameState& state,
                               core::DistrictId district_id)
    : MainPanelView(context, state),
      district_(state_.active_city()->district(district_id)),
      stanbobs_(context_.bob->get("bob/stanbobs.bob")) {

  create_buttons();
}

void MarketResearch::tick() {}

void MarketResearch::render(const RenderContext& ctx) {
  interactions_.begin_frame();
  ctx.surface->palette(PaletteHandle{8});
  uint8_t bg_color = 21;
  ctx.surface->fill(bg_color);

  switch (current_screen_) {
  case MarketResearchScreen::Area_Info:
    render_area_info(ctx);
    break;
  case MarketResearchScreen::Property_Info:
    render_property_info(ctx);
    break;
  case MarketResearchScreen::Trends:
    render_trends(ctx);
    break;
  case MarketResearchScreen::Pizza_Charts:
    render_pizza_charts(ctx);
    break;
  case MarketResearchScreen::Economy:
    render_economy(ctx);
    break;
  case MarketResearchScreen::Competitors:
    render_competitors(ctx);
    break;
  case MarketResearchScreen::Market_Share:
    render_market_share(ctx);
    break;
  case MarketResearchScreen::Customer_Taste:
    render_customer_taste(ctx);
    break;
  default:
    break;
  }
}

void MarketResearch::create_buttons() {
  using MR = MarketResearchScreen;

  auto make_screen_button = [this](MR screen, Icon icon, ButtonPosition pos,
                                   const std::string& tooltip) {
    input_panel_->add_button(icon, ButtonType::Square, pos, tooltip,
                             [this, screen]() {
                               if (screen == MR::None) {
                                 state_.ui().pop();
                               }
                               else {
                                 current_screen_ = screen;
                                 mark_dirty();
                               }
                             });
  };

  make_screen_button(MR::Area_Info, Icon::INFO, ButtonPosition::TopLeft,
                     "Area Info");
  make_screen_button(MR::Property_Info, Icon::HOUSE, ButtonPosition::TopCenter,
                     "Property Info");
  make_screen_button(MR::Economy, Icon::GRAPH, ButtonPosition::TopRight,
                     "Economy");
  make_screen_button(MR::Customer_Taste, Icon::CUSTOMER_TASTE,
                     ButtonPosition::MiddleLeft, "Customer Taste");
  make_screen_button(MR::Trends, Icon::ARROW_UP_DOWN,
                     ButtonPosition::MiddleCenter, "Trends");
  make_screen_button(MR::Pizza_Charts, Icon::PODIUM,
                     ButtonPosition::MiddleRight, "Pizza Charts");
  make_screen_button(MR::Competitors, Icon::BAR_CHART,
                     ButtonPosition::BottomLeft, "Competitors");
  make_screen_button(MR::Market_Share, Icon::CUBE, ButtonPosition::BottomCenter,
                     "Market Share");
  make_screen_button(MR::None, Icon::ARROW_BACK, ButtonPosition::BottomRight,
                     "Back");
}

void MarketResearch::render_area_info(const RenderContext& ctx) {
  views::ui::render_cork_background(*ctx.surface,
                                    context_.vga->get("gfx/permap"));
  render_paper(ctx, ViewRect{10, 14, 360, 328});

  std::array<std::string, static_cast<int>(DistrictType::count)> type_strings =
      {"unknown", "residential", "industrial", "business",
       "leisure", "shopping",    "traffic"};
  std::array<std::string, static_cast<int>(RangeDescriptor::out_of_range) + 1>
      range_strings = {"low",
                       "very little",
                       "little",
                       "average",
                       "above "
                       "average",
                       "high",
                       "very high",
                       "extremely high",
                       "out of range"};

  ViewPoint position{0, 28};
  context_.font->render_text_onto(*ctx.surface, "zone info", position,
                                  FontFace::RUBY, text_color, AlignX::Center);
  position.y += 32;
  context_.font->render_text_onto(*ctx.surface, district_.name(), position,
                                  FontFace::RUBY, text_color, AlignX::Center);
  position.y += 26;
  context_.font->render_text_onto(*ctx.surface, "population", position,
                                  FontFace::PIZ_NUMB, text_color,
                                  AlignX::Center);

  auto& percentages = district_.population_percentages();
  std::array<std::string, static_cast<int>(PopulationType::count)>
      population_strings = {"Children",        "Young people",
                            "Blue collar",     "White collar",
                            "Business people", "Senior Citizens"};
  position.x = 88;
  position.y += 18;
  for (int i = 0; i < static_cast<int>(PopulationType::count); ++i) {
    context_.font->render_text_onto(*ctx.surface, population_strings[i],
                                    position, FontFace::PIZ_NUMB, text_color,
                                    AlignX::Left);
    context_.font->render_text_onto(
        *ctx.surface, std::to_string(percentages[i]) + " %",
        ViewPoint{-100, position.y}, FontFace::PIZ_NUMB, text_color,
        AlignX::Right);
    position.y += 20;
  }

  position.x = 36;
  position.y += 20;

  context_.font->render_text_onto(*ctx.surface, "income", position,
                                  FontFace::PIZ_NUMB, text_color, AlignX::Left);
  context_.font->render_text_onto(
      *ctx.surface, "$" + std::to_string(district_.average_income()),
      ViewPoint{200, position.y}, FontFace::PIZ_NUMB, text_color, AlignX::Left);

  position.y += 22;

  context_.font->render_text_onto(*ctx.surface, "type of zone", position,
                                  FontFace::PIZ_NUMB, text_color, AlignX::Left);
  context_.font->render_text_onto(
      *ctx.surface, type_strings[static_cast<int>(district_.district_type())],
      ViewPoint{192, position.y}, FontFace::PIZ_NUMB, text_color, AlignX::Left);

  position.y += 22;
  context_.font->render_text_onto(*ctx.surface, "traffic density", position,
                                  FontFace::PIZ_NUMB, text_color, AlignX::Left);
  context_.font->render_text_onto(
      *ctx.surface,
      range_strings[static_cast<int>(
          district_.range_discriptor(district_.traffic_density(), 0, 100))],
      ViewPoint{192, position.y}, FontFace::PIZ_NUMB, text_color, AlignX::Left);

  position.y += 22;

  context_.font->render_text_onto(*ctx.surface, "crime statistics", position,
                                  FontFace::PIZ_NUMB, text_color, AlignX::Left);
  context_.font->render_text_onto(
      *ctx.surface,
      range_strings[static_cast<int>(
          district_.range_discriptor(district_.crime_statistics(), 0, 100))],
      ViewPoint{192, position.y}, FontFace::PIZ_NUMB, text_color, AlignX::Left);
}

void MarketResearch::render_property_info(const RenderContext& ctx) {
  views::ui::render_cork_background(*ctx.surface,
                                    context_.vga->get("gfx/permap"));
  render_paper(ctx, ViewRect{10, 14, 360, 328});

  ViewPoint position(0, 28);
  context_.font->render_text_onto(*ctx.surface, "Property Info", position,
                                  FontFace::RUBY, text_color, AlignX::Center);

  position.y += 32;
  context_.font->render_text_onto(*ctx.surface, district_.name(), position,
                                  FontFace::RUBY, text_color, AlignX::Center);

  position.x = 50;
  position.y += 30;
  context_.font->render_text_onto(*ctx.surface,
                                  "premises availability:", position,
                                  FontFace::PIZ_NUMB, text_color, AlignX::Left);

  context_.font->render_text_onto(
      *ctx.surface, std::to_string(district_.premises_availability()) + " %",
      ViewPoint{270, position.y}, FontFace::PIZ_NUMB, text_color, AlignX::Left);

  position.y += 24;
  context_.font->render_text_onto(*ctx.surface, "rent level:", position,
                                  FontFace::PIZ_NUMB, text_color, AlignX::Left);

  // Both price and rent level are left shifted twice in the original.
  context_.font->render_text_onto(
      *ctx.surface, "$" + std::to_string(district_.rent_level() * 4) + "/sqm",
      ViewPoint{270, position.y}, FontFace::PIZ_NUMB, text_color, AlignX::Left);

  position.y += 24;
  context_.font->render_text_onto(*ctx.surface, "price level:", position,
                                  FontFace::PIZ_NUMB, text_color, AlignX::Left);

  context_.font->render_text_onto(
      *ctx.surface, "$" + std::to_string(district_.price_level() * 4) + "/sqm",
      ViewPoint{270, position.y}, FontFace::PIZ_NUMB, text_color, AlignX::Left);

  position.y += 24;
  context_.font->render_text_onto(*ctx.surface, "tendency:", position,
                                  FontFace::PIZ_NUMB, text_color, AlignX::Left);

  int cur_tendency = district_.tendency_current();
  int target_tendency = district_.tendency_target();
  std::string tendency_str =
      cur_tendency < target_tendency ? "rising" : "falling";
  context_.font->render_text_onto(*ctx.surface, tendency_str,
                                  ViewPoint{270, position.y},
                                  FontFace::PIZ_NUMB, text_color, AlignX::Left);

  position.y += 24;
  context_.font->render_text_onto(*ctx.surface, "average size:", position,
                                  FontFace::PIZ_NUMB, text_color, AlignX::Left);

  context_.font->render_text_onto(
      *ctx.surface, std::to_string(district_.average_square_meters()) + " sqm",
      ViewPoint{270, position.y}, FontFace::PIZ_NUMB, text_color, AlignX::Left);

  position.y += 44;
  context_.font->render_text_onto(*ctx.surface, "rooms:", position,
                                  FontFace::PIZ_NUMB, text_color, AlignX::Left);

  struct BuildingCounts {
    int not_available = 0;
    int for_rent = 0;
    int for_sale = 0;
  };
  BuildingCounts building_counts{};

  for (const auto& bp : district_.city()->properties()) {
    switch (bp->status()) {
    case PremiseStatus::FOR_SALE:
      building_counts.for_sale++;
      break;
    case PremiseStatus::FOR_RENT:
      building_counts.for_rent++;
      break;
    case PremiseStatus::FOR_SALE_RENT:
      building_counts.for_sale++;
      building_counts.for_rent++;
      break;
    default:
      building_counts.not_available++;
      break;
    }
  }

  // Named buildings are permanent premises the original counts as unavailable,
  // though they don't show up on the not available highlight view
  const City* city = district_.city();
  for (int index = 0; index < city->data_length(); ++index) {
    if (city->is_named_building_by_index(index)) {
      building_counts.not_available++;
    }
  }

  position.y += 24;
  constexpr int col_2_x = 270;

  context_.font->render_text_onto(*ctx.surface, "to rent:", position,
                                  FontFace::PIZ_NUMB, text_color, AlignX::Left);

  context_.font->render_text_onto(*ctx.surface,
                                  std::to_string(building_counts.for_sale),
                                  ViewPoint{col_2_x, position.y},
                                  FontFace::PIZ_NUMB, text_color, AlignX::Left);

  position.y += 24;
  context_.font->render_text_onto(*ctx.surface, "for sale:", position,
                                  FontFace::PIZ_NUMB, text_color, AlignX::Left);

  context_.font->render_text_onto(*ctx.surface,
                                  std::to_string(building_counts.for_rent),
                                  ViewPoint{col_2_x, position.y},
                                  FontFace::PIZ_NUMB, text_color, AlignX::Left);

  position.y += 24;
  context_.font->render_text_onto(*ctx.surface, "not available:", position,
                                  FontFace::PIZ_NUMB, text_color, AlignX::Left);

  context_.font->render_text_onto(*ctx.surface,
                                  std::to_string(building_counts.not_available),
                                  ViewPoint{col_2_x, position.y},
                                  FontFace::PIZ_NUMB, text_color, AlignX::Left);
}

void MarketResearch::render_economy(const RenderContext& ctx) {
  ctx.surface->palette(PaletteHandle{7});
  views::ui::render_cork_background(*ctx.surface,
                                    context_.vga->get("gfx/permap"));

  constexpr int grid_color = 27;
  constexpr int axis_color = 240;
  constexpr int graph_color = 23;
  constexpr int tick_color = 20;

  const ViewRect graph_box{10, 12, 360, 214};

  // Draw paper background and grid before decorations so pins sit on top.
  ctx.surface->fill_rect(graph_box, 21);
  gfx::draw::grid(*ctx.surface, graph_box, grid_color);

  // Vertical axis
  gfx::draw::line(*ctx.surface, graph_box.point() + PxOffset{48, 30},
                  graph_box.point() + PxOffset{48, 190}, axis_color, 2);
  // Horizontal axis (zero line)
  gfx::draw::line(*ctx.surface, graph_box.point() + PxOffset{38, 116},
                  graph_box.point() + PxOffset{320, 116}, axis_color, 2);

  // Plot economy history as connected line segments.
  const auto& trend = state_.active_city()->economy_trend();
  int count = trend.history_count();
  const auto& history = trend.history();

  if (count > 1) {
    auto graph_point = [&](int index) -> ViewPoint {
      int x = graph_box.x + 50 + (index * 4);
      int y = graph_box.y + 116 + (history[index] * 2 / 3);
      return ViewPoint{x, y};
    };

    for (int i = 0; i < count - 1; ++i) {
      gfx::draw::line(*ctx.surface, graph_point(i), graph_point(i + 1),
                      graph_color, 2);
    }
  }

  // Decorations on top of grid + graph
  views::ui::render_corner_decorations(
      *ctx.surface, context_.blitmap->get<Blitmap2Icon>(), graph_box);
  views::ui::render_push_pin(*ctx.surface, stanbobs_,
                             ViewPoint{graph_box.x + 15, graph_box.y + 15},
                             false);
  views::ui::render_push_pin(
      *ctx.surface, stanbobs_,
      ViewPoint{graph_box.x + graph_box.w - 40, graph_box.y + 15}, true);

  // Titles
  context_.font->render_line_onto(
      *ctx.surface, "economy",
      graph_box.point() + PxOffset{(graph_box.w / 2) - 56, 14},
      FontFace::ASTANDAR, text_color, AlignX::CenterPoint);
  context_.font->render_line_onto(
      *ctx.surface, std::to_string(state_.time().current_date().year()),
      graph_box.point() + PxOffset{(graph_box.w / 2) + 92, 14},
      FontFace::ASTANDAR, text_color, AlignX::CenterPoint);

  // Inflation rate box, original never seems to change from 3, so just render
  // it as static text
  const ViewRect inflation_box{70, 238, 244, 110};
  render_paper(ctx, inflation_box);

  ViewPoint infl_center =
      inflation_box.point() + PxOffset{inflation_box.w / 2, 20};
  context_.font->render_line_onto(*ctx.surface, "inflation", infl_center,
                                  FontFace::RUBY, 13, AlignX::CenterPoint);
  context_.font->render_line_onto(*ctx.surface, "3%",
                                  infl_center + PxOffset{0, 32}, FontFace::RUBY,
                                  13, AlignX::CenterPoint);
}

void MarketResearch::render_customer_taste(const RenderContext& ctx) {
  render_paper(ctx,
               ViewRect{0, 0, views::base::MAIN_PANEL_WIDTH,
                        views::base::MAIN_PANEL_HEIGHT},
               21);

  const int text_color = 10;
  context_.font->render_line_onto(
      *ctx.surface, "taste", ViewPoint{views::base::MAIN_PANEL_WIDTH / 2, 4},
      FontFace::RUBY, text_color, AlignX::CenterPoint);

  const std::array<std::string_view, 5> tastes = {"fatty", "sweet", "salty",
                                                  "hot", "spicy"};
  const auto& blitmap = context_.blitmap->get2();
  const int x = 16;
  int y = 48;
  const int line_height = 48;
  const auto& thumbs_up =
      static_cast<Blitmap2Icon>(Blitmap2Icon::THUMBS_UP_SMALL);
  const auto& thumbs_down =
      static_cast<Blitmap2Icon>(Blitmap2Icon::THUMBS_DOWN_SMALL);
  const int taste_label_y = 30;
  const int col_width = 56;

  for (size_t i = 0; i < tastes.size(); ++i) {
    int col_offset = 92 + (static_cast<int>(i) * col_width);
    context_.font->render_line_onto(
        *ctx.surface, tastes[i], ViewPoint{x + col_offset, taste_label_y},
        FontFace::PIZ_NUMB, text_color, AlignX::CenterPoint);
  }

  for (auto demo : ALL_CONSUMER_TYPES) {
    const auto demo_icon = static_cast<Blitmap2Icon>(static_cast<int>(demo));
    blitmap.render_icon(demo_icon, *ctx.surface, ViewPoint{x, y});
    const auto& tp = state_.customer_taste_profiles()[demo];
    const std::array<int, 5> taste_values = {tp.fat, tp.sweet, tp.salty, tp.hot,
                                             tp.spicy};
    for (size_t i = 0; i < tastes.size(); ++i) {
      int col_offset = 70 + (static_cast<int>(i) * col_width);
      int percentage = taste_values[i] * 10;
      if (taste_render_percentages_) {
        context_.font->render_line_onto(
            *ctx.surface, compat::format("{} %", percentage),
            ViewPoint{x + col_offset + 20, y + 18}, FontFace::PIZ_NUMB,
            text_color, AlignX::CenterPoint);
      }
      else {
        blitmap.render_icon(percentage > 50 ? thumbs_up : thumbs_down,
                            *ctx.surface, ViewPoint{x + col_offset, y + 4});
      }
    }
    y += line_height;
  }
  interactions_.create_frame_item(
      ViewRect{0, 0, views::base::MAIN_PANEL_WIDTH,
               views::base::MAIN_PANEL_HEIGHT},
      "taste_toggle", [this](views::ui::InteractionEvent ie) {
        if (ie.button == views::ui::MouseButton::Left &&
            ie.type == views::ui::InteractionEvent::Type::Click) {
          taste_render_percentages_ = !taste_render_percentages_;
          mark_dirty();
        }
      });
}

void MarketResearch::render_trends(const RenderContext& ctx) {
  render_paper(ctx, ViewRect{0, 0, views::base::MAIN_PANEL_WIDTH,
                             views::base::MAIN_PANEL_HEIGHT});
  const int text_color = 240;
  const ViewPoint trend_infos_pos{views::base::MAIN_PANEL_WIDTH / 2, 22};
  context_.font->render_line_onto(*ctx.surface, "--- Trend Infos ---",
                                  trend_infos_pos, FontFace::PIZ_NUMB,
                                  text_color, AlignX::CenterPoint);

  const ViewPoint latest_thing_pos{views::base::MAIN_PANEL_WIDTH / 2, 54};
  TextStyle ts{
      .color = text_color,
      .align = AlignX::CenterPoint,
      .shadow = std::nullopt,
      .character_spacing = 4,
  };
  context_.font->render_line_onto(*ctx.surface, "the latest thing",
                                  latest_thing_pos, FontFace::RUBY, ts);

  const auto& thumbs_up =
      context_.blitmap->get2().icon_surface(Blitmap2Icon::THUMBS_UP_BIG);
  const auto& thumbs_down =
      context_.blitmap->get2().icon_surface(Blitmap2Icon::THUMBS_DOWN_BIG);

  ctx.surface->blit_masked(thumbs_up, ViewPoint{56, 88});
  ctx.surface->blit_masked(thumbs_down, ViewPoint{230, 88});

  const ViewPoint up_row1_label{16, 176};
  const ViewPoint up_row1_value{178, 176};
  const ViewPoint down_row1_label{196, 176};
  const ViewPoint down_row1_value{364, 176};
  constexpr int trend_row_height = 18;

  const auto& trends = state_.active_city()->ingredient_trend_system().trends();

  // The original only lists trends whose magnitude falls in [9, 100], at most
  // 8 per column: tiny values and runaway values past +/-100 are hidden.
  constexpr int min_visible_magnitude = 9;
  constexpr int max_visible_magnitude = 100;
  constexpr int max_rows_per_column = 8;

  std::vector<std::pair<int, int>> positive_trends;
  std::vector<std::pair<int, int>> negative_trends;

  for (int i = 0; i < max_ingredient_count; ++i) {
    int val = trends[i];
    int magnitude = std::abs(val);
    if (magnitude < min_visible_magnitude ||
        magnitude > max_visible_magnitude) {
      continue;
    }
    if (val > 0) {
      positive_trends.emplace_back(i, val);
    }
    else {
      negative_trends.emplace_back(i, val);
    }
  }

  // Stable so ties keep ingredient-index order, matching the original's
  // threshold scan (descending magnitude, ascending index within a tie).
  std::stable_sort(
      positive_trends.begin(), positive_trends.end(),
      [](const auto& a, const auto& b) { return a.second > b.second; });
  std::stable_sort(
      negative_trends.begin(), negative_trends.end(),
      [](const auto& a, const auto& b) { return a.second < b.second; });

  const int positive_rows =
      std::min(static_cast<int>(positive_trends.size()), max_rows_per_column);
  const int negative_rows =
      std::min(static_cast<int>(negative_trends.size()), max_rows_per_column);

  for (int row = 0; row < positive_rows; ++row) {
    const auto& [slot, val] = positive_trends[row];
    int y = up_row1_label.y + (row * trend_row_height);
    auto name = std::string(
        core::food::to_string(static_cast<core::food::IngredientType>(slot)));
    context_.font->render_line_onto(
        *ctx.surface, name, ViewPoint{up_row1_label.x, y}, FontFace::PIZ_NUMB,
        text_color, AlignX::Left);
    context_.font->render_line_onto(
        *ctx.surface, std::to_string(val), ViewPoint{up_row1_value.x, y},
        FontFace::PIZ_NUMB, text_color, AlignX::RightEdge);
  }

  for (int row = 0; row < negative_rows; ++row) {
    const auto& [slot, val] = negative_trends[row];
    int y = down_row1_label.y + (row * trend_row_height);
    auto name = std::string(
        core::food::to_string(static_cast<core::food::IngredientType>(slot)));
    context_.font->render_line_onto(
        *ctx.surface, name, ViewPoint{down_row1_label.x, y}, FontFace::PIZ_NUMB,
        text_color, AlignX::Left);
    context_.font->render_line_onto(
        *ctx.surface, std::to_string(val), ViewPoint{down_row1_value.x, y},
        FontFace::PIZ_NUMB, text_color, AlignX::RightEdge);
  }
}

void MarketResearch::render_pizza_charts(const RenderContext& ctx) {
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back30"));

  const ViewPoint first_row{38, 76};
  const int line_height = 22;
  constexpr int text_color = 240;
  constexpr int last_week_color = 12;

  const auto& chart = state_.active_city()->pizza_charts().entries();

  TextStyle style{
      .color = 240,
      .align = AlignX::Left,
      .shadow = std::nullopt,
      .character_spacing = 1,
  };

  const int x = first_row.x;
  int y = first_row.y;
  for (int i = 0; i < PizzaCharts::places; ++i) {
    ViewPoint row_nr_pos{x, y};
    ViewPoint last_week_pos{x + 20, y};
    ViewPoint pizza_name_pos{x + 26, y};
    ViewPoint player_name_pos{x + 186, y};

    context_.font->render_line_onto(*ctx.surface, std::to_string(i + 1),
                                    row_nr_pos, FontFace::PIZ_NUMB, text_color,
                                    AlignX::RightEdge);

    if (i < static_cast<int>(chart.size())) {
      const auto& entry = chart[i];
      std::string prev = entry.previous_position == 0
                             ? "-"
                             : std::to_string(entry.previous_position);
      context_.font->render_line_onto(*ctx.surface, prev, last_week_pos,
                                      FontFace::PIZ_NUMB, last_week_color,
                                      AlignX::RightEdge);
      context_.font->render_line_onto(*ctx.surface, entry.pizza_name,
                                      pizza_name_pos, FontFace::PIZ_NUMB,
                                      style);
      context_.font->render_line_onto(*ctx.surface, entry.player_name,
                                      player_name_pos, FontFace::PIZ_NUMB,
                                      style);
    }

    y += line_height;
  }
}

void MarketResearch::render_competitors(const RenderContext& ctx) {
  std::array<float, NUMBER_OF_PLAYERS> values{};
  auto players = state_.players();
  City* city = state_.active_city();
  if (city) {
    core::CityId city_id = city->id();
    for (int i = 0; i < NUMBER_OF_PLAYERS; ++i) {
      if (players[i]) {
        values[i] =
            static_cast<float>(players[i]->turnover_in_city(city_id).whole());
      }
    }
  }
  render_bar_chart(ctx, BarChartMode::Currency, values);
}

void MarketResearch::render_market_share(const RenderContext& ctx) {
  const auto& values = state_.active_city()->control_map().player_percentages();
  render_bar_chart(ctx, BarChartMode::Percentage, values);
}

void MarketResearch::render_paper(const RenderContext& ctx, ViewRect box,
                                  int bg_color) {
  ctx.surface->fill_rect(box, bg_color);
  views::ui::render_corner_decorations(
      *ctx.surface, context_.blitmap->get<Blitmap2Icon>(), box);
  views::ui::render_push_pin(*ctx.surface, stanbobs_,
                             ViewPoint{box.x + 15, box.y + 15}, false);
  views::ui::render_push_pin(*ctx.surface, stanbobs_,
                             ViewPoint{box.x + box.w - 40, box.y + 15}, true);
}
