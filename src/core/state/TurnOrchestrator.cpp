/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/state/TurnOrchestrator.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "conversations/MayorConversation.h"
#include "core/Demographics.h"
#include "core/character/Actor.h"
#include "core/character/Player.h"
#include "core/city/City.h"
#include "core/city/Property.h"
#include "core/events/LeventId.h"
#include "core/food/IngredientCategory.h"
#include "core/food/IngredientType.h"
#include "core/restaurant/AIEconomy.h"
#include "core/restaurant/Restaurant.h"
#include "core/save/SaveGameManager.h"
#include "core/state/GameOptions.h"
#include "core/state/GameState.h"
#include "core/state/Overloaded.h"
#include "core/state/TurnPresenter.h"
#include "core/state/UIState.h"
#include "core/status/StatusHub.h"
#include "core/time/TimeOfDay.h"
#include "engine/Logger.h"
#include "engine/Random.h"
#include "factories/CharacterFactory.h"

void TurnOrchestrator::request_end_of_turn() { should_end_of_turn_ = true; }

void TurnOrchestrator::skip_to_next_player() {
  suppress_end_of_turn_reports_ = true;
  end_of_turn();
}

void TurnOrchestrator::run_pending_end_of_turn() {
  if (should_end_of_turn_) {
    end_of_turn();
    should_end_of_turn_ = false;
  }
}

bool TurnOrchestrator::tick_pipeline_delay() {
  if (pipeline_delay_frames_ <= 0) {
    return false;
  }
  if (--pipeline_delay_frames_ == 0) {
    advance_pipeline();
  }
  return true;
}

bool TurnOrchestrator::is_player_day_over() const {
  if (prevent_auto_end_of_turn_) {
    return false;
  }
  // AI turns end when their turn card auto-dismisses, never by the clock.
  auto* player = gs_->active_player();
  if (!player || !player->is_human_controlled()) {
    return false;
  }
  return gs_->is_past_player_end_of_day();
}

void TurnOrchestrator::prevent_auto_end_of_turn(bool prevent) {
  prevent_auto_end_of_turn_ = prevent;
  if (!prevent_auto_end_of_turn_) {
    // Show any staff event dialogs for today (or earlier if they ended the
    // day before 10am then)
    if (!showing_deferred_staff_event_ && !deferred_staff_events_.empty() &&
        gs_->time().is_past_time(10, 0)) {
      show_next_deferred_staff_event();
      // end of turn will be checked after the dialog chain finishes
      return;
    }
    if (is_player_day_over()) {
      request_end_of_turn();
    }
  }
}

void TurnOrchestrator::end_of_turn() {
  gs_->pause_time_advancement();

  auto* current_player = gs_->active_player();

  bool interactive = current_player->is_human_controlled() &&
                     !current_player->is_on_holiday() &&
                     !current_player->is_in_jail() &&
                     !suppress_end_of_turn_reports_;
  suppress_end_of_turn_reports_ = false;
  if (!interactive) {
    gs_->status().suppress(true);
  }

  // The original's day-end flush loop passes the protection racket roll about
  // twice, so ending the day early never fully escapes the mafia. A fired
  // offer is delivered on this player's next turn.
  if (interactive) {
    for (int i = 0; i < 2; ++i) {
      if (auto offer = gs_->roll_protection_racket()) {
        deferred_racket_offers_.push_back(
            {.player_id = current_player->id(),
             .restaurant_id = offer->restaurant_id,
             .first_contact = offer->first_contact});
      }
    }
  }

  // Player end of day accounting
  current_player->end_of_day(*gs_);

  const bool electricity_invoice_due =
      gs_->time().current_date().is_end_of_month() &&
      !gs_->time().current_date().is_end_of_week();

  // FinishTurn receives the full restaurant list for the weekly reports,
  // but only in-scope restaurants are actually processed.
  const bool is_human = current_player->is_human_controlled();
  std::vector<core::RestaurantId> all_ids;
  std::vector<core::RestaurantId> in_scope_ids;
  for (auto* rest : gs_->restaurants_for(current_player->id())) {
    if (!rest) {
      continue;
    }
    all_ids.push_back(rest->id());
    if (is_restaurant_in_processing_scope(*rest, is_human)) {
      in_scope_ids.push_back(rest->id());
    }
  }

  constexpr int delay_frames_between_restaurants = 2;

  turn_pipeline_.clear();
  for (std::size_t i = 0; i < in_scope_ids.size(); ++i) {
    turn_pipeline_.emplace_back(
        turn::ProcessRestaurant{in_scope_ids[i], interactive});
    if (interactive && electricity_invoice_due) {
      turn_pipeline_.emplace_back(
          turn::MonthlyRestaurantInvoice{in_scope_ids[i]});
    }
    // Delay is so that humans can see the amount of cash they have change live,
    // per restaurant that gets processed (even if automatic reports is off).
    if (interactive && i + 1 < in_scope_ids.size()) {
      turn_pipeline_.emplace_back(
          turn::Delay{delay_frames_between_restaurants});
    }
  }
  turn_pipeline_.emplace_back(
      turn::FinishTurn{std::move(all_ids), interactive});

  processing_end_of_turn_ = true;
  advance_pipeline();
}

