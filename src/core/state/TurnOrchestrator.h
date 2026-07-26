/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/city/MayorOffice.h"
#include "core/common/Ids.h"
#include "core/restaurant/StaffEventData.h"
#include "core/state/TurnPipeline.h"
#include "core/util/Constants.h"

#include <array>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

class City;
class GameState;
class Restaurant;

// Owns the end-of-turn pipeline and the deferred event queues (staff events,
// mayor inspections/awards, manager alerts) that fill up during turn
// processing and are delivered on the player's next day.
class TurnOrchestrator {
public:
  explicit TurnOrchestrator(GameState& state) : gs_(&state) {}

  // Ends the active player's turn: builds the restaurant processing steps
  // and starts draining the pipeline.
  void end_of_turn();
  // Defer end_of_turn to the next GameState::update() tick.
  void request_end_of_turn();
  void skip_to_next_player(); // jail/holiday release: non-interactive
  void on_player_turn_card_finished();

  // Drain the end-of-turn pipeline until a step yields to the UI. Async
  // steps resume by calling this from their completion callback.
  void advance_pipeline();

  // GameState::update() hook: returns true while a Delay step is counting
  // down, meaning game time must not advance this tick.
  bool tick_pipeline_delay();
  // GameState::update() hook: runs a requested end_of_turn.
  void run_pending_end_of_turn();

  // Deliver due deferred events; called once per 10-minute block.
  void fire_due_events();

  // Run a city's start-of-day work (newspaper, mayor scheduling) once per
  // calendar day; also called by GameState when a city becomes active.
  void run_city_start_of_day(City& city);

  // Returns true if it ran (first time of the day), false for any subsequent
  // invocations.
  bool begin_restaurant_day(core::CityId city, core::PlayerId player);

  [[nodiscard]] bool is_player_day_over() const;
  void prevent_auto_end_of_turn(bool prevent);
  [[nodiscard]] bool is_auto_end_of_turn_prevented() const {
    return prevent_auto_end_of_turn_;
  }
  [[nodiscard]] bool is_processing_end_of_turn() const {
    return processing_end_of_turn_;
  }
  // True while the turn pipeline is running and not parked on a step that
  // waits for player input; the turn lock layer blocks pointer interaction
  // while set.
  [[nodiscard]] bool is_pipeline_blocking_input() const;
  // Caption the turn lock layer shows while input is blocked.
  [[nodiscard]] std::string_view pipeline_activity() const {
    return pipeline_activity_;
  }

  // Mayor inspection events deferred from EOD to the next day.
  struct DeferredInspectionEvent {
    bool is_first;
    core::RestaurantId restaurant_id;
  };

  // Staff event dialogs deferred from EOD to the next day.
  struct DeferredStaffEvent {
    core::restaurant::StaffEventData event;
    core::RestaurantId restaurant_id;
  };

  // Manager call-me alerts. Each carries the time block it is due, and at most
  // one is delivered per block.
  struct DeferredManagerAlert {
    core::RestaurantId restaurant_id;
    std::string message;
    int display_block;
  };

  // Protection racket offers rolled during the day-end flush, delivered on
  // the owning player's next turn.
  struct DeferredRacketOffer {
    core::PlayerId player_id;
    core::RestaurantId restaurant_id;
    bool first_contact;
  };

  void queue_manager_alert(core::RestaurantId rid, std::string message,
                           int display_block);
  void drop_manager_alerts_outside(core::CityId city);
  void queue_mayor_award(const MayorAwardEvent& ev);

  // Queue a mayor inspection and show it immediately (debug use).
  void trigger_mayor_inspection(core::RestaurantId rid, bool is_first);

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
  const std::vector<DeferredRacketOffer>& deferred_racket_offers() const {
    return deferred_racket_offers_;
  }
  void restore_deferred_racket_offer(const DeferredRacketOffer& offer) {
    deferred_racket_offers_.push_back(offer);
  }

private:
  turn::StepResult execute_pipeline_step(const turn::Step& step);
  turn::StepResult process_restaurant_step(const turn::ProcessRestaurant& step);
  turn::StepResult finish_end_of_turn(const turn::FinishTurn& step);
  void build_turn_pipeline(bool wrapped, core::PlayerId prev_player_id,
                           std::vector<core::RestaurantId> prev_restaurant_ids,
                           bool prev_is_human);
  void snapshot_weekly_profits(
      const std::vector<core::RestaurantId>& restaurant_ids);
  bool is_restaurant_in_processing_scope(const Restaurant& rest,
                                         bool owner_is_human) const;
  void next_player();
  void show_next_deferred_staff_event();
  void show_next_deferred_manager_alert();
  void show_next_deferred_inspection();

  // Day boundary processing, run as pipeline steps after all players have
  // finished their turns.
  void end_of_day();
  void tick_active_cities();
  void end_of_week_processing();
  void end_of_month_processing();
  void end_of_year_processing();
  void process_depot_theft();
  void deliver_depot_stock(core::PlayerId player_id);

  GameState* gs_;

  // cleared at start of each calendar day, set when run_city_start_of_day
  // fires
  std::array<bool, NUMBER_OF_CITIES> city_start_of_day_ran_{};

  // Make sure we run begin_restaurant_day only once per day.
  std::array<std::array<bool, NUMBER_OF_PLAYERS>, NUMBER_OF_CITIES>
      restaurant_day_ran_{};

  // End-of-turn pipeline: built in end_of_turn() (restaurant steps) and
  // build_turn_pipeline() (day/turn boundary steps), drained by
  // advance_pipeline().
  std::deque<turn::Step> turn_pipeline_;
  // Countdown for an active Delay step; tick_pipeline_delay() resumes the
  // pipeline at 0.
  int pipeline_delay_frames_{0};
  // True from end_of_turn() until FinishTurn runs; gates deferred event
  // delivery in fire_due_events().
  bool processing_end_of_turn_{false};
  // True while the pipeline is parked on a step that waits for player input
  // (report screens, competitions, invoices); timer waits (Delay,
  // FadeTransition) leave it false.
  bool pipeline_waiting_for_player_{false};

  static constexpr std::string_view autosave_activity = "Saving...";
  // Named only while a step blocks the loop; the lock layer draws no caption
  // while it is empty.
  std::string_view pipeline_activity_;

  bool should_end_of_turn_{false};
  bool suppress_end_of_turn_reports_{false};
  bool prevent_auto_end_of_turn_{false};
  // Track whether the current player turn card is for an AI that should
  // auto-end
  bool pending_ai_auto_end_turn_{false};

  std::vector<DeferredStaffEvent> deferred_staff_events_;
  bool showing_deferred_staff_event_{false};
  // Set while end_of_turn() drains un-fired staff events; the dialog chain
  // re-enters end_of_turn() when it empties.
  bool flush_staff_events_then_end_turn_{false};

  std::vector<DeferredManagerAlert> deferred_manager_alerts_;
  bool showing_manager_alert_{false};

  std::vector<DeferredRacketOffer> deferred_racket_offers_;

  std::vector<DeferredInspectionEvent> deferred_mayor_inspections_;
  bool showing_deferred_inspection_{false};

  // Mayor award events from end_of_day() queued for delivery on the
  // active player's next turn.
  std::vector<MayorAwardEvent> deferred_mayor_awards_;
};
