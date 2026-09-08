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
#include "core/GameTypes.h"
#include "core/common/Ids.h"
#include "core/conversations/MafiaLine.h"
#include "core/conversations/MayorLine.h"
#include "core/events/MilestoneId.h"
#include "core/finance/Money.h"
#include "core/food/BakeSession.h"
#include "core/food/IngredientCategoryType.h"
#include "core/food/Pizza.h"
#include "core/food/PlayerRecipes.h"
#include "core/restaurant/RestaurantTypes.h"
#include "core/state/AnimatedStatsScreen.h"
#include "core/state/CompetitionSession.h"
#include "core/state/ViewerType.h"
#include "core/supply/IngredientBatch.h"
#include "core/supply/Supplier.h"
#include "core/time/GameDate.h"

struct ScreenTag {
  enum class Kind {
    GameLocation,
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
    MenuSaveAsMission,
    MissionSelect,

    Advertising,
    StaffView,
    Newspaper,
  };

  Kind kind;
};

class Property;

enum class MapMode;
enum class SupplierID;
enum class ActorType;
enum class RestaurantViewMode;

enum class OptionsScreen { Audio, Graphics, Controls, Gameplay };

struct CityViewState {
  core::CityId city_id;
};
struct OptionsMenuState {
  OptionsScreen screen;
};
struct RestaurantMenuState {
  core::RestaurantId id;
};
struct GlobalStatsState {
  std::optional<int> screen_index = std::nullopt; // GlobalStatsScreen index
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
  AnimatedStatsScreen screen{AnimatedStatsScreen::Production};
};
struct RestaurantViewState {
  core::RestaurantId id;
  RestaurantViewMode mode;
  std::optional<SupplierType> return_to_supplier_type = std::nullopt;
  bool reopen_restaurant = false;
  bool kitchen_view = false;
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
  PizzaTycoon,     // you won
  MissionComplete, // achieved the mission's last target milestone
};

struct GameOverState {
  core::PlayerId player_id;
  GameOverCause cause;
};
struct MaestroMessageState {
  std::string text;
  std::function<void()> on_dismiss;
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
struct ViewerState {
  std::filesystem::path filename;
  ViewerType type = ViewerType::Auto;
};
struct CategoryOverviewState {
  SupplierType type;
  // Show the customer ratings page instead of the category's own page.
  bool demographics{false};
};
struct FurnitureListState {
  SupplierID supplier_id;
};
struct FloorListState {
  SupplierID supplier_id;
};
struct TerminalState {
  // The category page the terminal opens on.
  std::optional<TerminalCategory> category = std::nullopt;
  // Play the 'connection successful' line.
  bool connection_made = false;
};
struct ActorTerminalState {
  ActorType type{};
  core::ActorId id{};
};
// This used to have an extra argument but it refactored out, so maybe
// it could go back into ActorTerminalState. But maybe we give everyone
// their own one anyway. For now leave it.
struct BankState {
  core::ActorId id{};
};
struct MafiaState {
  core::ActorId id;
  std::optional<MafiaMode> mode;
  std::optional<core::RestaurantId> restaurant_id;
  bool first_contact{false};
};
struct PoliceState {
  core::ActorId id;
  std::optional<ArrestReason> reason;
  std::function<void()> on_complete;
};
// Restaurant stock:
struct StockMainState {};
struct ContractsViewState {};

enum class IngredientScreenMode { CENTRAL_DEPOT_BUY, CENTRAL_DEPOT_SELL };

struct IngredientSupplierListState {
  IngredientCategory category;
};
struct IngredientOrderState {
  uint8_t supplier_id;
  IngredientCategory category;
  std::optional<std::vector<IngredientBatch>> saved_ingredients = std::nullopt;
};
struct IngredientCategoryViewState {
  IngredientCategory category;
};
struct DepotIngredientCategoryViewState {
  IngredientCategory category;
};
struct DepotIngredientSupplierListState {
  IngredientCategory category;
  IngredientScreenMode mode = IngredientScreenMode::CENTRAL_DEPOT_BUY;
};
struct DepotIngredientOrderState {
  uint8_t supplier_id;
  IngredientCategory category;
  IngredientScreenMode mode = IngredientScreenMode::CENTRAL_DEPOT_BUY;
  std::optional<std::vector<IngredientBatch>> saved_ingredients = std::nullopt;
};
// Central Depot:
struct DepotMainState {
  Property* property;
};
struct DepotExportState {
  IngredientCategory category;
  core::CityId target_city_id;
  Money transport_cost;
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
  std::function<void()> on_closed;
};
struct FullscreenStatusState {
  core::events::MilestoneId message_id;
  std::function<void()> on_dismiss;
};
struct WinSequenceState {
  int line_index;
  core::PlayerId player_id;
};
struct MarketResearchState {
  core::DistrictIndex district_index;
};
struct MafiaMissionViewState {
  MafiaLine line;
};
struct MayorState {
  core::ActorId id{};
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
    ActorTerminalState, BankState, PoliceState, MafiaState, MayorState,
    RestaurantViewState, ViewerState, FloorListState, FurnitureListState,
    CategoryOverviewState, SupplierListState, PizzaDesignState,
    RestaurantMenuState, GlobalStatsState, RestaurantStatsState,
    WeeksTakingsState, AnimatedStatsState, IngredientCategoryViewState,
    DepotIngredientCategoryViewState, StockMainState, ContractsViewState,
    DepotMainState, DepotExportState, IngredientSupplierListState,
    DepotIngredientSupplierListState, IngredientOrderState,
    DepotIngredientOrderState, DepotWeaponInterfaceState, TerminalState,
    SupplierContractState, PlayerTurnCardState, FullscreenStatusState,
    WinSequenceState, MafiaMissionViewState, MarketResearchState,
    StatsRestaurantsState, PlayerProfileState, GameOverState,
    MaestroMessageState, OptionsMenuState,
    // nothing on screen:
    std::monostate>;