turn::StepResult
TurnOrchestrator::process_restaurant_step(const turn::ProcessRestaurant& step) {
  auto* restaurant = gs_->restaurant(step.id);
  if (!restaurant) {
    return turn::StepResult::Done;
  }

  restaurant->end_of_day(*gs_);
  restaurant->property().calculate_demographics(restaurant);

  gs_->status().refresh_header();

  if (step.interactive) {
    // Queue mayor inspection event for delivery next day at 10:00.
    if (auto insp = restaurant->take_pending_inspection()) {
      deferred_mayor_inspections_.push_back({*insp, restaurant->id()});
    }

    // Queue staff event dialogs for next day at 10:00
    // Replace any existing queued event for the same staff member so that
    // only the most recent event survives
    for (auto& ev : restaurant->take_pending_staff_events()) {
      auto it = std::ranges::find_if(
          deferred_staff_events_, [&](const DeferredStaffEvent& d) {
            return d.event.job_role == ev.job_role &&
                   d.event.slot_index == ev.slot_index &&
                   d.restaurant_id == restaurant->id();
          });
      if (it != deferred_staff_events_.end()) {
        Logger::debug("[staff_event] replace queued event: rest={} role={} "
                      "slot={} old_type={} new_type={}",
                      restaurant->id().value, static_cast<int>(ev.job_role),
                      ev.slot_index, static_cast<int>(it->event.type),
                      static_cast<int>(ev.type));
        it->event = ev;
      }
      else {
        Logger::debug("[staff_event] enqueue: rest={} role={} slot={} "
                      "type={} queue_size_after={}",
                      restaurant->id().value, static_cast<int>(ev.job_role),
                      ev.slot_index, static_cast<int>(ev.type),
                      deferred_staff_events_.size() + 1);
        deferred_staff_events_.push_back({ev, restaurant->id()});
      }
    }

    // The daily report's dismiss button resumes the pipeline.
    if (!gs_->options().automatic_report && restaurant->wants_open() &&
        !restaurant->is_force_closed()) {
      gs_->ui().push<RestaurantStatsState>(restaurant->id(), std::nullopt,
                                           true);
      return turn::StepResult::Waiting;
    }
  }

  return turn::StepResult::Done;
}

void TurnOrchestrator::show_next_deferred_staff_event() {
  if (deferred_staff_events_.empty()) {
    showing_deferred_staff_event_ = false;
    // The dialog chain may have been started from the
    // prevent_auto_end_of_turn setter, which deferred the end-of-turn
    // check. Now that all dialogs are done, check if we should end the turn.
    if (is_player_day_over()) {
      request_end_of_turn();
    }
    return;
  }
  showing_deferred_staff_event_ = true;

  auto deferred = deferred_staff_events_.front();
  deferred_staff_events_.erase(deferred_staff_events_.begin());

  Logger::debug(
      "[staff_event] dequeue for display: rest={} role={} slot={} type={} "
      "remaining={}",
      deferred.restaurant_id.value, static_cast<int>(deferred.event.job_role),
      deferred.event.slot_index, static_cast<int>(deferred.event.type),
      deferred_staff_events_.size());

  auto* restaurant = gs_->restaurant(deferred.restaurant_id);
  if (!restaurant) {
    Logger::debug("[staff_event] skip (restaurant gone): rest={}",
                  deferred.restaurant_id.value);
    // Restaurant gone, skip and try next.
    show_next_deferred_staff_event();
    return;
  }

  gs_->show_staff_event_dialog(deferred.event, restaurant,
                               [this]() { show_next_deferred_staff_event(); });
}

void TurnOrchestrator::queue_manager_alert(core::RestaurantId rid,
                                           std::string message) {
  deferred_manager_alerts_.push_back({rid, std::move(message)});
}

void TurnOrchestrator::queue_mayor_award(const MayorAwardEvent& ev) {
  deferred_mayor_awards_.push_back(ev);
}

