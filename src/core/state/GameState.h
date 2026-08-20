/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/character/Player.h"
#include "core/city/City.h"
#include "core/city/CityViewMapState.h"
#include "core/economy/WeaponMarket.h"
#include "core/events/CareerLog.h"
#include "core/finance/BankAccountRegistry.h"
#include "core/food/CustomerTasteProfiles.h"
#include "core/init/NewGameConfig.h"
#include "core/mission/MissionTarget.h"
#include "core/newspaper/ArticleDatabase.h"
#include "core/restaurant/FurnitureBatch.h"
#include "core/restaurant/Restaurant.h"
#include "core/restaurant/StaffEventData.h"
#include "core/state/ClockRate.h"
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
class FadePresenter;

constexpr int simulation_ticks_per_second = 30;
constexpr uint32_t simulation_tick_ms = 1000 / simulation_ticks_per_second;

// PIT is for Programmable Interval Timer, a chip the BIOS set to
// interrupt 18.2065 times a second on the hardware of the day. The original
// polls the timer, so one PIT tick is the finest time it can tell.
constexpr uint32_t pit_tick_us = 54925;

// The original calls its clock routine every 18 PIT ticks and treats that as
// one second, making the displayed playtime run about 1.1% fast.
constexpr int pit_ticks_per_second = 18;

// A ten-minute block takes eleven of those seconds.
constexpr int seconds_per_block = 11;

// The original uses a byte for this counter. 256 is a multiple of the window
// spacing, so the windows keep their spacing across the wrap.
constexpr int second_counter_wrap = 256;

// An interruption window (the time that interruptions are allowed to make it
// through) opens at 0, 8, 16, etc. and lasts for one second. With an
// eleven-second block, that means two windows per block, at 0 and 8.
constexpr int seconds_between_interruption_windows = 8;

// Each interruption pass runs the next phase of this round-robin. The fourth
// phase does nothing, matching the original's empty fourth case.
enum class InterruptionPhase {
  QueuedEvents,
  StaffEvents,
  ProtectionRacket,
  Unused,
};

constexpr InterruptionPhase next_phase(InterruptionPhase phase) {
  switch (phase) {
  case InterruptionPhase::QueuedEvents:
    return InterruptionPhase::StaffEvents;
  case InterruptionPhase::StaffEvents:
    return InterruptionPhase::ProtectionRacket;
  case InterruptionPhase::ProtectionRacket:
    return InterruptionPhase::Unused;
  case InterruptionPhase::Unused:
    return InterruptionPhase::QueuedEvents;
  }
  return InterruptionPhase::QueuedEvents;
}

constexpr bool is_valid_interruption_phase(int phase) {
  return phase >= static_cast<int>(InterruptionPhase::QueuedEvents) &&
         phase <= static_cast<int>(InterruptionPhase::Unused);
}

