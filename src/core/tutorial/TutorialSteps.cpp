/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/tutorial/TutorialSteps.h"

#include <algorithm>
#include <magic_enum/magic_enum.hpp>
#include <string>
#include <variant>
#include <vector>

#include "core/GameTypes.h"
#include "core/character/Player.h"
#include "core/city/City.h"
#include "core/city/CityViewCommon.h"
#include "core/city/Property.h"
#include "core/common/Ids.h"
#include "core/factories/FurnitureFactory.h"
#include "core/factories/SupplierFactory.h"
#include "core/food/BakeSession.h"
#include "core/food/Ingredient.h"
#include "core/food/IngredientType.h"
#include "core/food/Pizza.h"
#include "core/food/PlayerRecipes.h"
#include "core/food/Recipe.h"
#include "core/food/StandardPizza.h"
#include "core/restaurant/Furniture.h"
#include "core/restaurant/PlacedFurniture.h"
#include "core/restaurant/Restaurant.h"
#include "core/restaurant/RestaurantLayout.h"
#include "core/restaurant/RestaurantTypes.h"
#include "core/state/GameOptions.h"
#include "core/state/GameRules.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "core/supply/Supplier.h"
#include "core/tutorial/Tutorial.h"
#include "engine/FormatCompat.h"

namespace core::tutorial {

namespace {

constexpr core::DistrictIndex retiro{2};
// About a second and a half, so passing over Retiro does not count.
constexpr int retiro_steady_ticks = 45;
constexpr MapTile bank_gringo{.x = 118, .y = 91};
constexpr SupplierID supplier = SupplierID::SECOND_HAND_FURNITURE;
constexpr FurnitureType table = FurnitureType::ROUND_GRILL;
constexpr FurnitureType chair = FurnitureType::KITCHEN_WHITE;
constexpr int tables_wanted = 2;
constexpr int chairs_per_table = 2;
constexpr SupplierID oven_supplier = SupplierID::OVENS;
constexpr FurnitureType oven = FurnitureType::OVEN_SMALL;

// The items the player may interact with during the tutorial.
const std::vector<std::string> allowed_map_items = {
    "citymap_viewport", "scroll_edge_", "rmb_drag_scroller"};

const std::vector<std::string> allowed_room_items = {
    "rest_view_catchall", "furniture_ghost_tracker", "furn_R_", "scroll_edge_",
    "rmb_drag_scroller"};

const std::vector<std::string> allowed_kitchen_items = {
    "rest_view_catchall", "furniture_ghost_tracker", "furn_K_", "scroll_edge_",
    "rmb_drag_scroller"};

std::vector<std::string> with(std::vector<std::string> items,
                              const std::vector<std::string>& more) {
  items.insert(items.end(), more.begin(), more.end());
  return items;
}

std::string supplier_item(SupplierID id) {
  return "supplier_" + SupplierFactory::create_supplier(id)->name();
}

std::string furniture_item(SupplierType type, FurnitureType furniture) {
  return compat::format("furniture_list.{}.{}", magic_enum::enum_name(type),
                        FurnitureFactory::create_furniture(furniture)->name());
}

bool on_a_call(const UIState& top) {
  return std::holds_alternative<ActorTerminalState>(top) ||
         std::holds_alternative<TerminalState>(top);
}

bool clicked(const GameState& gs, MapTile tile) {
  return gs.last_clicked_property_index() == tile.index();
}

const Property* the_premise(const GameState& gs) {
  const auto* city = gs.active_city();
  return city ? city->property_by_index(tutorial_premise.index()) : nullptr;
}

bool premise_rented(const GameState& gs) {
  const auto* property = the_premise(gs);
  const auto* player = gs.active_player();
  return property && player && property->is_rented() &&
         property->owner_or_tenant() == player->id();
}

int ordered(const GameState& gs, FurnitureType type) {
  const auto* list = gs.ui().top_as<FurnitureListState>();
  return list ? list->ordered(type) : 0;
}

const Restaurant* the_restaurant(const GameState& gs) {
  const auto* property = the_premise(gs);
  if (!property || !property->restaurant_id()) {
    return nullptr;
  }
  return gs.restaurant(*property->restaurant_id());
}

bool shopping_for_kitchenware(const GameState& gs) {
  const auto* overview = gs.ui().top_as<CategoryOverviewState>();
  return overview && overview->type == SupplierType::KITCHENWARE;
}

bool tables_ready(const GameState& gs) {
  const auto* restaurant = the_restaurant(gs);
  if (!restaurant) {
    return false;
  }
  const auto& layout = *restaurant->layout();
  int ready = 0;
  for (const auto& placed : layout.placed_furniture()) {
    if (placed->furniture()->category() == FurnitureCategory::TABLE &&
        layout.linked_furniture_count(placed->id()) >= chairs_per_table) {
      ++ready;
    }
  }
  return ready >= tables_wanted;
}

bool oven_placed(const GameState& gs) {
  const auto* restaurant = the_restaurant(gs);
  return restaurant && restaurant->count_ovens() > 0;
}

using core::food::IngredientType;
using core::food::StandardPizza;

constexpr StandardPizza margherita = StandardPizza::PizzaMargherita;
const std::string margherita_row{core::food::to_string(margherita)};
// Once baked, the row's item name starts with the recipe's id. The
// Margherita is the first recipe the player makes.
const std::string baked_margherita_row =
    std::to_string(core::food::PlayerRecipes{}.next_id.value) + margherita_row;
constexpr int imitation_wanted = 60;

// An open ingredient dropdown, and the pizza design screen itself.
const std::vector<std::string> placing_items = {"dd_", "pizza fallback"};

const PizzaDesignShown* pizza_screen(const GameState& gs) {
  const auto* design = gs.ui().top_as<PizzaDesignState>();
  return design ? &design->shown : nullptr;
}

bool cook_book_open(const GameState& gs, PizzaDesignMode mode) {
  const auto* shown = pizza_screen(gs);
  return shown && shown->mode == mode && shown->cook_book == margherita;
}

bool baking_margherita(const GameState& gs, PizzaDesignMode mode) {
  const auto* design = gs.ui().top_as<PizzaDesignState>();
  return design && design->shown.mode == mode && design->bake &&
         design->bake->reference_id == margherita;
}

bool grams_between(const GameState& gs, IngredientType type, int low,
                   int high) {
  const auto* design = gs.ui().top_as<PizzaDesignState>();
  if (!design || !design->bake) {
    return false;
  }
  const auto weights = design->bake->working.ingredient_grams();
  const auto it = std::ranges::find_if(weights, [type](const auto& weight) {
    return weight.ingredient->type() == type;
  });
  const int grams = it == weights.end() ? 0 : it->grams;
  return grams >= low && grams <= high;
}

const core::food::Recipe* baked_margherita(const GameState& gs) {
  const auto* player = gs.active_player();
  if (!player) {
    return nullptr;
  }
  const auto& slot =
      player->recipes().standard[core::food::to_index(margherita)];
  return slot.created() ? &*slot.recipe : nullptr;
}

bool margherita_good_enough(const GameState& gs) {
  const auto* recipe = baked_margherita(gs);
  return recipe && recipe->pizza().imitation() >= imitation_wanted;
}

} // namespace

const std::vector<Step>& tutorial_steps() {
  static const std::vector<Step> steps = {
      Step{
          .name = "welcome",
          .text = "Welcome to Pizza Legacy! This tutorial walks you through "
                  "opening your first restaurant, here in Madrid. Your "
                  "character and your rivals have been picked for you. You "
                  "can leave at any time with SKIP TUTORIAL in the top right.",
          .target = std::string{skip_item},
          .click_filter = ClickFilter::target_only(),
      },
      Step{
          .name = "rules",
          .text = "This game judges your pizzas the relaxed way and shows "
                  "you the cook book in the game. Your rules for new games "
                  "are set differently, so a new game will not play quite "
                  "like this one. You can change them under Options.",
          .skip_if =
              [](const GameState& gs) {
                return gs.rules() == gs.options().rules;
              },
      },
      Step{
          .name = "districts_tab",
          .text = "The city is divided into districts. Click the DISTRICTS "
                  "tab to see them.",
          .screen = on_screen<CityViewState>,
          .target = "citytab_DISTRICTS",
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) {
                const auto* city = gs.ui().top_as<CityViewState>();
                return city && city->shown.tab == CityViewTabType::DISTRICTS;
              },
      },
      Step{
          .name = "find_retiro",
          .text = "Move the mouse over the map. The blinking box in the "
                  "status panel names the district under it. Find Retiro, "
                  "dark blue on the DISTRICTS tab, and rest the mouse on it.",
          .screen = on_screen<CityViewState>,
          .target = "status_district",
          .click_filter =
              ClickFilter::allow(with(allowed_map_items, {"citytab_"})),
          .panel = PanelPlace::Top,
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) {
                return gs.tutorial().hovered_item() == "citymap_viewport" &&
                       gs.active_district_index() == retiro;
              },
          .steady_ticks = retiro_steady_ticks,
          .on_enter = [](GameState& gs) { gs.clear_active_district(); },
      },
      Step{
          .name = "retiro_found",
          .text = "That's Retiro. It is a good place to start.",
          .screen = on_screen<CityViewState>,
          .click_filter = ClickFilter::target_only(),
          .panel = PanelPlace::Top,
      },
      Step{
          .name = "map_tab",
          .text = "Click the MAP tab to see its streets again. The status "
                  "display still shows the district you are hovering.",
          .screen = on_screen<CityViewState>,
          .target = "citytab_MAP",
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) {
                const auto* city = gs.ui().top_as<CityViewState>();
                return city && city->shown.tab == CityViewTabType::MAP;
              },
      },
      Step{
          .name = "open_locations",
          .text = "The LOCATIONS list highlights every building of the "
                  "selected type on the map. Click it to open it.",
          .screen = on_screen<CityViewState>,
          .target = "LOCATIONS",
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::TargetClicked,
      },
      Step{
          .name = "choose_bank",
          .text = "Choose BANK. The banks will light up on the map.",
          .screen = on_screen<CityViewState>,
          .target = "dd_row_LOCATIONS_1",
          // LOCATIONS stays clickable, so the list can be opened again if it
          // closes.
          .click_filter = ClickFilter::allow({"LOCATIONS"}),
          .completion = Completion::TargetClicked,
      },
      Step{
          .name = "zoom_in",
          .text =
              "Now zoom in on the bank in Retiro. Click anywhere on the map to "
              "get a zoom box (the zoom box has to fit on the map so don't "
              "click too close to the edge), move it over the bank and "
              "click again. From that zoom level again click left to get a "
              "zoom box and click over the bank again. You can zoom out by "
              "right clicking.",
          .screen = on_screen<CityViewState>,
          .click_filter = ClickFilter::allow(allowed_map_items),
          .panel = PanelPlace::Top,
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) {
                const auto* city = gs.ui().top_as<CityViewState>();
                return city && city->shown.zoom == ZoomLevel::CLOSE;
              },
      },
      Step{
          .name = "find_bank",
          .text = "When you click a building the panel at the top right will "
                  "give you information about it. Find Bank Gringo by clicking "
                  "the tile that says BANK on it. You can scroll around with "
                  "the right mouse button or by clicking the wooden borders, "
                  "or right-click to zoom out if you get lost.",
          .screen = on_screen<CityViewState>,
          .click_filter = ClickFilter::allow(allowed_map_items),
          .panel = PanelPlace::Top,
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) { return clicked(gs, bank_gringo); },
      },
      Step{
          .name = "find_premise",
          .text = "Right next to the bank is a business premise. Click it to "
                  "show its information in the status panel in the top right.",
          .screen = on_screen<CityViewState>,
          .target = tutorial_premise,
          .click_filter = ClickFilter::allow(allowed_map_items),
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) { return clicked(gs, tutorial_premise); },
      },
      Step{
          .name = "realtor_name",
          .text = "The status panel shows the premise: its size, its "
                  "price, the monthly rent and the realtor who handles it, "
                  "Quixote Dream Estate. At this point you can't afford to buy "
                  "it, but you can rent it.",
          .screen = on_screen<CityViewState>,
          .target = "status_panel",
          .click_filter = ClickFilter::target_only(),
          .panel = PanelPlace::Bottom,
      },
      Step{
          .name = "selected_property",
          .text = "When you call a realtor the conversation is always about "
                  "the last property that you clicked in the city map. "
                  "Currently that's the premise we want to rent so we can give "
                  "the realtor a call.",
          .screen = on_screen<CityViewState>,
          .click_filter = ClickFilter::target_only(),
          .panel = PanelPlace::Bottom,
      },
      Step{
          .name = "open_terminal",
          .text = "Time to call the realtor. Click the Terminal button in the "
                  "input panel.",
          .screen = on_screen<CityViewState>,
          .target = "Terminal",
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::TargetClicked,
      },
      Step{
          .name = "realtors",
          .text = "In the terminal you can find lots of people to call, split "
                  "across these categories. You can explore this later, for "
                  "now click on Realtors/Estate Agents.",
          .screen = on_screen<TerminalState>,
          .target = "REALTORS",
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::TargetClicked,
      },
      Step{
          .name = "call_realtor",
          .text = "Click Quixote Dream Estate to call them.",
          .screen = on_screen<TerminalState>,
          .target = tutorial_realtor,
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::TargetClicked,
      },
      Step{
          .name = "wait_for_answer",
          .text = "The number is being dialed, wait for someone to pick "
                  "up.",
          .screen = on_screen<TerminalState>,
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) {
                return gs.ui().is_top<ActorTerminalState>();
              },
      },
      Step{
          .name = "greeting",
          .text = "The realtor answers. Click the thumbs up to continue.",
          .screen = on_screen<ActorTerminalState>,
          .target = "Agree",
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::TargetClicked,
      },
      Step{
          .name = "ask_to_rent",
          .text = "In these conversations you can pick what you want to say by "
                  "clicking the right line below the person you're talking to. "
                  "You can respond to questions with the thumbs up and down "
                  "icons in the input panel. Tell the realtor that you want to "
                  "rent this shop.",
          .screen = on_screen<ActorTerminalState>,
          .target = "REALTOR.line.I_WANT_TO_RENT_THIS_SHOP",
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::TargetClicked,
      },
      Step{
          .name = "agree_rent",
          .text = "The realtor tells you how much it costs. Sometimes "
                  "you can haggle when someone proposes a price so it might be "
                  "worth declining an offer, "
                  "but in this case just click the thumbs up to agree.",
          .screen = on_screen<ActorTerminalState>,
          .target = "Agree",
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::Condition,
          .condition = premise_rented,
      },
      Step{
          .name = "hang_up",
          .text = "The restaurant is now yours! Click Back to hang up, and "
                  "again until you are back on the city map.",
          .screen = on_a_call,
          .target = "Back",
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) {
                return gs.ui().is_top<CityViewState>();
              },
      },
      Step{
          .name = "enter_restaurant",
          .text = "The awnings in your player color are up! Click your "
                  "restaurant to go inside. If your shop is not the last "
                  "clicked property you'll have to click it twice to enter.",
          .screen = on_screen<CityViewState>,
          .target = tutorial_premise,
          .click_filter = ClickFilter::allow(allowed_map_items),
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) {
                return gs.ui().is_top<RestaurantViewState>();
              },
      },
      Step{
          .name = "open_furnishing",
          .text = "This is your restaurant. It is still empty, so let's get "
                  "some tables and chairs in. Click the Furnishings button.",
          .screen = on_screen<RestaurantViewState>,
          .target = "Furnishing",
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::TargetClicked,
      },
      Step{
          .name = "furniture",
          .text = "There are four categories of furnishings you can shop, you "
                  "can explore this later. For now, click Furniture.",
          .screen = on_screen<PurchasingState>,
          .target = "Furniture",
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::TargetClicked,
      },
      Step{
          .name = "shop",
          .text = "You get an overview of stats about your current furniture. "
                  "Click Shop to see a list of furniture vendors.",
          .screen = on_screen<CategoryOverviewState>,
          .target = "Shop",
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::TargetClicked,
      },
      Step{
          .name = "supplier",
          .text = "We don't have a lot of money (see the bottom of the status "
                  "panel for your current cash), so let's go with some 2nd "
                  "Hand Furniture. You can get better furniture once we get "
                  "some money coming in.",
          .screen = on_screen<SupplierListState>,
          .target = supplier_item(supplier),
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::TargetClicked,
      },
      Step{
          .name = "order_tables",
          .text = "Click the Round Grill table twice to order two. A left "
                  "click increases the count, a right click decreases it.",
          .screen = on_screen<FurnitureListState>,
          .target = furniture_item(SupplierType::FURNITURE, table),
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) {
                return ordered(gs, table) >= tables_wanted;
              },
      },
      Step{
          .name = "order_chairs",
          .text = "Every table needs chairs. Click the Kitchen White chair "
                  "four times.",
          .screen = on_screen<FurnitureListState>,
          .target = furniture_item(SupplierType::FURNITURE, chair),
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) {
                return ordered(gs, chair) >= tables_wanted * chairs_per_table;
              },
      },
      Step{
          .name = "buy",
          .text = "Click BUY.",
          .screen = on_screen<FurnitureListState>,
          .target = "BUY",
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::TargetClicked,
      },
      Step{
          .name = "pay",
          .text = "This is the invoice. Click anywhere on it to pay so the "
                  "furniture will be delivered to your restaurant.",
          .screen = on_screen<FurnitureListState>,
          .target = ActivePopup{},
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) {
                return gs.ui().is_top<RestaurantViewState>();
              },
      },
      Step{
          .name = "place_furniture",
          .text =
              "Time to lay out your restaurant. A left click places an item, "
              "a right click rotates it. A chair only counts when it "
              "touches a table. A chair will automatically rotate to face the "
              "table you place it in contact with. You can switch between the "
              "tables and chairs with the arrows in the input panel, or by "
              "scrolling your mouse. Now place the tables and chairs.",
          .screen = on_screen<RestaurantViewState>,
          .click_filter = ClickFilter::allow(
              with(allowed_room_items, {"Next item", "Previous item"})),
          .panel = PanelPlace::Top,
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) {
                return gs.restaurant_placement().empty();
              },
      },
      Step{
          .name = "kitchenware_now",
          .text = "Alright, the next thing we definitely need, is an oven."
                  " Click Kitchenware to go shopping.",
          .screen = on_screen<CategoryOverviewState>,
          .target = "Kitchenware",
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::Condition,
          .condition = shopping_for_kitchenware,
          .skip_if = [](const GameState& gs) { return !tables_ready(gs); },
      },
      Step{
          .name = "back_to_restaurant",
          .text = "Not every table has two chairs yet. Click Back to return "
                  "to the restaurant and fix that.",
          .screen = on_screen<CategoryOverviewState>,
          .target = "Back",
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) {
                return gs.ui().is_top<RestaurantViewState>();
              },
          .skip_if = tables_ready,
      },
      Step{
          .name = "fix_chairs",
          .text = "Click a chair, click it again to pick it up, then put it "
                  "down touching a table.",
          .screen = on_screen<RestaurantViewState>,
          .click_filter = ClickFilter::allow(allowed_room_items),
          .panel = PanelPlace::Top,
          .completion = Completion::Condition,
          .condition = tables_ready,
          .skip_if = tables_ready,
      },
      Step{
          .name = "oven_furnishing",
          .text =
              "Guests can sit down now, that means it's time to get an oven."
              " Click Furnishing again to buy an oven.",
          .screen = on_screen<RestaurantViewState>,
          .target = "Furnishing",
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::TargetClicked,
          .skip_if = shopping_for_kitchenware,
      },
      Step{
          .name = "kitchenware",
          .text = "This time click Kitchenware.",
          .screen = on_screen<PurchasingState>,
          .target = "Kitchenware",
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::TargetClicked,
          .skip_if = shopping_for_kitchenware,
      },
      Step{
          .name = "oven_shop",
          .text = "Click Shop to see the vendors.",
          .screen = on_screen<CategoryOverviewState>,
          .target = "Shop",
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::TargetClicked,
      },
      Step{
          .name = "oven_supplier",
          .text = "Pick Ovens.",
          .screen = on_screen<SupplierListState>,
          .target = supplier_item(oven_supplier),
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::TargetClicked,
      },
      Step{
          .name = "order_oven",
          .text = "Click Oven small once. It's faster than the grill and has a "
                  "higher quality, which your customers will appreciate.",
          .screen = on_screen<FurnitureListState>,
          .target = furniture_item(SupplierType::KITCHENWARE, oven),
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) { return ordered(gs, oven) >= 1; },
      },
      Step{
          .name = "buy_oven",
          .text = "Click BUY.",
          .screen = on_screen<FurnitureListState>,
          .target = "BUY",
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::TargetClicked,
      },
      Step{
          .name = "pay_oven",
          .text = "Click the invoice to pay for the oven.",
          .screen = on_screen<FurnitureListState>,
          .target = ActivePopup{},
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) {
                return gs.ui().is_top<RestaurantViewState>();
              },
      },
      Step{
          .name = "place_oven",
          .text = "Ovens go in the kitchen, so this is where you are now. "
                  "Left click to put the oven down anywhere inside the "
                  "walls.",
          .screen = on_screen<RestaurantViewState>,
          .click_filter = ClickFilter::allow(allowed_kitchen_items),
          .panel = PanelPlace::Top,
          .completion = Completion::Condition,
          .condition = oven_placed,
      },
      Step{
          .name = "oven_back_to_restaurant",
          .text = "The oven is in. Click Back to return to the restaurant.",
          .screen = on_screen<CategoryOverviewState>,
          .target = "Back",
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) {
                return gs.ui().is_top<RestaurantViewState>();
              },
      },
      // Creating a pizza.
      Step{
          .name = "open_recipes",
          .text = "Guests come for pizza, so you need a recipe. Click Recipe "
                  "list.",
          .screen = on_screen<RestaurantViewState>,
          .target = "Recipe list",
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) {
                return gs.ui().is_top<PizzaDesignState>();
              },
      },
      Step{
          .name = "recipe_list",
          .text = "This is your recipe list. The first page holds the "
                  "standard pizzas from the cook book. You haven't baked any "
                  "of them yet. Let's start with a Pizza Margherita.",
          .screen = on_screen<PizzaDesignState>,
          .click_filter = ClickFilter::target_only(),
      },
      Step{
          .name = "open_cook_book",
          .text = "Right-click Pizza Margherita and choose Show cook book.",
          .screen = on_screen<PizzaDesignState>,
          .target = margherita_row,
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) {
                return cook_book_open(gs, PizzaDesignMode::View);
              },
      },
      Step{
          .name = "cook_book_picture",
          .text = "This is the Margherita from the cook book. It has "
                  "tomatoes, mozzarella, parmesan, oregano and a few olives. "
                  "Where each piece lies hardly matters. What counts is "
                  "what goes on it, and how much.",
          .screen = on_screen<PizzaDesignState>,
          .click_filter = ClickFilter::target_only(),
          .panel = PanelPlace::Top,
      },
      Step{
          .name = "cook_book_ingredients",
          .text = "Click Ingredients to see how much of each it takes.",
          .screen = on_screen<PizzaDesignState>,
          .target = "Ingredients",
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) {
                return cook_book_open(gs, PizzaDesignMode::Ingredients);
              },
      },
      Step{
          .name = "bake",
          .text = "Your own pizza should come as close to these weights as "
                  "you can get it. Click Bake pizza to make one.",
          .screen = on_screen<PizzaDesignState>,
          .target = "Bake pizza",
          .click_filter = ClickFilter::target_only(),
          .panel = PanelPlace::Bottom,
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) {
                return baking_margherita(gs, PizzaDesignMode::Design);
              },
      },
      Step{
          .name = "pizza_base",
          .text = "Here is your pizza base. The four boxes at the top hold "
                  "the ingredients: vegetables, meat, fruit, and cheese with "
                  "the herbs. The chopper at the bottom left cuts a piece "
                  "smaller.",
          .screen = on_screen<PizzaDesignState>,
          .click_filter = ClickFilter::target_only(),
      },
      Step{
          .name = "tomatoes",
          .text = "Open VEGETABLES and pick TOMATOES. Click the pizza to put "
                  "a slice down, then click again to take the next one. Put "
                  "down 15 slices. A right-click takes the last one off "
                  "again. Drag this panel aside if it is in the way.",
          .screen = on_screen<PizzaDesignState>,
          .target = "VEGETABLES",
          .click_filter =
              ClickFilter::allow(with(placing_items, {"VEGETABLES"})),
          .panel = PanelPlace::Bottom,
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) {
                return grams_between(gs, IngredientType::TOMATOES, 300, 300);
              },
      },
      Step{
          .name = "weigh_tomatoes",
          .text = "Click Ingredients to see what is on your pizza.",
          .screen = on_screen<PizzaDesignState>,
          .target = "Ingredients",
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) {
                return baking_margherita(gs, PizzaDesignMode::Ingredients);
              },
      },
      Step{
          .name = "tomatoes_weighed",
          .text = "15 slices of 20 g make 300 g of tomatoes, just like the "
                  "cook book.",
          .screen = on_screen<PizzaDesignState>,
          .click_filter = ClickFilter::target_only(),
          .panel = PanelPlace::Bottom,
      },
      Step{
          .name = "back_to_pizza",
          .text = "Click View pizza to go back to your pizza.",
          .screen = on_screen<PizzaDesignState>,
          .target = "View pizza",
          .click_filter = ClickFilter::allow({"pizza_main_panel"}),
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) {
                return baking_margherita(gs, PizzaDesignMode::Design);
              },
      },
      Step{
          .name = "mozzarella",
          .text = "Now the mozzarella. Open CHEESE and pick MOZZARELLA. The "
                  "status panel shows that a whole one weighs 80 g, so click "
                  "the chopper once to cut it into 20 g pieces. Put down 7 "
                  "of them.",
          .screen = on_screen<PizzaDesignState>,
          .target = "CHEESE",
          .click_filter =
              ClickFilter::allow(with(placing_items, {"CHEESE", "chopper"})),
          .panel = PanelPlace::Bottom,
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) {
                return grams_between(gs, IngredientType::MOZZARELLA, 140, 140);
              },
      },
      Step{
          .name = "parmesan",
          .text = "The parmesan is in the same box. Pick PARMESAN, chop it "
                  "once and put down 3 pieces.",
          .screen = on_screen<PizzaDesignState>,
          .target = "CHEESE",
          .click_filter =
              ClickFilter::allow(with(placing_items, {"CHEESE", "chopper"})),
          .panel = PanelPlace::Bottom,
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) {
                return grams_between(gs, IngredientType::PARMESAN, 55, 60);
              },
      },
      Step{
          .name = "oregano",
          .text = "So are the herbs. Pick OREGANO and put down 5 sprigs, "
                  "whole.",
          .screen = on_screen<PizzaDesignState>,
          .target = "CHEESE",
          .click_filter = ClickFilter::allow(with(placing_items, {"CHEESE"})),
          .panel = PanelPlace::Bottom,
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) {
                return grams_between(gs, IngredientType::OREGANO, 20, 24);
              },
      },
      Step{
          .name = "olives",
          .text = "Last come the olives. Open VEGETABLES, pick OLIVES and "
                  "put down 7.",
          .screen = on_screen<PizzaDesignState>,
          .target = "VEGETABLES",
          .click_filter =
              ClickFilter::allow(with(placing_items, {"VEGETABLES"})),
          .panel = PanelPlace::Bottom,
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) {
                return grams_between(gs, IngredientType::OLIVES, 35, 35);
              },
      },
      Step{
          .name = "weigh_pizza",
          .text = "Click Ingredients again and compare your pizza with the "
                  "cook book.",
          .screen = on_screen<PizzaDesignState>,
          .target = "Ingredients",
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) {
                return baking_margherita(gs, PizzaDesignMode::Ingredients);
              },
      },
      Step{
          .name = "judge",
          .text = "A few grams off here and there is fine. Click O.K. to let "
                  "the judges taste it.",
          .screen = on_screen<PizzaDesignState>,
          .target = "O.K.",
          .click_filter = ClickFilter::target_only(),
          .panel = PanelPlace::Bottom,
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) {
                return gs.ui().is_top<PizzaJudgesState>();
              },
      },
      Step{
          .name = "judges",
          .text = "The judges score how close your pizza comes to the cook "
                  "book. Click when they are done.",
          .screen = on_screen<PizzaJudgesState>,
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) {
                return baked_margherita(gs) &&
                       gs.ui().is_top<PizzaDesignState>();
              },
      },
      Step{
          .name = "bake_again",
          .text = "That is too far from the cook book. Right-click Pizza "
                  "Margherita and look at the cook book and its ingredients "
                  "again. Then pick Pizza Margherita, click Bake pizza and "
                  "make it once more.",
          .screen = on_screen<PizzaDesignState>,
          .completion = Completion::Condition,
          .condition = margherita_good_enough,
          .skip_if = margherita_good_enough,
      },
      Step{
          .name = "pick_recipe",
          .text = "Your Pizza Margherita is on the recipe list now. Click it.",
          .screen = on_screen<PizzaDesignState>,
          .target = baked_margherita_row,
          .click_filter = ClickFilter::target_only(),
          .completion = Completion::Condition,
          .condition =
              [](const GameState& gs) {
                const auto* shown = pizza_screen(gs);
                const auto* recipe = baked_margherita(gs);
                if (!shown || !recipe || !shown->selected) {
                  return false;
                }
                const auto* id = std::get_if<core::RecipeId>(&*shown->selected);
                return id && *id == recipe->id();
              },
      },
      Step{
          .name = "imitation",
          .text = "The status panel shows your recipe. Imitation, at the "
                  "bottom, is how close it came to the cook book. The closer "
                  "it is, the more guests like it.",
          .screen = on_screen<PizzaDesignState>,
          .target = "status_panel",
          .click_filter = ClickFilter::target_only(),
      },
      Step{
          .name = "the_end",
          .text = "Your restaurant has tables, an oven and its first "
                  "recipe. That is all the tutorial covers for now: next come "
                  "staff and a menu before you open. The game is yours from "
                  "here.",
          .click_filter = ClickFilter::target_only(),
      },
  };
  return steps;
}

} // namespace core::tutorial