void TurnOrchestrator::show_next_deferred_manager_alert() {
  if (deferred_manager_alerts_.empty()) {
    return;
  }

  auto alert = deferred_manager_alerts_.front();
  deferred_manager_alerts_.erase(deferred_manager_alerts_.begin());

  auto* restaurant = gs_->restaurant(alert.restaurant_id);
  const RawRenderable* manager_pic = nullptr;
  if (restaurant) {
    const auto& mgr = restaurant->manager();
    if (mgr) {
      manager_pic = CharacterFactory::create_character(mgr->character().id());
    }
  }

  gs_->turn_presenter().show_manager_alert(
      std::move(alert.message), manager_pic,
      [this]() { show_next_deferred_manager_alert(); });
}

void TurnOrchestrator::trigger_mayor_inspection(core::RestaurantId rid,
                                                bool is_first) {
  deferred_mayor_inspections_.push_back({is_first, rid});
  if (!showing_deferred_inspection_) {
    show_next_deferred_inspection();
  }
}

void TurnOrchestrator::show_next_deferred_inspection() {
  if (deferred_mayor_inspections_.empty()) {
    showing_deferred_inspection_ = false;
    if (is_player_day_over()) {
      request_end_of_turn();
    }
    return;
  }

  auto* city = gs_->active_city();
  if (!city) {
    deferred_mayor_inspections_.clear();
    showing_deferred_inspection_ = false;
    return;
  }

  auto mayor_actors = city->actors(ActorType::MAYOR);
  if (mayor_actors.empty()) {
    deferred_mayor_inspections_.clear();
    showing_deferred_inspection_ = false;
    return;
  }
  int mayor_id = mayor_actors[0]->id();

  auto deferred = deferred_mayor_inspections_.front();
  deferred_mayor_inspections_.erase(deferred_mayor_inspections_.begin());
  showing_deferred_inspection_ = true;

  auto* restaurant = gs_->restaurant(deferred.restaurant_id);
  if (!restaurant) {
    // Restaurant gone since EOD, skip.
    showing_deferred_inspection_ = false;
    show_next_deferred_inspection();
    return;
  }

  // Corruption check: if mayor favor score passes, award a star regardless of
  // quality.
  auto* player = gs_->active_player();
  bool corruption_pass = false;
  if (player) {
    int favor = city->mayor_office().favor_score(player->id().value);
    corruption_pass = engine::random::rand_percentage_chance(favor) &&
                      restaurant->star_rating() < 5;
  }

  MayorConversation::InspectionOutcome outcome;
  outcome.is_first = deferred.is_first;

  if (corruption_pass) {
    restaurant->star_rating(restaurant->star_rating() + 1);
    outcome.star_gained = true;
  }
  else {
    auto result = restaurant->update_star_rating();
    if (result.has_value()) {
      outcome.star_gained = *result;
      outcome.star_lost = !*result;
    }
  }

  // Force-close for 7 days on first inspection with star loss (original rule)
  if (outcome.star_lost && outcome.is_first) {
    restaurant->force_close(7);
  }

  std::function<void()> on_complete;
  if (outcome.star_gained) {
    using core::events::LeventId;
    LeventId msg_id = LeventId::AwardedFirstStar;
    switch (restaurant->star_rating()) {
    case 2:
      msg_id = LeventId::FirstTwoStarRestaurant;
      break;
    case 3:
      msg_id = LeventId::FirstThreeStarRestaurant;
      break;
    case 4:
      msg_id = LeventId::FirstFourStarRestaurant;
      break;
    case 5:
      msg_id = LeventId::FirstFiveStarRestaurant;
      break;
    default:
      break;
    }
    on_complete = [this, msg_id]() { gs_->deliver_message(msg_id); };
  }

  MayorConversationType conv_type =
      deferred.is_first ? MayorConversationType::FirstQualityInspection
                        : MayorConversationType::QualityInspection;
  gs_->ui().push<MayorState>(
      MayorState{.id = mayor_id,
                 .type = conv_type,
                 .restaurant_id = deferred.restaurant_id,
                 .is_first_inspection = deferred.is_first,
                 .award_winner_player_id = std::nullopt,
                 .inspection_star_gained = outcome.star_gained,
                 .inspection_star_lost = outcome.star_lost,
                 .on_complete = std::move(on_complete)});
}

