/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <array>
#include <filesystem>
#include <functional>
#include <optional>
#include <variant>
#include <vector>
#include "core/GameTypes.h"
#include "core/common/Ids.h"
#include "core/events/LeventId.h"
#include "core/food/BakeSession.h"
#include "core/food/IngredientCategoryType.h"
#include "core/food/Pizza.h"
#include "core/food/PlayerRecipes.h"
#include "core/restaurant/RestaurantTypes.h"
#include "core/state/ViewerType.h"
#include "core/supply/IngredientBatch.h"
#include "core/supply/Supplier.h"
#include "core/time/GameDate.h"

struct ScreenTag {
  enum class Kind {
    Intro,
    MenuMain,
    MenuNewGame,
    MenuOptions,
    MenuQuickstart,
    CharacterSelection,
    DifficultySelection,
    DiskMenu,
    MenuGameLoad,
    MenuSave,
    MenuSaveAsGame,

    Advertising,
    MarketResearch,
    StaffView,
    Newspaper,
    GlobalStats,
  };

  Kind kind;
};

class Property;

enum class MapMode;
enum class SupplierID;
enum class ActorType;
enum class RestaurantViewMode;
using ActorId = int;

struct CityViewState {
  core::CityId city_id;
};
struct RestaurantMenuState {
  core::RestaurantId id;
};
struct RestaurantStatsState {
  core::RestaurantId id;
  std::optional<int> screen_index = std::nullopt; // RestaurantStatsScreen index
  bool automatic_mode =
      false; // End of day automatic report (changes today/yesterday labels)
};
struct WeeksTakingsState {
  core::RestaurantId id;
};
struct AnimatedStatsState {
  core::RestaurantId id;
};
struct RestaurantViewState {
  core::RestaurantId id;
  RestaurantViewMode mode;
  std::optional<SupplierType> return_to_supplier_type = std::nullopt;
  bool reopen_restaurant = false;
};
struct PizzaManagerState {
  PizzaManagerMode initial_mode;
  std::optional<core::PlayerId> opponent = std::nullopt;
  std::shared_ptr<core::food::BakeSession> bake = nullptr;
  std::optional<RecipeKey> active_key = std::nullopt;
};
struct EuroMapState {
  MapMode mode;
  std::optional<IngredientCategory> category = std::nullopt;
};
struct PlayerProfileState {
  core::PlayerId player_id;
};
struct StatsRestaurantsState {
  core::PlayerId player_id;
};
struct PizzaJudgesState {
  std::array<int, NUMBER_OF_JUDGES> scores;
  std::function<void()> on_complete;
  bool is_canonical_comparison{false};
};
struct PurchasingState {
  SupplierType supplier;
};
struct SupplierListState {
  SupplierType supplier;
};
struct PurchasingDemographicState {
  SupplierType supplier;
};
struct ViewerState {
  std::filesystem::path filename;
  ViewerType type = ViewerType::Auto;
};
struct CategoryOverviewState {
  SupplierType type;
};
struct FurnitureListState {
  SupplierID supplier_id;
};
struct FloorListState {
  SupplierID supplier_id;
};
struct TerminalState {
  // Show the 'connection succeeded' string on return from an agent
  std::optional<TerminalCategory> category = std::nullopt;
};
struct ActorTerminalState {
  ActorType type{};
  ActorId id{};
};
// This used to have an extra argument but it refactored out, so maybe
// it could go back into ActorTerminalState. But maybe we give everyone
// their own one anyway. For now leave it.
struct BankState {
  ActorId id{};
};
struct PoliceState {
  ActorId id;
  std::optional<ArrestReason> reason;
};
// Restaurant stock:
struct StockMainState {};

enum class IngredientScreenMode {
  RESTAURANT,
  CENTRAL_DEPOT,
  CENTRAL_DEPOT_SELL
};

struct IngredientSupplierListState {
  IngredientCategory category;
  IngredientScreenMode mode = IngredientScreenMode::RESTAURANT;
};
struct IngredientOrderState {
  uint8_t supplier_id;
  IngredientCategory category;
  IngredientScreenMode mode = IngredientScreenMode::RESTAURANT;
};
struct IngredientCategoryViewState {
  IngredientCategory category;
  IngredientScreenMode mode = IngredientScreenMode::RESTAURANT;
};
// Central Depot:
struct DepotMainState {
  Property* property;
};
struct DepotExportState {
  IngredientCategory category;
  core::CityId target_city_id;
  int transport_cost;
};
struct DepotWeaponInterfaceState {
  Property* property;
};
struct SupplierContractState {
  uint8_t supplier_id;
  IngredientCategory category;
  std::vector<IngredientBatch> order;
  Money total;
};
struct PlayerTurnCardState {
  core::PlayerId player;
  GameDate date;
};
struct FullscreenStatusState {
  core::events::LeventId message_id;
};

using UIState = std::variant<
    ScreenTag, EuroMapState, PizzaJudgesState, CityViewState, PurchasingState,
    PurchasingDemographicState, ActorTerminalState, BankState, PoliceState,
    RestaurantViewState, ViewerState, FloorListState, FurnitureListState,
    CategoryOverviewState, SupplierListState, PizzaManagerState,
    RestaurantMenuState, RestaurantStatsState, WeeksTakingsState,
    AnimatedStatsState, IngredientCategoryViewState, StockMainState,
    DepotMainState, DepotExportState, IngredientSupplierListState,
    IngredientOrderState, DepotWeaponInterfaceState, TerminalState,
    SupplierContractState, PlayerTurnCardState, FullscreenStatusState,
    StatsRestaurantsState, PlayerProfileState,
    // nothing on screen:
    std::monostate>;