constexpr bool is_valid_city_id(core::CityId city_id) {
  return city_id.value >= 0 && city_id.value < NUMBER_OF_CITIES;
}

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

  void register_bank_accounts();

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

  BankAccountRegistry& bank_accounts() { return bank_accounts_; }
  const BankAccountRegistry& bank_accounts() const { return bank_accounts_; }

  WeaponMarket& weapon_market() { return weapon_market_; }
  const WeaponMarket& weapon_market() const { return weapon_market_; }

  core::food::CustomerTasteProfiles& customer_taste_profiles() {
    return customer_taste_profiles_;
  }
  const core::food::CustomerTasteProfiles& customer_taste_profiles() const {
    return customer_taste_profiles_;
  }

  // Recalculate demographic scores for all pizzas across all players.
  // Call after mutating customer_taste_profiles.
  void recalculate_all_pizza_demographics();

  // Transfer money from sender to a bank account. Returns false if the
  // account doesn't exist, or the player can't afford it.
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

  // Achievement milestone checks.
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
  void set_time(const GameTime& t) { time_ = t; }
  void advance_game_time();
  void update(uint32_t elapsed_ms);

  void pause_time_advancement();
  void resume_time_advancement();
  void clock_rate(ClockRate rate) { clock_rate_ = rate; }

  // For headless play:
  void enter_realtime_play();
  void leave_realtime_play();

  // Advance the clock by n ten-minute blocks. Stops at the player's end of day.
  void advance_time_blocks(int n);

  // Editing a restaurant's floor pattern tile by tile is a bonus feature,
  // available from floor_editing_bonus_date().
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
  // Returns whether an arrest was started.
  bool check_police_events();
  void bootstrap_minimal_playable_world(GameContext& context);

  StatusHub& status() { return status_hub_; }
  const StatusHub& status() const { return status_hub_; }

  // Game options - delegates to ConfigManager (single source of truth)
  GameOptions& options();
  const GameOptions& options() const;

  // Fullscreen message manager
  core::events::CareerLog& career_log() { return career_log_; }
  const core::events::CareerLog& career_log() const { return career_log_; }

  // Add the milestone to the player's CV and award its score, once per player.
  // For human players it also schedules the fullscreen message
  // Returns false if this player already reached this milestone
  bool record_milestone(core::events::MilestoneId msg_id,
                        core::PlayerId player_id);

  // Record the milestone and show its fullscreen right away, running on_dismiss
  // once the player closes it. Shows nothing if they have already seen it.
  void deliver_milestone(core::events::MilestoneId msg_id,
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
  std::vector<int> refused_call_actors() const;
  void eliminate_player(core::PlayerId id, GameOverCause cause);
  // Returns false if this player has already won this game.
  bool trigger_win_sequence(core::PlayerId pid);

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
  // Checks the mafia countdowns against today's date. Each one can open a
  // conversation.
  void check_mafia_deadlines();

  // Returns active city if possible, otherwise first loaded city, otherwise
  // city 0.
  core::CityId default_city_id() const;

  // Which city each player is standing in; decides what gets simulated.
  const std::array<std::optional<core::CityId>, NUMBER_OF_PLAYERS>&
  player_cities() const {
    return player_city_;
  }
  void player_city(core::PlayerId pid, std::optional<core::CityId> city_id) {
    player_city_[pid.value] = city_id;
  }

  InterruptionPhase interruption_phase() const { return interruption_phase_; }
  void interruption_phase(InterruptionPhase phase) {
    interruption_phase_ = phase;
  }
  int second_in_block() const { return second_in_block_; }
  void second_in_block(int seconds) { second_in_block_ = seconds; }
  int pit_tick_in_second() const { return pit_tick_in_second_; }
  void pit_tick_in_second(int tick) { pit_tick_in_second_ = tick; }

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
  void process_pending_milestones();

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
  TurnPresenter& turn_presenter();

  void fade_presenter(std::unique_ptr<FadePresenter> presenter);
  FadePresenter& fade_presenter();

private:
  bool is_past_player_end_of_day() const;
  void advance_time_block();
  void advance_second();
  // One turn of the interruption round-robin. Returns whether the player was
  // interrupted.
  bool run_interruption_pass();
  // Applies the guards an interruption must clear, then runs a pass. Returns
  // whether the player was interrupted.
  bool dispatch_interruption();
  // Dispatches on this PIT tick if the screen's rate allows it. Returns
  // whether the player was interrupted.
  bool offer_interruption_tick();
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
  ClockRate clock_rate_{ClockRate::Stopped};
  bool clock_suspended_{false};
  uint32_t pit_accumulator_us_{0};
  // PIT ticks since the last second, wrapping at pit_ticks_per_second.
  int pit_tick_in_second_{0};
  // Seconds into the current ten-minute block.
  int second_in_block_{0};
  InterruptionPhase interruption_phase_{InterruptionPhase::QueuedEvents};
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
  core::newspaper::ArticleDatabase article_database_;
  core::events::CareerLog career_log_;
  BankAccountRegistry bank_accounts_;
  core::food::CustomerTasteProfiles customer_taste_profiles_{
      core::food::default_customer_taste_profiles()};
  WeaponMarket weapon_market_;
  std::unordered_set<int> actors_refusing_calls_;
  // Context pointer for creating popups from GameState
  GameContext* context_{nullptr};
  // UI-layer callbacks; registered once at startup and preserved across
  // reset_for_load.
  StaffEventPresenter staff_event_presenter_;
  MonthlyEntryPresenter monthly_entry_presenter_;
  std::unique_ptr<TurnPresenter> turn_presenter_;
  std::unique_ptr<FadePresenter> fade_presenter_;

  // Global interest rate to influence loans and investments
  int global_interest_rate_{8};
};