void TurnOrchestrator::fire_due_events() {
  // Only called on a human player's turn: time never advances during AI
  // turns (gated in GameState::update()).
  const auto& time = gs_->time();

  // Fire manager call-me alerts at 9:00.
  if (!processing_end_of_turn_ && !prevent_auto_end_of_turn_ &&
      !deferred_manager_alerts_.empty() && time.is_past_time(9, 0)) {
    show_next_deferred_manager_alert();
  }

  // Deliver protection racket offers rolled during this player's day-end
  // flush.
  if (!processing_end_of_turn_ && !prevent_auto_end_of_turn_ &&
      !deferred_racket_offers_.empty()) {
    auto it = std::ranges::find_if(
        deferred_racket_offers_, [&](const DeferredRacketOffer& d) {
          return d.player_id == gs_->active_player()->id();
        });
    if (it != deferred_racket_offers_.end()) {
      GameState::RacketOffer offer{.restaurant_id = it->restaurant_id,
                                   .first_contact = it->first_contact};
      deferred_racket_offers_.erase(it);
      gs_->push_protection_racket_offer(offer);
    }
  }

  // Fire deferred staff event dialogs at 10:00 the next day.
  if (!processing_end_of_turn_ && !showing_deferred_staff_event_ &&
      !showing_deferred_inspection_ && !prevent_auto_end_of_turn_ &&
      !deferred_staff_events_.empty() && time.is_past_time(10, 0)) {
    show_next_deferred_staff_event();
  }

  // Reset inspection gate when the MayorState conversation has ended.
  if (showing_deferred_inspection_ && !gs_->ui().is_top<MayorState>()) {
    showing_deferred_inspection_ = false;
  }

  // Fire deferred mayor inspection events at 10:00 (after staff events drain).
  if (!processing_end_of_turn_ && !showing_deferred_staff_event_ &&
      !showing_deferred_inspection_ && !prevent_auto_end_of_turn_ &&
      !deferred_mayor_inspections_.empty() && time.is_past_time(10, 0)) {
    show_next_deferred_inspection();
  }

  // Fire deferred mayor award events at 10:00 (after inspections).
  if (!processing_end_of_turn_ && !showing_deferred_staff_event_ &&
      !showing_deferred_inspection_ && !gs_->ui().is_top<MayorState>() &&
      !prevent_auto_end_of_turn_ && !deferred_mayor_awards_.empty() &&
      time.is_past_time(10, 0)) {
    auto ev = deferred_mayor_awards_.front();
    deferred_mayor_awards_.erase(deferred_mayor_awards_.begin());
    gs_->process_mayor_award_event(ev);
  }
}

// For human players we only process their currently active city, other cities
// are frozen. AI players have no active city: their restaurants are
// processed in every city where a human player is present.
bool TurnOrchestrator::is_restaurant_in_processing_scope(
    const Restaurant& rest, bool owner_is_human) const {
  auto city_id = rest.property().city().id();
  return owner_is_human ? city_id == gs_->active_city()->id()
                        : gs_->has_human_in_city(city_id);
}

void TurnOrchestrator::snapshot_weekly_profits(
    const std::vector<core::RestaurantId>& restaurant_ids) {
  const bool is_human = gs_->active_player()->is_human_controlled();
  for (auto rid : restaurant_ids) {
    auto* rest = gs_->restaurant(rid);
    if (!rest || !is_restaurant_in_processing_scope(*rest, is_human)) {
      continue;
    }
    Money weekly_total;
    for (const auto& day :
         rest->stats().current_week(gs_->time().current_date())) {
      weekly_total = weekly_total + day.net_profit;
    }
    rest->last_weekly_profit(weekly_total);
  }
}

turn::StepResult
TurnOrchestrator::finish_end_of_turn(const turn::FinishTurn& step) {
  processing_end_of_turn_ = false;

  snapshot_weekly_profits(step.restaurant_ids);

  auto* current_player = gs_->active_player();
  if (current_player && current_player->is_human_controlled()) {
    bool is_eow = gs_->time().current_date().is_end_of_week();
    gs_->milestones().check_milestone_events(*current_player, is_eow);
  }

  if (gs_->active_player()->is_human_controlled()) {
    gs_->status().freeze_header(gs_->active_player()->id());
  }

  auto previous_player_id = gs_->active_player_;
  next_player();

  // AI players don't have an active city
  if (gs_->active_player()->is_human_controlled()) {
    if (auto next_city =
            gs_->city_view_view_state_[gs_->active_player_.value].city_id) {
      if (*next_city != gs_->active_city_) {
        gs_->active_city_ = *next_city;
        gs_->ui().push<CityViewState>(*next_city);
      }
    }
  }

  bool wrapped =
      (previous_player_id.value > 0 && gs_->active_player_.value == 0);

  // The driver keeps draining into the freshly built boundary steps.
  build_turn_pipeline(wrapped, previous_player_id, step.restaurant_ids,
                      step.interactive);
  return turn::StepResult::Done;
}

