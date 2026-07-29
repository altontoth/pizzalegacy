/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/character/Player.h"
#include "core/city/City.h"
#include "core/city/CityViewMapState.h"
#include "core/events/FullscreenMessageManager.h"
#include "core/finance/BankAccountRegistry.h"
#include "core/food/CustomerTasteProfiles.h"
#include "core/init/NewGameConfig.h"
#include "core/mission/MissionTarget.h"
#include "core/newspaper/ArticleDatabase.h"
#include "core/restaurant/FurnitureBatch.h"
#include "core/restaurant/Restaurant.h"
#include "core/restaurant/StaffEventData.h"
#include "core/state/CompetitionCoordinator.h"
#include "core/state/GameOptions.h"
#include "core/state/MilestoneTracker.h"
#include "core/state/RealEstate.h"
#include "core/state/TurnOrchestrator.h"
#include "core/state/UIStateStack.h"
#include "core/status/StatusHub.h"
#include "core/time/GameTime.h"
#include "core/util/Constants.h"
#include "engine/DisplayConstants.h"
#include "engine/geom/Spaces.h"

#include <array>
#include <deque>
#include <functional>
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>

class GameContext;
class TurnPresenter;

class GameState {
  friend std::ostream& operator<<(std::ostream& os, const GameState& state);
  // The orchestrator and competition coordinator are halves of this class,
  // split out for cohesion; they work on GameState internals by design.
  friend class TurnOrchestrator;
  friend class CompetitionCoordinator;
  friend class RealEstate;

public:
  GameState();
  ~GameState();

  GameState(const GameState&) = delete;
  GameState& operator=(const GameState&) = delete;
  GameState(GameState&&) = delete;
  GameState& operator=(GameState&&);

  bool should_exit() const { return quit_; }
  void request_exit() { quit_ = true; }

  // Fade transition control
  void request_fade_transition(int duration_frames = default_fade_length);
  [[nodiscard]] bool fade_requested() const;
  [[nodiscard]] int fade_duration_frames() const;
  void clear_fade_request();

  // Called by the engine when the screen is fully black (just before fading
  // back again)
  void on_fade_midpoint();
  [[nodiscard]] bool pending_fade_midpoint() const {
    return pending_fade_midpoint_;
  }

  // Resolution change request (consumed by engine)
  void request_resolution_change(ScreenResolution res);
  [[nodiscard]] std::optional<ScreenResolution>
  pending_resolution_change() const;
  void clear_resolution_request();

  // CRT aspect ratio change request (consumed by engine)
  void request_crt_change(bool enabled);
  [[nodiscard]] std::optional<bool> pending_crt_change() const;
  void clear_crt_request();

  // Game setup
  void begin_initialization();
  init::Config& initializer();
  void finalize_initialization(GameContext& context);

  // Player management
  std::array<Player*, NUMBER_OF_PLAYERS> players() const;
  Player* active_player() const; // returns active player
  Player* player(core::PlayerId player_id) const;
  Player* restaurant_owner(const Restaurant& restaurant) const;
  void active_player(core::PlayerId player_id);
  void players(std::array<std::unique_ptr<Player>, NUMBER_OF_PLAYERS>);

  // Spending money with insufficient funds notification
  // Returns true if successful, false if insufficient funds (and posts message)
  [[nodiscard]] bool spend_cash(Player& player, Account to, Money amount);
  [[nodiscard]] bool spend_cash(Account to, Money amount); // Uses active player
  [[nodiscard]] bool spend_cash(Account to, Money amount,
                                core::RestaurantId restaurant_id);

  // Bank account registry (player accounts, mayor bribes, mafia laundering)
  BankAccountRegistry& bank_accounts() { return bank_accounts_; }
  const BankAccountRegistry& bank_accounts() const { return bank_accounts_; }

  core::food::CustomerTasteProfiles& customer_taste_profiles() {
    return customer_taste_profiles_;
  }
  const core::food::CustomerTasteProfiles& customer_taste_profiles() const {
    return customer_taste_profiles_;
  }

  // Recalculate demographic scores for all pizzas across all players.
  // Call after mutating customer_taste_profiles.
  void recalculate_all_pizza_demographics();

