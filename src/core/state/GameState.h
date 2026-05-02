/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/character/Player.h"
#include "core/city/City.h"
#include "core/events/FullscreenMessageManager.h"
#include "core/finance/BankAccountRegistry.h"
#include "core/food/CustomerTasteProfiles.h"
#include "core/init/NewGameConfig.h"
#include "core/marketing/AdvertisingCampaign.h"
#include "core/newspaper/ArticleDatabase.h"
#include "core/restaurant/FurnitureBatch.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/AnnualCompetitionFlow.h"
#include "core/state/CompetitionSession.h"
#include "core/state/GameOptions.h"
#include "core/state/TurnPipeline.h"
#include "core/state/UIStateStack.h"
#include "core/status/StatusHub.h"
#include "core/time/GameTime.h"
#include "core/util/Constants.h"
#include "engine/DisplayConstants.h"
#include "engine/geom/Spaces.h"
#include "views/screens/city_view/CityViewMapState.h"

#include <array>
#include <deque>
#include <functional>
#include <optional>
#include <unordered_map>
#include <unordered_set>

class Player;
class Property;
class GameContext;
class StatusDisplay;

constexpr int default_fade_length = 15;

class GameState {
  friend std::ostream& operator<<(std::ostream& os, const GameState& state);

public:
  GameState();
  ~GameState();

  // Prevent copying/moving due to unique_ptr members with incomplete types
  GameState(const GameState&) = delete;
  GameState& operator=(const GameState&) = delete;
  GameState(GameState&&) = delete;
  GameState& operator=(GameState&&) = delete;

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

  // Game setup
  void begin_initialization();
  init::Config& initializer();
  void finalize_initialization(GameContext& context);

  // Player management
  std::array<Player*, NUMBER_OF_PLAYERS> players() const;
  Player* active_player() const; // returns active player
  Player* player(core::PlayerId player_id) const;
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

  core::food::IngredientDemand
  total_ingredient_need_for_player(core::PlayerId player_id) const;

  // Deduct staff wages for a restaurant (called from invoice popup on dismiss)
  void deduct_staff_wages_for_restaurant(core::RestaurantId restaurant_id);

  // Show the staff invoice popup for a restaurant (creates via PopupManager)
  void show_staff_invoice_popup(core::RestaurantId restaurant_id,
                                std::function<void()> after);

  // Show the monthly costs invoice popup (loans + rent) via PopupManager.
  // on_done is called after the popup is dismissed (optional).
  void show_monthly_invoice_popup(core::PlayerId player,
                                  std::function<void()> on_done = {});
  void run_monthly_competition();
  void run_annual_pizza_competition();
  void finalize_annual_pizza_competition(CompetitionSession session);

  // Show weekly city profit invoice for all non-active cities (Saturday only).
  // on_done is called after the popup is dismissed (optional).
  void show_weekly_city_invoice_popup(core::PlayerId player_id,
                                      std::function<void()> on_done = {});

  // Advance the turn boundary pipeline to the next step.
  void advance_pipeline();

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
  Restaurant* restaurant(core::RestaurantId r_id) const;
  std::optional<core::RestaurantId>
  next_restaurant(core::PlayerId pid, core::RestaurantId from) const;
  std::optional<core::RestaurantId>
  prev_restaurant(core::PlayerId pid, core::RestaurantId from) const;
  // For save/load:
  void add_restaurant(std::unique_ptr<Restaurant> restaurant);
  void remove_restaurant(core::RestaurantId r_id);
  void clear_restaurants_for_load();

  // Real Estate management
  bool buy_restaurant(Property&, Player&, Money price);
  Restaurant* rent_restaurant(Property&, Player&, bool free = false);
  bool buy_depot(Property&, Player&);
  bool rent_depot(Property&, Player&, bool free = false);
  bool sell_property(Property&, Player&, Money sale_price);
  bool end_property_lease(Property&, Player&);
  Restaurant* create_restaurant(Player& player, Property& property);
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
  void end_of_turn(); // per player
  void request_end_of_turn();
  void skip_to_next_player(); // jail/holiday release: non-interactive
  void update(uint32_t elapsed_ms);

