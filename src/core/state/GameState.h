/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/character/Player.h"
#include "core/city/City.h"
#include "core/events/FullscreenMessageManager.h"
#include "core/init/NewGameConfig.h"
#include "core/marketing/AdvertisingCampaign.h"
#include "core/newspaper/ArticleDatabase.h"
#include "core/restaurant/FurnitureBatch.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameOptions.h"
#include "core/state/UIStateStack.h"
#include "core/status/StatusHub.h"
#include "core/time/GameTime.h"
#include "core/util/Constants.h"
#include "engine/DisplayConstants.h"
#include "engine/geom/Spaces.h"
#include "views/screens/city_view/CityViewMapState.h"

#include <array>
#include <deque>
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
  bool spend_cash(Player& player, Account to, Money amount);
  bool spend_cash(Account to, Money amount); // Uses active player
  bool spend_cash(Account to, Money amount, core::RestaurantId restaurant_id);

  core::food::IngredientDemand
  total_ingredient_need_for_player(core::PlayerId player_id) const;

  // Deduct staff wages for a restaurant (called from invoice popup on dismiss)
  void deduct_staff_wages_for_restaurant(core::RestaurantId restaurant_id);

  // Show the staff invoice popup for a restaurant (creates via PopupManager)
  void show_staff_invoice_popup(core::RestaurantId restaurant_id);

  // Show the monthly costs invoice popup (loans + rent) via PopupManager
  void show_monthly_invoice_popup(core::PlayerId player);

  // City  management
  core::CityId active_city_id() const;
  ViewPoint city_position() const;
  std::optional<int> last_clicked_property_index() const;
  City* active_city() const;
  City* city(core::CityId city_id) const;
  std::vector<City*> visited_cities() const; // Returns all non-null cities
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
  void city_view_view_state(const CityViewMapState);

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
  void clear_restaurants_for_load();

  // Real Estate management
  bool buy_restaurant(Property&, Player&);
  Restaurant* rent_restaurant(Property&, Player&, bool free = false);
  bool buy_depot(Property&, Player&);
  bool rent_depot(Property&, Player&, bool free = false);
  bool sell_property(Property&, Player&);
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
  void end_of_day();
  void request_end_of_turn();
  void update(uint32_t elapsed_ms);

  // Time advancement control (for pausing during furniture placement, etc.)
  void pause_time_advancement();
  void resume_time_advancement();
  [[nodiscard]] bool is_time_advancement_enabled() const {
    return time_advancement_enabled_;
  }
  [[nodiscard]] bool is_past_closing_time() const;
  void prevent_end_of_day(bool prevent) {
    prevent_end_of_day_ = prevent;
    if (!prevent_end_of_day_) {
      if (is_past_closing_time()) {
        request_end_of_turn();
      }
    }
  }
  bool allow_end_of_day() const { return prevent_end_of_day_; }

  // Advance time by n blocks (10 min each) - used for explicit time costs
  // (furniture placement, phone calls, etc.). Stops early if closing time
  // is reached. Returns true if closing time was reached.
  bool advance_time_blocks(int n);

  int difficulty_level() const { return difficulty_level_; };

  // Game Events
  void initiate_arrest(ArrestReason);
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

  // Called when PlayerTurnCard finishes displaying
  void on_player_turn_card_finished();

  // Called when a restaurant's end-of-turn processing is complete
  // (either after daily report is dismissed, or immediately if not shown)
  void on_restaurant_processed();

  GameMode game_mode() const;
  int human_player_count() const;

private:
  void check_market_share_milestones(const City& city);
  void process_next_restaurant();
  void finish_end_of_turn();
  void deliver_depot_stock(core::PlayerId player_id);

  // State for multi-step end-of-turn processing
  struct EndOfTurnState {
    std::vector<Restaurant*> restaurants;
    size_t current_index = 0;
    bool is_human = false;
    int delay_frames = 0; // Frames to wait before processing next restaurant
  };
  std::optional<EndOfTurnState> end_of_turn_state_;

  void load_initial_pizzas(GameContext& context, core::PlayerId player_id);

  GameTime time_;
  core::PlayerId active_player_{0};
  core::CityId active_city_{-1};
  ViewPoint city_position_{ViewPoint{-1, -1}};
  std::optional<core::DistrictId> active_district_id_;
  // The original only had one view state so whatever the previous player
  // did would be inherited by the next player. We do it per player:
  std::array<CityViewMapState, NUMBER_OF_PLAYERS> city_view_view_state_;
  std::array<std::unique_ptr<City>, NUMBER_OF_CITIES> cities_;
  std::array<std::unique_ptr<Player>, NUMBER_OF_PLAYERS> players_;
  std::deque<std::unique_ptr<Restaurant>> restaurants_;
  // List of states and their arguments, so we can go back to any previous state
  UIStateStack ui_;
  int window_width_{0};
  int window_height_{0};
  std::optional<int> last_clicked_property_index_;
  bool should_end_of_turn_{false};
  bool time_advancement_enabled_{true};
  bool prevent_end_of_day_{false};
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
  std::unique_ptr<StatusDisplay> status_display_; // Global status panel (lazy)

  // Resolution change request
  std::optional<ScreenResolution> pending_resolution_;

  // Fade transition request
  bool fade_requested_{false};
  int fade_duration_frames_{default_fade_length};

  // Article database (shared across all cities)
  core::newspaper::ArticleDatabase article_database_;

  // Fullscreen message system
  core::events::FullscreenMessageManager message_manager_;

  // Actor call management (actors refusing calls after being annoyed)
  std::unordered_set<int> actors_refusing_calls_;

  // Context pointer for creating popups from GameState
  GameContext* context_{nullptr};

  // Track whether the current player turn card is for an AI that should
  // auto-end
  bool pending_ai_auto_end_turn_{false};
  // Not quite week number, we start at 1 at start of the game (1st of March)
  // then reset it every year.
  int business_week_nr{1};
};