  // Transfer money from sender to a registered bank account.
  // Debits sender's cash, credits target (player cash or dynamic account).
  // Returns false if account doesn't exist or sender can't afford it.
  bool bank_transfer(int account_number, Money amount, Player& sender);

  // Daily ingredient demand of the player's open restaurants in one city
  // (the scope of a central depot).
  core::food::IngredientDemand
  total_ingredient_need_for_player(core::PlayerId player_id,
                                   core::CityId city) const;

  // Deduct staff wages for a restaurant (called from invoice popup on dismiss)
  void deduct_staff_wages_for_restaurant(core::RestaurantId restaurant_id);

  // Turn flow: end-of-turn pipeline and deferred event delivery.
  TurnOrchestrator& turn() { return turn_; }
  const TurnOrchestrator& turn() const { return turn_; }

  // Monthly and annual pizza competition flows.
  CompetitionCoordinator& competitions() { return competitions_; }
  const CompetitionCoordinator& competitions() const { return competitions_; }

  // Achievement milestone checks (Levent message scheduling).
  MilestoneTracker& milestones() { return milestones_; }

  int global_interest_rate() const { return global_interest_rate_; }
  void global_interest_rate(int rate) { global_interest_rate_ = rate; }

  // City  management
  core::CityId active_city_id() const;
  ViewPoint city_position() const;
  std::optional<int> last_clicked_property_index() const;
  City* active_city() const;
  City* city(core::CityId city_id) const;
  std::vector<City*> initialized_cities() const;
  std::vector<City*> visited_cities(const Player& player) const;
  bool is_city_loaded(core::CityId city_id) const;
  std::optional<core::DistrictId> active_district_id() const;
  void active_city(core::CityId city_id);
  // Create city without changing active_city (debug use):
  void ensure_city_loaded(core::CityId city_id);
  using CityInitFlags = std::array<bool, NUMBER_OF_CITIES>;
  [[nodiscard]] const CityInitFlags& city_initialised_flags() const {
    return city_initialised_;
  }
  void city_initialised_flags(const CityInitFlags& flags) {
    city_initialised_ = flags;
  }
  void active_district_id(core::DistrictId);
  void clear_active_district();
  void city_position(ViewPoint p);
  void last_clicked_property_index(int);
  const CityViewMapState& city_view_view_state() const;
  void city_view_view_state(CityViewMapState);

  // Restaurant management
  void active_restaurant(core::RestaurantId r_id);
  Restaurant* active_restaurant() const;
  std::vector<Restaurant*> restaurants_for(core::PlayerId pid) const;
  std::vector<const Restaurant*> all_restaurants() const;
  int restaurant_count_for_player_in_city(core::PlayerId pid,
                                          core::CityId city) const;
  Restaurant* restaurant(core::RestaurantId r_id) const;
  std::vector<Restaurant*> restaurants_in_city(core::PlayerId pid,
                                               core::CityId city) const;
  std::vector<Restaurant*> restaurant_cycle(core::PlayerId pid,
                                            core::RestaurantId from) const;
  std::optional<core::RestaurantId>
  next_restaurant(core::PlayerId pid, core::RestaurantId from) const;
  std::optional<core::RestaurantId>
  prev_restaurant(core::PlayerId pid, core::RestaurantId from) const;
  // For save/load:
  void add_restaurant(std::unique_ptr<Restaurant> restaurant);
  void remove_restaurant(core::RestaurantId r_id);
  void reset_for_load();

  // Property transactions and restaurant creation.
  RealEstate& real_estate() { return real_estate_; }
  const RealEstate& real_estate() const { return real_estate_; }

  void restaurant_placement(std::vector<FurnitureBatch> batch);
  std::vector<FurnitureBatch>& restaurant_placement();

  // State management
  UIStateStack& ui() { return ui_; }
  const UIStateStack& ui() const { return ui_; }

  int window_height() const;
  int window_width() const;
  void window_size(int width, int height);

  // Time management
  const GameTime& time() const;
  void set_time(const GameTime& t) { time_ = t; } // For save/load
  void advance_game_time();
  void update(uint32_t elapsed_ms);

  // Time advancement control (for pausing during furniture placement, etc.)
  void pause_time_advancement();
  void resume_time_advancement();
  [[nodiscard]] bool is_time_advancement_enabled() const {
    return time_advancement_enabled_;
  }