  // Time advancement control (for pausing during furniture placement, etc.)
  void pause_time_advancement();
  void resume_time_advancement();
  [[nodiscard]] bool is_time_advancement_enabled() const {
    return time_advancement_enabled_;
  }
  [[nodiscard]] bool is_player_day_over() const;
  void prevent_auto_end_of_turn(bool prevent) {
    prevent_auto_end_of_turn_ = prevent;
    if (!prevent_auto_end_of_turn_) {
      // Show any staff event dialogs for today (or earlier if they ended the
      // day before 10am then)
      if (!showing_deferred_staff_event_ && !deferred_staff_events_.empty() &&
          time_.is_past_time(10, 0)) {
        show_next_deferred_staff_event();
        // end of turn will be checked after the dialog chain finishes
        return;
      }
      if (is_player_day_over()) {
        request_end_of_turn();
      }
    }
  }
  bool is_auto_end_of_turn_prevented() const {
    return prevent_auto_end_of_turn_;
  }

  // Advance time by n blocks (10 min each) - used for explicit time costs
  // (furniture placement, phone calls, etc.). Stops early if end-of-day
  // is reached.
  void advance_time_blocks(int n);

  int difficulty_level() const { return difficulty_level_; };

  RankChange adjust_underworld_score(Player& player, int delta);
  RankChange adjust_social_score(Player& player, int delta);

  // Game Events
  void initiate_arrest(ArrestReason);
  void check_police_events();
  void bootstrap_minimal_playable_world(GameContext& context);

  // Create a restaurant with a specific layout in a random available property
  Restaurant* create_restaurant_with_layout(Player& player, City& city,
                                            int layout_id, bool is_big);

  StatusHub& status() { return status_hub_; }
  const StatusHub& status() const { return status_hub_; }

  // Global StatusDisplay panel (lazy initialized on first access)
  StatusDisplay* status_display(GameContext& context);

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
  void next_player();

  void on_player_turn_card_finished();

  // Called when a restaurant's end-of-turn processing is complete
  // (either after daily report is dismissed, or immediately if not shown)
  void on_restaurant_processed();

  GameMode game_mode() const;
  void game_mode(GameMode);
  int human_player_count() const;

  // Call when a restaurant is opened for the first time
  // Checks and schedules restaurant count / city milestone events.
  void check_restaurant_milestones(const Player& player);

  void on_city_entered(core::CityId city_id, const Player& player);
  void check_mafia_events();

  // Process all pending messages for the active player immediately:
  // award score, push fullscreen if needed, mark as shown.
  void process_pending_messages();

  // Drive the full staff event dialog chain (AlertPopup -> OptionsPopup ->
  // reply AlertPopup). Pass a no-op on_done for debug use.
  void show_staff_event_dialog(const core::restaurant::StaffEventData& event,
                               Restaurant* restaurant,
                               std::function<void()> on_done);

  // Queue a mayor inspection and show it immediately (debug use).
  void trigger_mayor_inspection(core::RestaurantId rid, bool is_first);

  // Push a mayor award event directly to the UI (debug use).
  void process_mayor_award_event(const MayorAwardEvent& award_event);

  // Mayor inspection events deferred from EOD to next-day 10 AM.
  struct DeferredInspectionEvent {
    bool is_first;
    core::RestaurantId restaurant_id;
  };

  // Staff event dialogs deferred from EOD to next-day 10 AM.
  struct DeferredStaffEvent {
    core::restaurant::StaffEventData event;
    core::RestaurantId restaurant_id;
  };

  // Save/load access to deferred queues. restore_* push without triggering.
  const std::vector<DeferredStaffEvent>& deferred_staff_events() const {
    return deferred_staff_events_;
  }
  void restore_deferred_staff_event(const core::restaurant::StaffEventData& ev,
                                    core::RestaurantId rid) {
    deferred_staff_events_.push_back({ev, rid});
  }
  const std::vector<DeferredInspectionEvent>&
  deferred_mayor_inspections() const {
    return deferred_mayor_inspections_;
  }
  void restore_deferred_mayor_inspection(core::RestaurantId rid,
                                         bool is_first) {
    deferred_mayor_inspections_.push_back({is_first, rid});
  }
  const std::vector<MayorAwardEvent>& deferred_mayor_awards() const {
    return deferred_mayor_awards_;
  }
  void restore_deferred_mayor_award(const MayorAwardEvent& ev) {
    deferred_mayor_awards_.push_back(ev);
  }
  int competition_count() const { return competition_count_; }
  void increment_competition_count() {
    competition_count_ = std::min(competition_count_ + 1, 255);
  }

private:
  bool is_past_player_end_of_day() const;
  void check_market_share_milestones(const City& city);
  void run_city_start_of_day(City& city);
  void process_next_restaurant();
  void show_next_deferred_staff_event();
  void show_next_deferred_inspection();
  void finish_end_of_turn();
  void deliver_depot_stock(core::PlayerId player_id);