void TurnOrchestrator::advance_pipeline() {
  while (!turn_pipeline_.empty()) {
    turn::Step step = std::move(turn_pipeline_.front());
    turn_pipeline_.pop_front();
    Logger::debug("TurnPipeline: step {} ({} remaining)", turn::step_name(step),
                  turn_pipeline_.size());
    if (execute_pipeline_step(step) == turn::StepResult::Waiting) {
      return;
    }
  }
}

void TurnOrchestrator::next_player() {
  if (++gs_->active_player_.value >= NUMBER_OF_PLAYERS) {
    gs_->active_player_.value = 0;
  }
}

void TurnOrchestrator::on_player_turn_card_finished() {
  // Only release for human players - keep frozen for AI to hide their money
  if (gs_->active_player()->is_human_controlled()) {
    gs_->status().unfreeze_header();
  }

  if (pending_ai_auto_end_turn_) {
    pending_ai_auto_end_turn_ = false;
    end_of_turn();
    return;
  }

  // Human player taking control: unsuppress
  gs_->status().suppress(false);

  auto* player = gs_->active_player();
  if (player && player->is_in_jail()) {
    player->decrement_jail_day();
    if (player->is_in_jail()) {
      end_of_turn();
    }
    else {
      // Released from prison, just advance to next player
      skip_to_next_player();
    }
    return;
  }
  if (player && player->is_on_holiday()) {
    player->decrement_holiday_day();
    if (player->is_on_holiday()) {
      end_of_turn();
    }
    else {
      skip_to_next_player();
    }
    return;
  }
  StatusMsg m{
      .payload = "Please wait...",
      .scope = StatusScope::Global,
  };
  gs_->status().post_direct(m);
}

void TurnOrchestrator::build_turn_pipeline(
    bool wrapped, core::PlayerId prev_player_id,
    std::vector<core::RestaurantId> prev_restaurant_ids, bool prev_is_human) {
  auto* new_player = gs_->players_[gs_->active_player_.value].get();
  // Clock only runs for human players, but AdvanceDate still needs a
  // time-of-day for the new date so for AI players we just use the default
  // opening time.
  const bool new_player_is_human =
      new_player && new_player->is_human_controlled();
  auto start_time = new_player_is_human ? new_player->start_of_day_time()
                                        : Restaurant::DEFAULT_OPENING_TIME;

  bool is_end_of_week = gs_->time().current_date().is_end_of_week();
  bool is_end_of_month = gs_->time().current_date().is_end_of_month();
  bool is_end_of_year = gs_->time().current_date().is_end_of_year();

  turn_pipeline_.clear();

  if (wrapped) {
    turn_pipeline_.push_back(turn::TickCities{});
    if (is_end_of_week) {
      turn_pipeline_.push_back(turn::EndOfWeekProcessing{});
    }
    if (is_end_of_year) {
      turn_pipeline_.push_back(turn::EndOfYearProcessing{});
    }
    if (is_end_of_month) {
      turn_pipeline_.push_back(turn::EndOfMonthProcessing{});
    }
    if (is_end_of_year) {
      turn_pipeline_.push_back(turn::AnnualCompetition{});
    }
    if (is_end_of_month) {
      turn_pipeline_.push_back(turn::MonthlyCompetition{});
    }
    turn_pipeline_.push_back(turn::FadeTransition{});
    turn_pipeline_.push_back(turn::AdvanceDate{start_time});
    bool autosave_due = false;
    switch (gs_->options().autosave_frequency) {
    case AutosaveFrequency::Off:
      break;
    case AutosaveFrequency::Daily:
      autosave_due = true;
      break;
    case AutosaveFrequency::Weekly:
      autosave_due = is_end_of_week;
      break;
    case AutosaveFrequency::Monthly:
      autosave_due = is_end_of_month;
      break;
    }
    if (autosave_due) {
      turn_pipeline_.push_back(turn::Autosave{});
    }
  }
  else if (new_player_is_human) {
    turn_pipeline_.push_back(turn::SetTimeOfDay{start_time});
  }

  turn_pipeline_.push_back(turn::PreparePlayerTurn{});
  turn_pipeline_.push_back(turn::ResumeTime{});
  turn_pipeline_.push_back(turn::CityStartOfDay{});
  turn_pipeline_.push_back(turn::PlayerTurnCard{});

  if (prev_is_human) {
    if (is_end_of_week) {
      turn_pipeline_.push_back(turn::WeeklyReports{
          prev_player_id, std::move(prev_restaurant_ids), is_end_of_month});
    }
    if (is_end_of_month) {
      turn_pipeline_.push_back(turn::MonthlyInvoice{prev_player_id});
    }
  }

  Logger::debug(
      "TurnPipeline: built {} steps (wrapped={} week={} month={} year={})",
      turn_pipeline_.size(), wrapped, is_end_of_week, is_end_of_month,
      is_end_of_year);
}