  // Advance time by n blocks (10 min each) - used for explicit time costs
  // (furniture placement, phone calls, etc.). Stops early if end-of-day
  // is reached.
  void advance_time_blocks(int n);

  // Editing a restaurant's floor pattern tile by tile is a bonus feature,
  // available from the day after El Maestro announces it a year in.
  bool floor_editing_unlocked() const;

  // Debug fast-forward: how many simulation ticks run per unit of wall time
  int time_scale() const { return time_scale_; }
  void time_scale(int scale) { time_scale_ = scale; }

  int difficulty_level() const { return difficulty_level_; };
  void difficulty_level(int d) { difficulty_level_ = d; }

  RankChange adjust_underworld_score(Player& player, int delta);
  RankChange adjust_social_score(Player& player, int delta);

  // Game Events
  void initiate_arrest(ArrestReason,
                       std::function<void()> on_complete = nullptr);
  void check_police_events();
  void bootstrap_minimal_playable_world(GameContext& context);

  StatusHub& status() { return status_hub_; }
  const StatusHub& status() const { return status_hub_; }

  // Game options - delegates to ConfigManager (single source of truth)
  GameOptions& options();
  const GameOptions& options() const;

  // Fullscreen message manager
  core::events::FullscreenMessageManager& message_manager() {
    return message_manager_;
  }
  const core::events::FullscreenMessageManager& message_manager() const {
    return message_manager_;
  }

  // Idempotent message delivery: schedule CV entry + score + push view + mark
  // shown
  void deliver_message(core::events::LeventId msg_id,
                       std::function<void()> on_dismiss = nullptr);
  core::newspaper::ArticleDatabase& article_database() {
    return article_database_;
  }
  const core::newspaper::ArticleDatabase& article_database() const {
    return article_database_;
  }

  // Actor call management (for angry realtors/etc refusing calls)
  void refuse_calls_from(int actor_id);
  bool is_refusing_calls(int actor_id) const;
  void clear_all_refused_calls();
  void eliminate_player(core::PlayerId id, GameOverCause cause);
  void trigger_win_sequence(core::PlayerId pid);

  GameMode game_mode() const;
  void game_mode(GameMode);
  int human_player_count() const;
  // Cities the simulation runs in today: the ones a human player is in.
  // Cities without a human stay dormant.
  std::vector<core::CityId> simulated_cities() const;
  // The subset a player's restaurants are simulated in: for a human the city
  // they are currently in, for the AI it's all the ones a human is in.
  std::vector<core::CityId> simulated_cities_for(core::PlayerId pid) const;

  // Target CV for mission mode; nullopt outside mission mode. Shared by all
  // players (the original copies the same target into every player's arrays).
  const std::optional<core::mission::MissionTarget>& mission_target() const {
    return mission_target_;
  }
  void mission_target(core::mission::MissionTarget target) {
    mission_target_ = std::move(target);
  }
  bool is_mission_active() const {
    return game_mode_ == GameMode::MISSION_MODE && mission_target_.has_value();
  }

  void on_city_entered(core::CityId city_id, const Player& player);
  void check_mafia_events();

  struct RacketOffer {
    core::RestaurantId restaurant_id;
    bool first_contact;
  };
  // Single protection racket roll: 2% chance, then one random restaurant of
  // the active player must be open and have an expired timer. On success the
  // restaurant is marked contacted and the offer is returned.
  std::optional<RacketOffer> roll_protection_racket();
  void push_protection_racket_offer(const RacketOffer& offer);

  // Process all pending messages for the active player immediately:
  // award score, push fullscreen if needed, mark as shown.
  void process_pending_messages();

  // Presenter for staff event dialogs (to keep popup dependencies out of the
  // core).
  using StaffEventPresenter =
      std::function<void(const core::restaurant::StaffEventData&, Restaurant*,
                         std::function<void()>)>;
  void staff_event_presenter(StaffEventPresenter presenter) {
    staff_event_presenter_ = std::move(presenter);
  }