  // finish_end_of_turn helpers
  void snapshot_weekly_profits();
  void
  push_weekly_restaurant_reports(const std::vector<Restaurant*>& restaurants,
                                 core::PlayerId previous_player_id,
                                 std::function<void()> on_done = {});

  // Turn boundary pipeline
  void build_turn_pipeline(bool wrapped, core::PlayerId prev_player_id,
                           const std::vector<Restaurant*>& prev_restaurants,
                           bool prev_is_human);
  void execute_pipeline_step(TurnPipelineStep step);

  // end_of_day helpers
  void end_of_day();
  void tick_active_cities();
  void end_of_week_processing();
  void end_of_year_processing();
  void end_of_month_processing();
  void process_depot_theft();

  // State for multi-step end-of-turn processing
  struct EndOfTurnState {
    std::vector<Restaurant*> restaurants;
    size_t current_index = 0;
    bool is_human = false;
    int delay_frames = 0; // Frames to wait before processing next restaurant
  };
  std::optional<EndOfTurnState> end_of_turn_state_;

  // Turn boundary pipeline: built once per day boundary in
  // build_turn_pipeline(), drained one step at a time via advance_pipeline().
  std::deque<TurnPipelineStep> turn_pipeline_;
  std::optional<TurnPipelineContext> pipeline_ctx_;
  std::optional<TurnPipelineStep> pipeline_active_step_;

  std::vector<DeferredStaffEvent> deferred_staff_events_;
  bool showing_deferred_staff_event_{false};

  std::vector<DeferredInspectionEvent> deferred_mayor_inspections_;
  bool showing_deferred_inspection_{false};

  // Mayor award events from end_of_day() queued for delivery on the
  // active player's next turn.
  std::vector<MayorAwardEvent> deferred_mayor_awards_;

  AnnualCompetitionFlow annual_comp_;

  void load_initial_pizzas(GameContext& context, core::PlayerId player_id);
  Restaurant* create_ai_starting_restaurant(Player& player, City& city);
  void
  run_monthly_competition_chain(std::vector<core::PlayerId> remaining,
                                std::shared_ptr<CompetitionSession> session);
  void launch_monthly_competition(std::shared_ptr<CompetitionSession> session);
  void advance_annual_competition();
  [[nodiscard]] bool annual_comp_ready_to_advance() const;

  GameTime time_;
  core::PlayerId active_player_{0};
  core::CityId active_city_{-1};
  ViewPoint city_position_{ViewPoint{-1, -1}};
  std::optional<core::DistrictId> active_district_id_;
  // Per-player city view state, persisted across screen transitions.
  std::array<CityViewMapState, NUMBER_OF_PLAYERS> city_view_view_state_;
  std::array<std::unique_ptr<City>, NUMBER_OF_CITIES> cities_;
  std::array<std::unique_ptr<Player>, NUMBER_OF_PLAYERS> players_;
  std::deque<std::unique_ptr<Restaurant>> restaurants_;
  UIStateStack ui_;
  int window_width_{0};
  int window_height_{0};
  std::optional<int> last_clicked_property_index_;
  bool should_end_of_turn_{false};
  bool suppress_end_of_turn_reports_{false};
  bool time_advancement_enabled_{false};
  bool prevent_auto_end_of_turn_{false};
  uint32_t time_accumulator_{0};
  std::optional<core::RestaurantId> active_restaurant_;
  GameMode game_mode_;
  std::optional<init::Config> initializer_;
  int difficulty_level_{1};
  std::unordered_map<
      int, std::unordered_map<AdvertisingChannel, AdvertisingCampaign>>
      advertising_campaigns_;
  bool quit_{false};
  core::RestaurantId next_restaurant_id_{0};
  std::vector<FurnitureBatch> furniture_placement_;
  StatusHub status_hub_{time_};
  std::unique_ptr<StatusDisplay> status_display_;
  std::optional<ScreenResolution> pending_resolution_;
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

  // Track whether the current player turn card is for an AI that should
  // auto-end
  bool pending_ai_auto_end_turn_{false};
  // cleared at start of each calendar day, set when run_city_start_of_day fires
  std::array<bool, NUMBER_OF_CITIES> city_start_of_day_ran_{};
  // Not quite week number, we start at 1 at start of the game (1st of March)
  // then reset it every year.
  int business_week_nr{1};
  // Global interest rate to influence loans and investments
  int global_interest_rate_{8};
  int competition_count_{0};
};