turn::StepResult
TurnOrchestrator::execute_pipeline_step(const turn::Step& step) {
  using turn::StepResult;

  return std::visit(
      overloaded{
          [&](const turn::ProcessRestaurant& s) {
            return process_restaurant_step(s);
          },

          [&](const turn::Delay& s) {
            pipeline_delay_frames_ = s.frames;
            return StepResult::Waiting;
          },

          [&](const turn::FinishTurn& s) { return finish_end_of_turn(s); },

          [&](const turn::TickCities&) {
            end_of_day();
            return StepResult::Done;
          },

          [&](const turn::EndOfWeekProcessing&) {
            end_of_week_processing();
            return StepResult::Done;
          },

          [&](const turn::EndOfYearProcessing&) {
            end_of_year_processing();
            return StepResult::Done;
          },

          [&](const turn::EndOfMonthProcessing&) {
            end_of_month_processing();
            return StepResult::Done;
          },

          [&](const turn::AnnualCompetition&) {
            // Resumes from advance_annual_competition() once the winner
            // announcement closes.
            return gs_->competitions().run_annual_pizza_competition()
                       ? StepResult::Waiting
                       : StepResult::Done;
          },

          [&](const turn::MonthlyCompetition&) {
            // Resumes via advance_pipeline() from the popup chain. When no
            // human can participate the chain resumes synchronously inside
            // this call, having already drained the pipeline; returning
            // Waiting is then a no-op.
            gs_->competitions().run_monthly_competition();
            return StepResult::Waiting;
          },

          [&](const turn::FadeTransition& s) {
            // Resumes from GameState::on_fade_midpoint().
            gs_->pending_fade_midpoint_ = true;
            gs_->request_fade_transition(s.frames);
            return StepResult::Waiting;
          },

          [&](const turn::AdvanceDate& s) {
            gs_->time_.time_on_next_day(s.start_time.hour, s.start_time.minute);
            // Freeze again so the snapshot shows the new date rather than the
            // one captured in finish_end_of_turn().
            auto* ap = gs_->active_player();
            gs_->status().freeze_header(ap ? std::optional{ap->id()}
                                           : std::nullopt);
            return StepResult::Done;
          },

          [&](const turn::SetTimeOfDay& s) {
            gs_->time_.set_time_of_day(s.start_time.hour, s.start_time.minute);
            return StepResult::Done;
          },

          [&](const turn::PreparePlayerTurn&) {
            for (auto* restaurant : gs_->restaurants_for(gs_->active_player_)) {
              if (restaurant) {
                restaurant->start_of_day(*gs_);
              }
            }
            deliver_depot_stock(gs_->active_player_);
            pending_ai_auto_end_turn_ =
                !gs_->players_[gs_->active_player_.value]
                     ->is_human_controlled();
            if (!pending_ai_auto_end_turn_) {
              gs_->status().suppress(false);
              for (auto& msg :
                   gs_->active_player()->take_pending_notifications()) {
                gs_->status().post(std::move(msg));
              }
            }
            return StepResult::Done;
          },

          [&](const turn::ResumeTime&) {
            // Only run the clock on a human turn. AI turns are ended by the
            // turn card auto-dismissing (pending_ai_auto_end_turn_), not by
            // reaching a closing time, so advancing time during them would
            // only jog the clock forward and risk firing per-block events
            // against a non-human player.
            if (gs_->players_[gs_->active_player_.value]
                    ->is_human_controlled()) {
              gs_->resume_time_advancement();
            }
            return StepResult::Done;
          },

          [&](const turn::CityStartOfDay&) {
            if (auto* city = gs_->active_city()) {
              run_city_start_of_day(*city);
            }
            return StepResult::Done;
          },

          [&](const turn::PlayerTurnCard&) {
            gs_->ui().push<PlayerTurnCardState>(gs_->active_player_,
                                                gs_->time().current_date());
            return StepResult::Done;
          },

          [&](const turn::WeeklyReports& s) {
            std::vector<Restaurant*> restaurants;
            for (auto rid : s.restaurant_ids) {
              if (auto* r = gs_->restaurant(rid)) {
                restaurants.push_back(r);
              }
            }
            if (s.interleave_electricity) {
              // Defer resumption via on_done so MonthlyInvoice only runs after
              // all restaurant invoices and the city invoice are dismissed.
              gs_->turn_presenter().show_weekly_reports(
                  restaurants, s.previous_player,
                  [this]() { advance_pipeline(); }, true);
              return StepResult::Waiting;
            }
            gs_->turn_presenter().show_weekly_reports(
                restaurants, s.previous_player, {}, false);
            return StepResult::Done;
          },

          [&](const turn::MonthlyRestaurantInvoice& s) {
            if (!gs_->restaurant(s.id)) {
              return StepResult::Done;
            }
            gs_->turn_presenter().show_electricity_slot_invoice(
                s.id, [this]() { advance_pipeline(); });
            return StepResult::Waiting;
          },

          [&](const turn::MonthlyInvoice& s) {
            gs_->turn_presenter().show_monthly_invoice(s.previous_player);
            return StepResult::Done;
          },

          [&](const turn::Autosave&) {
            if (gs_->context_) {
              save::SaveGameManager::autosave(*gs_, *gs_->context_);
            }
            return StepResult::Done;
          },
      },
      step);
}