  // Apply the event's world effect (PERSF types) and drive the full staff
  // event dialog chain via the registered presenter.
  void show_staff_event_dialog(const core::restaurant::StaffEventData& event,
                               Restaurant* restaurant,
                               std::function<void()> on_done);

  // Push a mayor award event directly to the UI (debug use).
  void process_mayor_award_event(const MayorAwardEvent& award_event);

  // Presenter that asks one player whether and how to enter the monthly
  // pizza competition; replies nullopt when the player declines.
  using MonthlyEntryPresenter = std::function<void(
      core::PlayerId, bool ask_speed,
      std::function<void(std::optional<MonthlyCompetitionEntry>)>)>;
  void monthly_entry_presenter(MonthlyEntryPresenter presenter) {
    monthly_entry_presenter_ = std::move(presenter);
  }

  // Presenter for the turn-boundary popups (manager alerts, invoices, weekly
  // reports). Registered once at startup; used by TurnOrchestrator.
  void turn_presenter(std::unique_ptr<TurnPresenter> presenter);
  TurnPresenter& turn_presenter() { return *turn_presenter_; }

private:
  bool is_past_player_end_of_day() const;
  // Status bar message posted when spend_cash fails.
  void post_insufficient_funds();

  void load_initial_pizzas(GameContext& context, core::PlayerId player_id);
  std::vector<std::pair<uint8_t, int>> build_player_chars() const;

  GameTime time_;
  core::PlayerId active_player_{0};
  // The city the simulation is currently working on, follows the active
  // human player.
  core::CityId active_city_{-1};
  // Where each human player is.
  std::array<std::optional<core::CityId>, NUMBER_OF_PLAYERS> player_city_;
  // City whose restaurants the queued status notifications belong to.
  std::optional<core::CityId> status_queue_city_;
  ViewPoint city_position_{ViewPoint{-1, -1}};
  std::optional<core::DistrictId> active_district_id_;
  // Per-player city view state, persisted across screen transitions.
  std::array<CityViewMapState, NUMBER_OF_PLAYERS> city_view_view_state_;
  std::array<std::unique_ptr<City>, NUMBER_OF_CITIES> cities_;
  CityInitFlags city_initialised_{};
  City& city_or_create(core::CityId city_id);
  void initialise_city_for_ai(City& city);
  std::array<std::unique_ptr<Player>, NUMBER_OF_PLAYERS> players_;
  std::deque<std::unique_ptr<Restaurant>> restaurants_;
  UIStateStack ui_;
  int window_width_{0};
  int window_height_{0};
  std::optional<int> last_clicked_property_index_;
  bool time_advancement_enabled_{false};
  uint32_t time_accumulator_{0};
  int protection_roll_phase_{0};
  std::optional<core::RestaurantId> active_restaurant_;
  GameMode game_mode_;
  std::optional<core::mission::MissionTarget> mission_target_;
  std::optional<init::Config> initializer_;
  int difficulty_level_{1};
  int time_scale_{1};
  bool quit_{false};
  core::RestaurantId next_restaurant_id_{0};
  std::vector<FurnitureBatch> furniture_placement_;
  StatusHub status_hub_{time_};
  TurnOrchestrator turn_{*this};
  CompetitionCoordinator competitions_{*this};
  MilestoneTracker milestones_{*this};
  RealEstate real_estate_{*this};
  std::optional<ScreenResolution> pending_resolution_;
  std::optional<bool> pending_crt_;
  bool fade_requested_{false};
  int fade_duration_frames_{default_fade_length};
  bool pending_fade_midpoint_{false};
  core::newspaper::ArticleDatabase article_database_;
  core::events::FullscreenMessageManager message_manager_;
  BankAccountRegistry bank_accounts_;
  core::food::CustomerTasteProfiles customer_taste_profiles_{
      core::food::default_customer_taste_profiles()};
  std::unordered_set<int> actors_refusing_calls_;
  // Context pointer for creating popups from GameState
  GameContext* context_{nullptr};
  // UI-layer callbacks; registered once at startup and preserved across
  // reset_for_load.
  StaffEventPresenter staff_event_presenter_;
  MonthlyEntryPresenter monthly_entry_presenter_;
  std::unique_ptr<TurnPresenter> turn_presenter_;

  // Global interest rate to influence loans and investments
  int global_interest_rate_{8};
};
