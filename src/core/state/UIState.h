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
#include <string>
#include <variant>
#include <vector>
#include "conversations/MafiaLine.h"
#include "conversations/MayorLine.h"
#include "core/GameTypes.h"
#include "core/common/Ids.h"
#include "core/events/LeventId.h"
#include "core/food/BakeSession.h"
#include "core/food/IngredientCategoryType.h"
#include "core/food/Pizza.h"
#include "core/food/PlayerRecipes.h"
#include "core/restaurant/RestaurantTypes.h"
#include "core/state/CompetitionSession.h"
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
    MenuHighScores,
    CharacterSelection,
    DifficultySelection,
    DiskMenu,
    MenuGameLoad,
    MenuSave,
    MenuSaveAsGame,

    Advertising,
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
  bool end_of_week_mode{false};
  std::function<void()> on_done;
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
struct PizzaDesignState {
  PizzaDesignMode initial_mode;
  std::optional<core::PlayerId> opponent = std::nullopt;
  std::shared_ptr<core::food::BakeSession> bake = nullptr;
  std::optional<RecipeKey> active_key = std::nullopt;
  std::shared_ptr<CompetitionSession> competition_session = nullptr;
  // Set when a specific player is competing (multi-player competitions).
  // When absent, falls back to active_player().
  std::optional<core::PlayerId> competing_player_id = std::nullopt;
  // Called when the screen is dismissed after a competition (optional).
  std::function<void()> on_done = nullptr;
};
struct EuroMapState {
  MapMode mode;
  std::optional<IngredientCategory> category = std::nullopt;
};
enum class GameOverCause {
  BankruptLoans, // unused
  BankruptRent,  // unused
  BankruptDebts,
  BankruptSuppliers,
  KilledByMafia,
  PizzaTycoon, // you won
};

struct GameOverState {
  core::PlayerId player_id;
  GameOverCause cause;
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
  JudgingMode judging_mode{JudgingMode::Own};
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
struct MafiaState {
  ActorId id;
  std::optional<MafiaMode> mode;
  std::optional<core::RestaurantId> restaurant_id;
  bool first_contact{false};
};
struct PoliceState {
  ActorId id;
  std::optional<ArrestReason> reason;
};
// Restaurant stock:
struct StockMainState {};
struct ContractsViewState {};

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
  std::optional<std::vector<IngredientBatch>> saved_ingredients = std::nullopt;
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
  std::function<void()> on_dismiss;
};
struct WinSequenceState {
  int line_index;
  core::PlayerId player_id;
};
struct MarketResearchState {
  core::DistrictId district_id;
};
struct MafiaMissionViewState {
  MafiaLine line;
};
struct MayorState {
  ActorId id{};
  MayorConversationType type{MayorConversationType::PhoneCall};
  std::optional<core::RestaurantId> restaurant_id;
  std::optional<bool> is_first_inspection; // true=first, false=regular
  std::optional<int> award_winner_player_id;
  bool inspection_star_gained{false};
  bool inspection_star_lost{false};
  std::function<void()> on_complete;
};

using UIState = std::variant<
    ScreenTag, EuroMapState, PizzaJudgesState, CityViewState, PurchasingState,
    PurchasingDemographicState, ActorTerminalState, BankState, PoliceState,
    MafiaState, MayorState, RestaurantViewState, ViewerState, FloorListState,
    FurnitureListState, CategoryOverviewState, SupplierListState,
    PizzaDesignState, RestaurantMenuState, RestaurantStatsState,
    WeeksTakingsState, AnimatedStatsState, IngredientCategoryViewState,
    StockMainState, ContractsViewState, DepotMainState, DepotExportState,
    IngredientSupplierListState, IngredientOrderState,
    DepotWeaponInterfaceState, TerminalState, SupplierContractState,
    PlayerTurnCardState, FullscreenStatusState, WinSequenceState,
    MafiaMissionViewState, MarketResearchState, StatsRestaurantsState,
    PlayerProfileState, GameOverState,
    // nothing on screen:
    std::monostate>;