void TurnOrchestrator::deliver_depot_stock(core::PlayerId player_id) {
  auto* city_ptr = gs_->active_city();
  if (!city_ptr) {
    return;
  }

  City& city = *city_ptr;

  auto* player = gs_->players_[player_id.value].get();
  if (!player) {
    return;
  }

  std::vector<core::RestaurantId> city_rest_ids;
  for (auto* r : gs_->restaurants_for(player_id)) {
    if (r && r->property().city().id() == city.id()) {
      city_rest_ids.push_back(r->id());
    }
  }

  if (city_rest_ids.empty()) {
    return;
  }

  auto& depot = city.central_depot(player_id);
  depot.sync_delivery_priorities(city_rest_ids);

  for (auto rest_id : depot.delivery_priorities()) {
    auto* rest = gs_->restaurant(rest_id);
    if (!rest || !rest->wants_open() || rest->is_force_closed()) {
      continue;
    }

    auto demand =
        rest->calculate_daily_ingredient_demand_grams(player->recipes());

    for (int i = 0; i < max_ingredient_count; ++i) {
      int needed = demand[i] * 2;
      if (needed <= 0) {
        continue;
      }

      auto ing_type = static_cast<core::food::IngredientType>(i);
      auto& depot_stock = depot.get_stock(ing_type);

      constexpr int grams_per_kg = 1000;
      int current_raw = rest->inventory().get_stock(ing_type).quantity;
      int current = (current_raw / grams_per_kg) * grams_per_kg;
      int shortfall = needed - current;

      if (depot_stock.quantity <= 0) {
        continue;
      }

      // We just top up the stock until it's at 2x yesterday's demand
      if (shortfall <= 0) {
        continue;
      }

      int shortfall_rounded =
          ((shortfall + grams_per_kg - 1) / grams_per_kg) * grams_per_kg;
      int transfer = std::min(shortfall_rounded, depot_stock.quantity);
      int quality = depot_stock.weighted_avg_quality;
      Money cost = depot_stock.weighted_avg_cost;

      depot_stock.consume(transfer);
      rest->inventory().add_ingredient(ing_type, transfer, quality, cost);
    }
  }

  auto& central_depot = city.central_depot(player_id);
  if (central_depot.capacity() > 0 &&
      central_depot.used() >= central_depot.capacity()) {
    gs_->message_manager_.schedule_message(
        core::events::LeventId::CentralDepotFull, player_id,
        gs_->time_.current_date());
  }
}

void TurnOrchestrator::tick_active_cities() {
  // Tick each city that has at least one human player; cities without a
  // human stay dormant (matching the original). AI players have no location
  // of their own, their city_view_view_state_ entry is just the game's
  // starting city. Each city ticks exactly once regardless of how many
  // players share it. We use city_view_view_state_ rather than active_city_
  // because it has already been switched to the next player's starting city.
  std::array<bool, NUMBER_OF_CITIES> cities_to_tick{};
  for (int i = 0; i < NUMBER_OF_PLAYERS; ++i) {
    if (gs_->players_[i] && gs_->players_[i]->is_human_controlled() &&
        gs_->city_view_view_state_[i].city_id) {
      cities_to_tick[gs_->city_view_view_state_[i].city_id->value] = true;
    }
  }
  for (int i = 0; i < NUMBER_OF_CITIES; ++i) {
    if (cities_to_tick[i] && gs_->cities_[i]) {
      gs_->cities_[i]->end_of_day(gs_->difficulty_level_);
    }
  }
}

void TurnOrchestrator::end_of_week_processing() {
  for (auto& city : gs_->cities_) {
    if (city) {
      city->reset_weekly_vendor_spending();
    }
  }

  // Pay weekly staff salaries
  for (auto& player : gs_->players_) {
    if (player) {
      player->end_of_week(*gs_);
    }
  }

  // 30% chance to re-randomize one demographic's taste profile
  if (engine::random::rand_percentage_chance(30)) {
    auto demo = static_cast<ConsumerType>(engine::random::rand_max_n(6));
    auto& tp = gs_->customer_taste_profiles_[demo];
    tp.fat = engine::random::rand_max_n(10);
    tp.sweet = engine::random::rand_max_n(10);
    tp.salty = engine::random::rand_max_n(10);
    tp.hot = engine::random::rand_max_n(10);
    tp.spicy = engine::random::rand_max_n(10);
    gs_->recalculate_all_pizza_demographics();
  }
}

void TurnOrchestrator::end_of_year_processing() {
  for (auto& player : gs_->players_) {
    if (player) {
      auto& ch = player->character();
      ch.age(static_cast<uint8_t>(std::min(255, (int)ch.age() + 1)));
    }
  }
}

void TurnOrchestrator::end_of_month_processing() {
  for (auto& city : gs_->cities_) {
    if (city) {
      city->end_of_month();
    }
  }
}

void TurnOrchestrator::process_depot_theft() {
  // 1% chance (10% x 10%) per player per day for the active city's depot.
  auto* city = gs_->active_city();
  if (!city) {
    return;
  }
  using engine::random::rand_max_n;
  using engine::random::rand_percentage_chance;
  for (auto& player : gs_->players_) {
    if (!player) {
      continue;
    }
    auto& depot = city->central_depot(player->id());
    if (depot.capacity() <= 0) {
      continue;
    }
    // Two independent 10% rolls = 1% combined chance
    bool first_roll = rand_percentage_chance(10);
    bool second_roll = rand_percentage_chance(10);
    if (!first_roll || !second_roll) {
      continue;
    }
    auto category = static_cast<IngredientCategory>(rand_max_n(4));
    depot.steal_stock(category);
    Logger::debug("Stock of category {} stolen.", to_string(category));
    player->make_insurance_claim(InsuranceType::WAREHOUSE);
  }
}

// Called only after all players have finished their turns.
void TurnOrchestrator::end_of_day() {

  city_start_of_day_ran_.fill(false);

  tick_active_cities();

  // Update AI menu slot history for pizza charts
  for (auto& player : gs_->players_) {
    if (player && !player->is_human_controlled()) {
      int rest_count =
          static_cast<int>(gs_->restaurants_for(player->id()).size());
      ai::update_ai_menu(*player, rest_count);
    }
  }

  // Rebuild pizza charts and control map for active city
  if (auto* city = gs_->active_city()) {
    city->pizza_charts().build(*gs_);
    const auto& chart_entries = city->pizza_charts().entries();
    if (!chart_entries.empty()) {
      const auto& top = chart_entries[0];
      gs_->message_manager_.schedule_message(
          core::events::LeventId::PizzaReachedNo1InCharts, top.player_id,
          gs_->time_.current_date());
    }
    city->control_map().build(*gs_, *city);
    gs_->milestones().check_market_share_milestones(*city);
  }

  gs_->milestones().check_world_domination();

  process_depot_theft();

  // Clear actor refused calls (realtors cool down overnight)
  gs_->clear_all_refused_calls();

  for (auto& player : gs_->players_) {
    if (player && !player->is_human_controlled()) {
      player->decay_competition_willingness();
    }
  }
}

void TurnOrchestrator::run_city_start_of_day(City& city) {
  int idx = city.id().value;
  if (city_start_of_day_ran_[idx]) {
    return;
  }
  city_start_of_day_ran_[idx] = true;

  city.newspaper_event_system().daily_update(*gs_, city);
  city.mayor_office().start_of_day(gs_->time_.current_date(),
                                   gs_->bank_accounts_);
  auto awards = city.mayor_office().process_daily_scheduling(
      gs_->time_.current_date(), gs_->human_player_count(),
      gs_->bank_accounts_);
  for (auto& ev : awards) {
    queue_mayor_award(ev);
  }
}
