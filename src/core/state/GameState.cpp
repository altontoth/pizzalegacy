/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/state/GameState.h"

#include "core/state/FadePresenter.h"
#include "core/state/TurnPresenter.h"

#include <algorithm>
#include <cassert>
#include <ranges>
#include <utility>
#include "conversations/MayorConversation.h"
#include "core/character/Actor.h"
#include "core/character/Awards.h"
#include "core/character/Player.h"
#include "core/character/Staff.h"
#include "core/city/City.h"
#include "core/city/CityTypes.h"
#include "core/city/District.h"
#include "core/city/MayorOffice.h"
#include "core/city/Property.h"
#include "core/events/MilestoneId.h"
#include "core/finance/TurnInvoices.h"
#include "core/init/NewGameConfig.h"
#include "core/restaurant/AIEconomy.h"
#include "core/restaurant/Restaurant.h"
#include "core/restaurant/StaffEventDialogs.h"
#include "core/restaurant/StaffEvents.h"
#include "core/util/ConfigManager.h"
#include "core/util/GameContext.h"
#include "engine/Fatal.h"
#include "engine/FormatCompat.h"
#include "engine/Logger.h"
#include "engine/Random.h"
#include "engine/geom/Spaces.h"
#include "factories/CharacterFactory.h"
#include "factories/CityFactory.h"
#include "factories/PizzaFactory.h"
#include "factories/RestaurantLayoutFactory.h"
#include "resources/BobSizeFn.h"

GameState::GameState() = default;
GameState::~GameState() = default;
GameState& GameState::operator=(GameState&&) = default;

void GameState::turn_presenter(std::unique_ptr<TurnPresenter> presenter) {
  turn_presenter_ = std::move(presenter);
}

TurnPresenter& GameState::turn_presenter() {
  static NoTurnPresenter nobody;
  return turn_presenter_ ? *turn_presenter_ : nobody;
}

void GameState::fade_presenter(std::unique_ptr<FadePresenter> presenter) {
  fade_presenter_ = std::move(presenter);
}

FadePresenter& GameState::fade_presenter() {
  static NoFadePresenter nobody;
  return fade_presenter_ ? *fade_presenter_ : nobody;
}

std::ostream& operator<<(std::ostream& os, const GameState& state) {
  os << "GameState:\n";
  os << "  active_player=" << state.active_player_ << "\n";
  os << "  active_city=" << state.active_city_ << "\n";
  for (std::size_t i = 0; i < NUMBER_OF_PLAYERS; i++) {
    if (state.players_[i]) {
      os << "  players[" << i << "]: " << *state.players_[i];
    }
    else {
      os << "  players[" << i << "] : false?! " << "\n";
    }
  }
  for (std::size_t i = 0; i < NUMBER_OF_CITIES; ++i) {
    if (state.cities_[i]) {
      os << "  city[" << i << "]: " << *state.cities_[i];
    }
  }

  return os;
}

Player* GameState::player(core::PlayerId player_id) const {
  assert(player_id.value >= 0 && player_id.value < NUMBER_OF_PLAYERS);

  return players_[player_id.value].get();
}

Player* GameState::active_player() const {
  return players_[active_player_.value].get();
}

Player* GameState::restaurant_owner(const Restaurant& restaurant) const {
  auto owner_id = restaurant.property().owner_or_tenant();
  return owner_id ? player(*owner_id) : nullptr;
}

void GameState::post_insufficient_funds() {
  status_hub_.post_direct({
      .payload = "YOU DON'T HAVE ENOUGH MONEY!!!",
      .scope = StatusScope::Global,
      .ttl = std::chrono::milliseconds{3000},
  });
}

bool GameState::spend_cash(Player& player, Account to, Money amount) {
  if (player.spend_cash(time().current_date(), to, amount)) {
    return true;
  }
  post_insufficient_funds();
  return false;
}

bool GameState::spend_cash(Account to, Money amount) {
  auto* player = active_player();
  if (!player) {
    return false;
  }
  return spend_cash(*player, to, amount);
}

bool GameState::spend_cash(Account to, Money amount,
                           core::RestaurantId restaurant_id) {
  auto* player = active_player();
  if (!player) {
    return false;
  }

  if (player->spend_cash(time().current_date(), to, amount, restaurant_id)) {
    return true;
  }
  post_insufficient_funds();
  return false;
}

void GameState::deduct_staff_wages_for_restaurant(
    core::RestaurantId restaurant_id) {
  auto* restaurant = this->restaurant(restaurant_id);
  if (!restaurant) {
    return;
  }

  auto owner_id_opt = restaurant->property().owner_or_tenant();
  if (!owner_id_opt) {
    return;
  }
  auto* p = player(*owner_id_opt);
  if (!p) {
    return;
  }

  Money total_wages = core::finance::collect_staff_wages(*restaurant).total;
  if (total_wages.cents == 0) {
    return;
  }

  // wages (can go negative)
  p->financials().transfer(time_.current_date(), Account::Cash, Account::Staff,
                           total_wages, restaurant_id,
                           AccountingScope::Operational);

  Logger::info(
      "[Finance] {} - Staff wages deducted for {}: {} (balance now {})",
      p->name(), restaurant->name(), total_wages, p->cash_balance());

  if (p->cash_balance().cents < 0) {
    Logger::warn("[Finance] {} - BANKRUPT after staff payment! Balance: {}",
                 p->name(), p->cash_balance());
    deliver_milestone(core::events::MilestoneId::NotAbleToPayDebts,
                      [this, pid = *owner_id_opt]() {
                        eliminate_player(pid, GameOverCause::BankruptDebts);
                      });
  }
}

City* GameState::city(core::CityId city_id) const {
  assert(is_valid_city_id(city_id));
  return cities_[city_id.value].get();
}

std::vector<City*> GameState::initialized_cities() const {
  std::vector<City*> result;
  for (const auto& city_ptr : cities_) {
    if (city_ptr) {
      result.push_back(city_ptr.get());
    }
  }
  return result;
}

std::vector<City*> GameState::visited_cities(const Player& player) const {
  std::vector<City*> result;
  for (const auto& city_id : player.visited_city_ids()) {
    if (auto* c = city(city_id)) {
      result.push_back(c);
    }
  }

  // Sorted so callers get the same order every run; the weekly profits are
  // posted to the ledger in whatever order this returns.
  std::ranges::sort(
      result, [](const City* a, const City* b) { return a->id() < b->id(); });
  return result;
}

bool GameState::is_city_loaded(core::CityId city_id) const {
  assert(is_valid_city_id(city_id));
  return cities_[city_id.value] != nullptr;
}

core::CityId GameState::active_city_id() const { return active_city_; }

City* GameState::active_city() const {
  assert(is_valid_city_id(active_city_));
  return cities_[active_city_.value].get();
}

std::vector<std::pair<uint8_t, int>> GameState::build_player_chars() const {
  std::vector<std::pair<uint8_t, int>> result;
  for (int i = 0; i < NUMBER_OF_PLAYERS; ++i) {
    if (players_[i]) {
      result.push_back({players_[i]->character().id(), i});
    }
  }
  return result;
}

void GameState::initialise_city_for_ai(City& city) {
  for (auto& player : players_) {
    if (!player || player->is_human_controlled()) {
      continue;
    }
    player->ai_expansion_cooldown(ai_initial_expansion_cooldown);
    real_estate_.create_ai_starting_restaurant(*player, city);
    ai::init_ai_menu(*player);
  }
}

City& GameState::city_or_create(core::CityId city_id) {
  assert(is_valid_city_id(city_id));
  auto& city = cities_[city_id.value];
  if (city == nullptr) {
    city = CityFactory::create_city(city_id, build_player_chars());
  }
  return *city;
}

core::CityId GameState::default_city_id() const {
  if (is_valid_city_id(active_city_)) {
    return active_city_;
  }
  for (int i = 0; i < NUMBER_OF_CITIES; ++i) {
    if (cities_[i]) {
      return core::CityId{i};
    }
  }
  return core::CityId{0};
}

void GameState::active_city(core::CityId city_id) {
  assert(is_valid_city_id(city_id));
  active_city_ = city_id;
  City& city = city_or_create(city_id);
  if (!city_initialised_[city_id.value]) {
    city_initialised_[city_id.value] = true;
    initialise_city_for_ai(city);
  }
  player_city_[active_player_.value] = city_id;
  turn_.run_city_start_of_day(city);
}

void GameState::ensure_city_loaded(core::CityId city_id) {
  // Create city if it doesn't exist, but don't change active_city_
  city_or_create(city_id);
}

void GameState::city_position(ViewPoint p) { city_position_ = p; }

ViewPoint GameState::city_position() const { return city_position_; }

void GameState::window_size(int width, int height) {
  window_width_ = width;
  window_height_ = height;
}

int GameState::window_height() const { return window_height_; }

int GameState::window_width() const { return window_width_; }

std::array<Player*, NUMBER_OF_PLAYERS> GameState::players() const {
  auto raw_view =
      players_ | std::views::transform(
                     [](const std::unique_ptr<Player>& p) { return p.get(); });
  std::array<Player*, NUMBER_OF_PLAYERS> raw_players;
  std::ranges::copy(raw_view, raw_players.begin());
  return raw_players;
}

void GameState::players(
    std::array<std::unique_ptr<Player>, NUMBER_OF_PLAYERS> players) {
  players_ = std::move(players);
}

void GameState::last_clicked_property_index(int index) {
  last_clicked_property_index_ = index;
}

std::optional<int> GameState::last_clicked_property_index() const {
  return last_clicked_property_index_;
}

void GameState::advance_time_block() {
  time_.advance_10_minutes();

  const auto now = time_.time_of_day();

  if (now == Restaurant::DEFAULT_OPENING_TIME) {
    status_hub_.post_direct(StatusMsg{
        .payload = "It's 9 am. The restaurants are opening.",
        .scope = StatusScope::Global,
    });
  }
  else if (now == Restaurant::DEFAULT_CLOSING_TIME) {
    status_hub_.post_direct(StatusMsg{
        .payload = "It's 9 pm. The restaurants are closing.",
        .scope = StatusScope::Global,
    });
  }
}

void GameState::advance_game_time() {
  advance_time_block();
  if (turn_.interruptions_allowed()) {
    check_mafia_deadlines();
  }
}

void GameState::advance_second() {
  // A stopped clock doesn't count seconds, so the block resumes where it left
  // off.
  if (clock_rate_ == ClockRate::Stopped) {
    return;
  }

  ++second_in_block_;
  if (second_in_block_ >= second_counter_wrap) {
    second_in_block_ = 0;
  }

  if (clock_rate_ != ClockRate::Running || clock_suspended_) {
    return;
  }
  if (second_in_block_ < seconds_per_block) {
    return;
  }

  second_in_block_ = 0;
  // The clock stops at the player's end of day; the dispatcher ends the turn.
  if (!is_past_player_end_of_day()) {
    advance_game_time();
  }
}

bool GameState::run_interruption_pass() {
  interruption_phase_ = next_phase(interruption_phase_);

  switch (interruption_phase_) {
  case InterruptionPhase::QueuedEvents:
    if (turn_.fire_queued_events()) {
      return true;
    }
    return check_police_events();
  case InterruptionPhase::StaffEvents:
    return turn_.fire_staff_events();
  case InterruptionPhase::ProtectionRacket:
    if (auto offer = roll_protection_racket()) {
      push_protection_racket_offer(*offer);
      return true;
    }
    return false;
  case InterruptionPhase::Unused:
    return false;
  }
  return false;
}

bool GameState::dispatch_interruption() {
  if (turn_.is_processing_end_of_turn() || !active_player() ||
      !active_player()->is_human_controlled()) {
    return false;
  }

  if (process_pending_milestones()) {
    return true;
  }

  // The player's end of day is checked ahead of the window, so it lands
  // whatever the second counter is doing.
  if (is_past_player_end_of_day()) {
    turn_.request_end_of_turn();
    return false;
  }

  if (second_in_block_ % seconds_between_interruption_windows != 0) {
    return false;
  }
  return run_interruption_pass();
}

void GameState::on_player_input() {
  if (turn_.interruption_rate() == InterruptionRate::OnAction) {
    dispatch_interruption();
  }
}

bool GameState::offer_interruption_tick() {
  switch (turn_.interruption_rate()) {
  case InterruptionRate::Full:
    break;
  // Every other tick.
  case InterruptionRate::Half:
    if (pit_tick_in_second_ % 2 != 0) {
      return false;
    }
    break;
  case InterruptionRate::OnAction:
  case InterruptionRate::None:
    return false;
  }
  return dispatch_interruption();
}

bool GameState::record_milestone(core::events::MilestoneId msg_id,
                                 core::PlayerId player_id) {
  if (!career_log_.schedule_message(msg_id, player_id, time_.current_date())) {
    return false;
  }

  const int score = career_log_.get_data(msg_id).score;
  auto* p = player(player_id);
  if (p && score > 0) {
    p->add_score(score);
  }
  return true;
}

void GameState::deliver_milestone(core::events::MilestoneId msg_id,
                                  std::function<void()> on_dismiss) {
  auto* player = active_player();
  if (!player) {
    return;
  }

  record_milestone(msg_id, player->id());

  if (!career_log_.claim_for_delivery(msg_id, player->id(),
                                      time().current_date())) {
    if (on_dismiss) {
      on_dismiss();
    }
    return;
  }

  // Mission mode: achieving the mission's last target milestone ends the
  // game.
  const bool mission_complete = is_mission_active() &&
                                player->is_human_controlled() &&
                                msg_id == mission_target_->final_event();
  if (mission_complete) {
    on_dismiss = [this, pid = player->id(),
                  original_on_dismiss = std::move(on_dismiss)]() {
      if (original_on_dismiss) {
        original_on_dismiss();
      }
      eliminate_player(pid, GameOverCause::MissionComplete);
    };
  }

  if (career_log_.has_fullscreen(msg_id)) {
    FullscreenStatusState fss{.message_id = msg_id,
                              .on_dismiss = std::move(on_dismiss)};
    ui_.push<FullscreenStatusState>(std::move(fss));
  }
  else if (mission_complete) {
    // CV-only milestones show no fullscreen, end the mission right away.
    on_dismiss();
  }
}

void GameState::eliminate_player(core::PlayerId id, GameOverCause cause) {
  auto* p = player(id);
  if (!p || p->status() == PlayerStatus::Eliminated) {
    return;
  }
  p->eliminate();
  ui_.push<GameOverState>(id, cause);
}

bool GameState::process_pending_milestones() {
  auto* player = active_player();
  if (!player || !player->is_human_controlled()) {
    return false;
  }

  auto pending = career_log_.get_pending_for_player(player->id());
  for (auto msg_id : pending) {
    deliver_milestone(msg_id);
  }
  return !pending.empty();
}

void GameState::pause_time_advancement() { clock_suspended_ = true; }

void GameState::resume_time_advancement() {
  clock_suspended_ = false;

  if (turn_.may_auto_end_turn()) {
    turn_.request_end_of_turn();
  }
}

void GameState::enter_realtime_play() {
  clock_rate_ = ClockRate::Running;
  resume_time_advancement();
  turn_.interruption_rate(InterruptionRate::Full);
}

void GameState::leave_realtime_play() {
  turn_.interruption_rate(InterruptionRate::None);
  clock_rate_ = ClockRate::Stopped;
}

bool GameState::is_past_player_end_of_day() const {
  auto end_time = active_player()->end_of_day_time();
  return time_.is_past_time(end_time.hour, end_time.minute);
}

void GameState::advance_time_blocks(int n) {
  second_in_block_ = 0;
  for (int i = 0; i < n; ++i) {
    if (is_past_player_end_of_day()) {
      return;
    }
    advance_time_block();
  }
}

const GameTime& GameState::time() const { return time_; }

bool GameState::floor_editing_unlocked() const {
  return time_.current_date() >= floor_editing_bonus_date;
}

void GameState::update(uint32_t elapsed_ms) {
  // Ahead of the world step.
  turn_presenter().tick();

  if (active_player_.value < 0) {
    return;
  }

  status_hub_.update(elapsed_ms);

  // Handle end-of-turn processing delay between restaurants,
  // so that we have visible changes to the money per restaurant processed.
  // Yes it would be so much easier to just do them all in a quick loop but
  // I wanted to match the original behavior of visibly doing them one at a
  // time.
  if (turn_.tick_pipeline_delay()) {
    return; // Don't advance game time while processing end of turn
  }

  // Time is only relevant to human players.
  if (active_player()->is_human_controlled()) {
    pit_accumulator_us_ += elapsed_ms * 1000;

    // At most one interruption per update; once a dialog is up the rest of
    // the ticks pass without one.
    bool interrupted = false;

    while (pit_accumulator_us_ >= pit_tick_us) {
      pit_accumulator_us_ -= pit_tick_us;

      ++pit_tick_in_second_;
      if (pit_tick_in_second_ >= pit_ticks_per_second) {
        pit_tick_in_second_ = 0;
        advance_second();
      }

      if (!interrupted) {
        interrupted = offer_interruption_tick();
      }
    }
  }

  competitions_.tick();

  turn_.run_pending_end_of_turn();
}

void GameState::process_mayor_award_event(const MayorAwardEvent& award_event) {
  auto* city = active_city();
  if (!city) {
    return;
  }
  auto mayor_actors = city->actors(ActorType::MAYOR);
  if (mayor_actors.empty()) {
    return;
  }
  int mayor_id = mayor_actors[0]->id();

  // For ceremonies, resolve the winner. Announcements always pass nullopt.
  // MayorOffice resolves awards scored from its own accumulated tallies;
  // restaurant-based awards (Most Beautiful, Golden Bow Tie) are scored from
  // live restaurant state, so GameState (which owns the restaurants) does them.
  std::optional<int> award_winner = award_event.winner_player_id;

  if (award_event.is_ceremony) {
    auto all_players = players();
    int pc = human_player_count();
    const auto& mayor_office = city->mayor_office();

    if (award_event.type == MayorAwardType::MostBeautifulRestaurant) {
      int best_score = -1;
      award_winner = std::nullopt;
      for (int i = 0; i < pc; ++i) {
        if (!all_players[i] || !mayor_office.fee_paid(i)) {
          continue;
        }
        int score = 0;
        for (auto* rest :
             restaurants_in_city(all_players[i]->id(), city->id())) {
          for (int v : rest->economy().furniture_appeal()) {
            score += v;
          }
        }
        if (score > best_score) {
          best_score = score;
          award_winner = i;
        }
      }
    }
    else if (award_event.type == MayorAwardType::GoldenBowTie) {
      int best_time = -1;
      award_winner = std::nullopt;
      for (int i = 0; i < pc; ++i) {
        if (!all_players[i] || !mayor_office.fee_paid(i)) {
          continue;
        }
        for (auto* rest :
             restaurants_in_city(all_players[i]->id(), city->id())) {
          int t = rest->calculate_service_time_minutes();
          if (t > 0 && (best_time < 0 || t < best_time)) {
            best_time = t;
            award_winner = i;
          }
        }
      }
    }

    // Grant a trophy level to the winner.
    if (award_winner.has_value()) {
      int winner_id = *award_winner;
      if (winner_id >= 0 && winner_id < NUMBER_OF_PLAYERS &&
          all_players[winner_id]) {
        auto& player = *all_players[winner_id];
        AwardCategory cat = AwardCategory::Heart;
        switch (award_event.type) {
        case MayorAwardType::MostBeautifulRestaurant:
          cat = AwardCategory::Heart;
          break;
        case MayorAwardType::PizzaOfTheYear:
          cat = AwardCategory::Pizza;
          break;
        case MayorAwardType::BestInvestorInTown:
          cat = AwardCategory::Star;
          break;
        case MayorAwardType::BestImitator:
          cat = AwardCategory::Spoon;
          break;
        case MayorAwardType::GoldenBowTie:
          cat = AwardCategory::BowTie;
          break;
        case MayorAwardType::TransAtlanticTrade:
          cat = AwardCategory::Cross;
          break;
        default:
          break;
        }
        auto current = player.awards()[static_cast<int>(cat)];
        if (current < AwardLevel::Gold) {
          player.award(cat,
                       static_cast<AwardLevel>(static_cast<int>(current) + 1));
        }
      }
    }

    // Schedule MilestoneId milestones for the winner if they are the active
    // player.
    if (award_winner.has_value()) {
      auto* p = active_player();
      if (p && p->id().value == *award_winner) {
        using core::events::MilestoneId;
        switch (award_event.type) {
        case MayorAwardType::MostBeautifulRestaurant:
          record_milestone(MilestoneId::AwardInteriorOfTheYear, p->id());
          record_milestone(MilestoneId::AwardMostBeautifulRestaurant, p->id());
          break;
        case MayorAwardType::PizzaOfTheYear:
          record_milestone(MilestoneId::AwardPizzaOfTheYearMayor, p->id());
          break;
        case MayorAwardType::BestInvestorInTown:
          record_milestone(MilestoneId::AwardBestInvestor, p->id());
          break;
        case MayorAwardType::BestImitator:
          record_milestone(MilestoneId::AwardBestImitator, p->id());
          break;
        case MayorAwardType::GoldenBowTie:
          record_milestone(MilestoneId::AwardGoldenBowTie, p->id());
          break;
        case MayorAwardType::TransAtlanticTrade:
          record_milestone(MilestoneId::AwardTransAtlanticTradePlaque, p->id());
          break;
        default:
          break;
        }
      }
    }

    // Awards without a "nobody won" dialog silently skip if no one qualified.
    bool has_nobody_dialog =
        (award_event.type == MayorAwardType::BestImitator ||
         award_event.type == MayorAwardType::TransAtlanticTrade);
    if (!award_winner.has_value() && !has_nobody_dialog) {
      return;
    }
  }

  // Map MayorAwardType to MayorConversationType
  MayorConversationType conv_type = MayorConversationType::PhoneCall;
  switch (award_event.type) {
  case MayorAwardType::MostBeautifulRestaurant:
    conv_type = MayorConversationType::MostBeautifulRestaurantAward;
    break;
  case MayorAwardType::PizzaOfTheYear:
    conv_type = award_winner.has_value()
                    ? MayorConversationType::PizzaOfTheYearWinner
                    : MayorConversationType::PizzaOfTheYearCompetition;
    break;
  case MayorAwardType::BestInvestorInTown:
    conv_type = MayorConversationType::BestInvestorInTownAward;
    break;
  case MayorAwardType::BestImitator:
    conv_type = MayorConversationType::BestImitatorAward;
    break;
  case MayorAwardType::GoldenBowTie:
    conv_type = MayorConversationType::GoldenBowTieAward;
    break;
  case MayorAwardType::TransAtlanticTrade:
    conv_type = MayorConversationType::TransAtlanticTradeAward;
    break;
  case MayorAwardType::None:
    return;
  }

  ui_.push<MayorState>(MayorState{.id = mayor_id,
                                  .type = conv_type,
                                  .restaurant_id = std::nullopt,
                                  .is_first_inspection = std::nullopt,
                                  .award_winner_player_id = award_winner});
}

void GameState::show_staff_event_dialog(
    const core::restaurant::StaffEventData& event, Restaurant* restaurant,
    std::function<void()> on_done) {
  using core::restaurant::StaffEventDialogs;
  using core::restaurant::StaffEvents;

  // Apply the world effect when showing the dialog. PERS effects are
  // outcome-dependent and applied later via StaffEvents::apply_outcome.
  Staff* staff = restaurant->staff_at(event.job_role, event.slot_index);
  if (StaffEventDialogs::is_persf(event.type) && staff) {
    StaffEvents::apply_event_effect(event.type, *staff, *restaurant, *this);
  }

  if (!staff_event_presenter_) {
    on_done();
    return;
  }
  staff_event_presenter_(event, restaurant, std::move(on_done));
}

bool GameState::trigger_win_sequence(core::PlayerId pid) {
  if (!record_milestone(core::events::MilestoneId::ControlsWorld, pid)) {
    return false;
  }
  ui_.push<WinSequenceState>(
      WinSequenceState{.line_index = 0, .player_id = pid});
  return true;
}

void GameState::on_city_entered(core::CityId city_id, const Player& player) {
  if (auto* p = this->player(player.id())) {
    p->mark_city_visited(city_id);
  }

  // Travelling leaves the calls and staff dialogs the restaurants back home
  // queued for you behind.
  if (status_queue_city_ && *status_queue_city_ != city_id) {
    status_hub_.clear_queue();
    turn_.drop_city_local_events_outside(city_id);
  }
  status_queue_city_ = city_id;

  // Restaurants outside the city the player woke up in start their day on
  // arrival, as in the original. Re-entry later the same day is a no-op.
  if (turn_.begin_restaurant_day(city_id, player.id())) {
    for (auto* restaurant : restaurants_in_city(player.id(), city_id)) {
      restaurant->start_of_day(*this);
    }
  }

  using core::events::MilestoneId;
  auto arrival = static_cast<MilestoneId>(
      static_cast<int>(MilestoneId::ArrivalInParis) + city_id.value);
  record_milestone(arrival, player.id());
  process_pending_milestones();
}

void GameState::check_mafia_deadlines() {
  // Only reached on a human player's turn.
  auto* player = active_player();
  auto* city = active_city();

  auto mafia_actors = city->actors(ActorType::MAFIA);
  if (mafia_actors.empty()) {
    return;
  }
  const auto& date = time_.current_date();

  // A mafia call must come from the clan the player dealt with.
  auto clan_actor_id = [&](int clan) {
    int idx = std::clamp(clan, 0, static_cast<int>(mafia_actors.size()) - 1);
    return mafia_actors[idx]->id();
  };

  // Loan repayment check
  auto& loan = player->mafia_state().loan;
  if (loan && date >= loan->due_date) {
    ui_.push<MafiaState>(MafiaState{.id = clan_actor_id(loan->clan),
                                    .mode = MafiaMode::LoanRepaymentDue,
                                    .restaurant_id = std::nullopt});
    return;
  }

  // Check laundering mission completion (all 4 accounts received their share)
  auto& mission = player->mafia_state().active_mission;
  if (mission && mission->active &&
      mission->mission_type == MafiaMissionType::Laundering &&
      !mission->completed) {
    Money per_account = mission->cash_amount / 4;
    bool all_done = true;
    for (int i = 0; i < 4; i++) {
      auto* acct = bank_accounts_.find(mission->bank_accounts[i]);
      if (acct && acct->deposited >= per_account) {
        mission->launder_done[i] = true;
      }
      if (!mission->launder_done[i]) {
        all_done = false;
      }
    }
    if (all_done) {
      mission->completed = true;
      for (int acct : mission->bank_accounts) {
        bank_accounts_.close_account(acct);
      }
    }
  }

  // Mission deadline check
  if (mission && mission->active) {
    // Package missions fail immediately once the pickup window has passed,
    // since without the item the dropoff can never succeed. But if the
    // player already picked up the package, the window passing is expected.
    if (mission->mission_type == MafiaMissionType::Package &&
        !mission->pickup_done && mission->pickup.has_passed(time_)) {
      ui_.push<MafiaState>(MafiaState{.id = clan_actor_id(mission->clan),
                                      .mode = MafiaMode::MissionOverdue,
                                      .restaurant_id = std::nullopt});
      return;
    }
    if (date > mission->deadline && !mission->completed) {
      ui_.push<MafiaState>(MafiaState{.id = clan_actor_id(mission->clan),
                                      .mode = MafiaMode::MissionOverdue,
                                      .restaurant_id = std::nullopt});
      return;
    }
    if (date >= mission->deadline && mission->completed) {
      ui_.push<MafiaState>(MafiaState{.id = clan_actor_id(mission->clan),
                                      .mode = MafiaMode::MissionDeadlineReached,
                                      .restaurant_id = std::nullopt});
      return;
    }
  }
}

std::optional<GameState::RacketOffer> GameState::roll_protection_racket() {
  auto* player = active_player();
  auto* city = active_city();
  if (!player || !city) {
    return std::nullopt;
  }
  if (city->actors(ActorType::MAFIA).empty()) {
    return std::nullopt;
  }

  if (player->social_status().underworld_score() >= 500) {
    return std::nullopt;
  }

  auto candidates = restaurants_in_city(player->id(), city->id());
  if (candidates.empty()) {
    return std::nullopt;
  }

  if (!engine::random::rand_percentage_chance(2)) {
    return std::nullopt;
  }

  // The 2% roll targets a single random restaurant; if the pick is closed or
  // its timer hasn't expired, nothing happens this roll.
  auto* restaurant = candidates[engine::random::rand_max_n(
      static_cast<int>(candidates.size()))];
  if (!restaurant->wants_open()) {
    return std::nullopt;
  }
  auto timer = restaurant->protection_timer();
  if (timer.has_value() && *timer > 1) {
    return std::nullopt;
  }

  bool first = !restaurant->protection_contacted();
  // Set timer now to prevent re-triggering while the offer is pending
  // delivery. The conversation will overwrite this with the real cooldown
  // value when it resolves.
  restaurant->protection_timer(30);
  restaurant->protection_contacted(true);
  return RacketOffer{.restaurant_id = restaurant->id(), .first_contact = first};
}

void GameState::push_protection_racket_offer(const RacketOffer& offer) {
  auto* city = active_city();
  if (!city || !restaurant(offer.restaurant_id)) {
    return;
  }
  auto mafia_actors = city->actors(ActorType::MAFIA);
  if (mafia_actors.empty()) {
    return;
  }
  ui_.push<MafiaState>(MafiaState{.id = mafia_actors[0]->id(),
                                  .mode = MafiaMode::ProtectionRacketOffer,
                                  .restaurant_id = offer.restaurant_id,
                                  .first_contact = offer.first_contact});
}

void GameState::active_player(core::PlayerId player_id) {
  active_player_ = player_id;
}

void GameState::active_district_id(core::DistrictId id) {
  active_district_id_ = id;
}

void GameState::clear_active_district() { active_district_id_.reset(); }

std::optional<core::DistrictId> GameState::active_district_id() const {
  return active_district_id_;
}

void GameState::active_restaurant(core::RestaurantId r_id) {
  active_restaurant_ = r_id;
}

Restaurant* GameState::active_restaurant() const {
  if (active_restaurant_) {
    return restaurant(*active_restaurant_);
  }
  return nullptr;
}

void GameState::begin_initialization() { initializer_ = init::Config{}; }

init::Config& GameState::initializer() {
  if (!initializer_) {
    initializer_.emplace();
  }
  return *initializer_;
}

void GameState::register_bank_accounts() {
  // Fixed player accounts, 111111 through 888888.
  for (int i = 0; i < NUMBER_OF_PLAYERS; ++i) {
    if (players_[i]) {
      bank_accounts_.register_player_account(core::PlayerId{i});
    }
  }
}

void GameState::finalize_initialization(GameContext& context) {
  context_ = &context;

  // Static newspaper article/template data
  article_database_.load_articles();

  if (!initializer_) {
    return;
  }

  init::Config cfg = *initializer_;

  // Use initializer to set permanent state
  game_mode_ = cfg.mode;
  if (cfg.mode == GameMode::MISSION_MODE) {
    mission_target_ = std::move(cfg.mission);
  }
  active_city(cfg.city_id); // Set the active city before pushing UI state
  ui().push<CityViewState>(cfg.city_id);
  difficulty_level_ = static_cast<int>(cfg.difficulty);
  weapon_market_.init();

  for (int i = 0; i < NUMBER_OF_PLAYERS; ++i) {
    auto player_item = initializer_->players.at(i);

    players_[i] = std::make_unique<Player>(
        core::PlayerId{i},
        core::character::Character(
            CharacterFactory::create_character(player_item.character_id)),
        player_item.is_human);

    players_[i]->name(player_item.name);
    city_view_view_state_[i] = CityViewMapState{core::PlayerId{i}};

    if (cfg.mode == GameMode::QUICKSTART && player_item.is_human) {
      // Human player in QuickStart gets a restaurant sized to their choice.
      const bool big = (initializer_->size == init::RestaurantSize::BIG);
      const int idx4 = engine::random::rand_max_n(4); // 0 to 3
      const int layout_id = big ? (idx4 * 2 + 1)      // 1,3,5,7
                                : (idx4 * 2 + 2);     // 2,4,6,8

      auto* rest = real_estate_.create_restaurant_with_layout(
          *players_[i], *active_city(), layout_id);
      if (!rest) {
        Logger::warn("Failed to create restaurant for player {}", i);
        continue;
      }

      load_initial_pizzas(context, core::PlayerId{i});
    }
  }

  // All players start in the same city.
  for (int i = 0; i < NUMBER_OF_PLAYERS; ++i) {
    if (players_[i]) {
      if (players_[i]->is_human_controlled()) {
        player_city_[i] = cfg.city_id;
      }
      players_[i]->mark_city_visited(cfg.city_id);
    }
  }

  register_bank_accounts();

  initialise_city_for_ai(*active_city());

  // We start at 9 in the morning on the first day, so 9 times 6 time blocks:
  time_.time_blocks(9 * 6);

  // Seed popularity, demographics and economy metrics so they are populated
  // before the first turn starts the first restaurant day.
  for (int i = 0; i < NUMBER_OF_PLAYERS; ++i) {
    active_player(core::PlayerId{i});
    for (auto* restaurant : restaurants_for(core::PlayerId{i})) {
      if (restaurant) {
        restaurant->recalculate_economy_metrics(*this);
      }
    }
  }
  active_player(core::PlayerId{0});

  // Build initial pizza charts so they're not empty on the first day.
  // AI menu slots are pre-warmed so they'll populate the chart immediately.
  if (auto* city = active_city()) {
    city->pizza_charts().build(*this);
  }

  initializer_.reset(); // Clear after use
}

void GameState::add_restaurant(std::unique_ptr<Restaurant> restaurant) {
  // Keep next_restaurant_id_ above any loaded ID to avoid collisions
  if (restaurant->id().value >= next_restaurant_id_.value) {
    next_restaurant_id_.value = restaurant->id().value + 1;
  }
  restaurants_.push_back(std::move(restaurant));
}

void GameState::remove_restaurant(core::RestaurantId r_id) {
  // Clear the property link
  for (auto& city_ptr : cities_) {
    if (!city_ptr) {
      continue;
    }
    for (auto* property : city_ptr->properties()) {
      if (property && property->has_restaurant() &&
          property->restaurant_id() == r_id) {
        property->clear_restaurant();
      }
    }
  }
  // Clear active restaurant
  if (active_restaurant_ && *active_restaurant_ == r_id) {
    active_restaurant_.reset();
  }
  // Actually remove the restaurant object
  std::erase_if(restaurants_,
                [r_id](const auto& r) { return r->id() == r_id; });
}

void GameState::reset_for_load() {
  // UI-layer presenters are registered once at startup; keep them across
  // the reset. The status hub is kept whole so the panel it feeds (owned by
  // the engine, outliving this reset) stays subscribed.
  auto staff_presenter = std::move(staff_event_presenter_);
  auto entry_presenter = std::move(monthly_entry_presenter_);
  auto turn_presenter = std::move(turn_presenter_);
  auto fade_presenter = std::move(fade_presenter_);
  auto hub = std::move(status_hub_);

  // We don't save these fields, so keep the ones we have.
  const ClockRate clock_rate = clock_rate_;
  const uint32_t accumulator = pit_accumulator_us_;
  const InterruptionRate rate = turn_.interruption_rate();

  *this = GameState{};
  // Re-seat self-referencing members; move assignment left them pointing at
  // the moved-from temporary.
  status_hub_ = std::move(hub);
  status_hub_.reset_for_new_game(time_);
  turn_ = TurnOrchestrator{*this};
  competitions_ = CompetitionCoordinator{*this};
  milestones_ = MilestoneTracker{*this};
  real_estate_ = RealEstate{*this};

  clock_rate_ = clock_rate;
  pit_accumulator_us_ = accumulator;
  turn_.interruption_rate(rate);

  staff_event_presenter_ = std::move(staff_presenter);
  monthly_entry_presenter_ = std::move(entry_presenter);
  turn_presenter_ = std::move(turn_presenter);
  fade_presenter_ = std::move(fade_presenter);
}

const CityViewMapState& GameState::city_view_view_state() const {
  return city_view_view_state_[active_player_.value];
}
void GameState::city_view_view_state(CityViewMapState cmvs) {
  city_view_view_state_[active_player_.value] = cmvs;
}

RankChange GameState::adjust_underworld_score(Player& player, int delta) {
  auto rc = player.social_status().adjust_underworld(delta, difficulty_level_);
  if (rc.promoted()) {
    // Underworld rank MilestoneIds run from AsGoodAsDead(15) to Godfather(27)
    auto id = static_cast<core::events::MilestoneId>(15 + rc.new_rank);
    record_milestone(id, player.id());
    player.character().adjust_courage(5);
    player.character().adjust_personality(4);
  }
  else if (rc.demoted()) {
    player.character().adjust_courage(-7);
    player.character().adjust_personality(-3);
  }
  return rc;
}

RankChange GameState::adjust_social_score(Player& player, int delta) {
  auto rc = player.social_status().adjust_social(delta, difficulty_level_);
  if (rc.promoted()) {
    // Business rank MilestoneIds run from LaughingStock(1) to StatusTycoon(14)
    auto id = static_cast<core::events::MilestoneId>(1 + rc.new_rank);
    record_milestone(id, player.id());
    player.character().adjust_popularity(5);
    player.character().adjust_personality(4);
  }
  else if (rc.demoted()) {
    player.character().adjust_popularity(-7);
    player.character().adjust_personality(-3);
  }
  return rc;
}

void GameState::initiate_arrest(ArrestReason reason,
                                std::function<void()> on_complete) {
  auto* city = active_city();
  if (!city) {
    return;
  }
  auto police_actors = city->actors(ActorType::POLICE);
  if (police_actors.empty()) {
    return;
  }
  ui_.push<PoliceState>(PoliceState{.id = police_actors[0]->id(),
                                    .reason = reason,
                                    .on_complete = std::move(on_complete)});
}

bool GameState::check_police_events() {
  // Only reached on a human player's turn.
  auto* player = active_player();

  using core::item::Item;
  auto& inv = player->inventory();
  int contraband = 0;
  for (auto item :
       {Item::FlameThrower, Item::Chemicals, Item::Bazooka, Item::Bomb}) {
    contraband += inv[item];
  }
  if (contraband == 0) {
    return false;
  }

  if (!engine::random::rand_percentage_chance(2)) {
    return false;
  }

  int evasion = player->character().dexterity();
  if (engine::random::rand_percentage_chance(evasion)) {
    return false;
  }

  initiate_arrest(ArrestReason::PosessingWeapons);
  return true;
}

Restaurant* GameState::restaurant(core::RestaurantId r_id) const {
  for (const auto& r : restaurants_) {
    if (r->id() == r_id) {
      return r.get();
    }
  }
  return nullptr;
}

std::vector<Restaurant*> GameState::restaurants_for(core::PlayerId pid) const {
  std::vector<Restaurant*> result;
  for (const auto& uptr : restaurants_) {
    auto owner = uptr->property().owner_or_tenant();
    if (owner && *owner == pid) {
      result.push_back(uptr.get());
    }
  }
  return result;
}

int GameState::restaurant_count_for_player_in_city(core::PlayerId pid,
                                                   core::CityId city) const {
  int count = 0;
  for (const auto* rest : restaurants_for(pid)) {
    if (rest->property().city().id() == city) {
      count++;
    }
  }
  return count;
}

void GameState::bootstrap_minimal_playable_world(GameContext& context) {
  // Only bootstrap once - if already initialized, just return
  if (!initializer_ && !restaurants_.empty()) {
    return;
  }

  begin_initialization();
  auto& cfg = initializer();
  cfg.mode = GameMode::QUICKSTART;
  cfg.size = init::RestaurantSize::BIG;
  cfg.difficulty = init::DifficultyLevel::PUSHOVER;
  cfg.city_id = core::CityId{6}; // Madrid because that's where I like to start

  cfg.players.resize(NUMBER_OF_PLAYERS);
  for (int i = 0; i < NUMBER_OF_PLAYERS; ++i) {
    auto* character = CharacterFactory::create_character(i);
    cfg.players[i].name = character->last_name + " Pizza";
    cfg.players[i].character_id = i;
    cfg.players[i].is_human = (i == 0);
  }
  // goes through the normal flow, pushes CityView:
  finalize_initialization(context);
  active_restaurant({0}); // first restaurant of first player

  // Pop the CityView that finalize_initialization pushed, since the caller
  // will push their own desired view
  ui().pop();
}

void GameState::restaurant_placement(std::vector<FurnitureBatch> batch) {
  furniture_placement_ = std::move(batch);
}
std::vector<FurnitureBatch>& GameState::restaurant_placement() {
  return furniture_placement_;
}

namespace {
// Index of `from` in `list`, or nullopt when absent.
std::optional<std::size_t>
restaurant_index(const std::vector<Restaurant*>& list,
                 core::RestaurantId from) {
  for (std::size_t i = 0; i < list.size(); ++i) {
    if (list[i]->id() == from) {
      return i;
    }
  }
  return std::nullopt;
}
} // namespace

std::vector<Restaurant*>
GameState::restaurants_in_city(core::PlayerId pid, core::CityId city) const {
  std::vector<Restaurant*> list;
  for (auto* rest : restaurants_for(pid)) {
    if (rest->property().city().id() == city) {
      list.push_back(rest);
    }
  }
  return list;
}

std::vector<Restaurant*>
GameState::restaurant_cycle(core::PlayerId pid, core::RestaurantId from) const {
  const auto* current = restaurant(from);
  return restaurants_in_city(pid, current ? current->property().city().id()
                                          : active_city_);
}

std::optional<core::RestaurantId>
GameState::next_restaurant(core::PlayerId pid, core::RestaurantId from) const {
  auto list = restaurant_cycle(pid, from);
  if (list.empty()) {
    return std::nullopt;
  }

  // if `from` is not found, just take the first
  auto idx = restaurant_index(list, from);
  const std::size_t next_idx = idx ? (*idx + 1) % list.size() : 0;
  return list[next_idx]->id();
}

std::optional<core::RestaurantId>
GameState::prev_restaurant(core::PlayerId pid, core::RestaurantId from) const {
  auto list = restaurant_cycle(pid, from);
  if (list.empty()) {
    return std::nullopt;
  }

  auto idx = restaurant_index(list, from);
  const std::size_t prev_idx = idx ? (*idx + list.size() - 1) % list.size() : 0;
  return list[prev_idx]->id();
}

void GameState::load_initial_pizzas(GameContext& context,
                                    core::PlayerId player_id) {
  Player* player = this->player(player_id);
  if (!player) {
    Logger::warn("Could not find player {} to load initial pizzas",
                 player_id.value);
    return;
  }

  auto& recipes = player->recipes();

  // Get the bob resource and create the pizza size function
  auto& zutaten_bob = context.bob->get("bob/zutaten.bob");
  auto get_bob_size =
      make_ingredient_bob_size_fn<Bob, core::food::Topping>(zutaten_bob);

  const int number_of_canonical_pizzas_to_load = 4;
  for (int i = 0; i < number_of_canonical_pizzas_to_load; ++i) {
    const auto cid = static_cast<core::food::CanonicalPizza>(i);

    // Create pizza from disk
    auto pizza =
        PizzaFactory::create_pizza(cid, get_bob_size, customer_taste_profiles_);
    if (!pizza) {
      Logger::warn("Failed to load canonical pizza {}", i);
      continue;
    }

    // Add to player's recipes
    auto rid = recipes.create_from_canonical(cid, std::move(*pizza));
    // Logger::info("Loaded initial pizza {} for player {} into recipe slot {}",
    // i, player_id.value, rid.value);
  }
}

void GameState::recalculate_all_pizza_demographics() {
  for (auto& player_ptr : players_) {
    if (!player_ptr) {
      continue;
    }
    auto& recipes = player_ptr->recipes();
    for (auto& slot : recipes.canon) {
      if (slot.created()) {
        slot.recipe->pizza_mut().calculate_demographic_scores(
            customer_taste_profiles_);
      }
    }
    for (auto& recipe : recipes.user) {
      recipe.pizza_mut().calculate_demographic_scores(customer_taste_profiles_);
    }
  }
}

// Game options delegates to ConfigManager (single source of truth)
GameOptions& GameState::options() {
  auto* config = ConfigManager::instance();
  if (!config) {
    fatal("ConfigManager not initialized when accessing GameState::options()");
  }
  return config->options();
}

const GameOptions& GameState::options() const {
  auto* config = ConfigManager::instance();
  if (!config) {
    fatal("ConfigManager not initialized when accessing GameState::options()");
  }
  return config->options();
}

// Actor call management (for angry realtors/etc refusing calls)
void GameState::refuse_calls_from(int actor_id) {
  actors_refusing_calls_.insert(actor_id);
}

std::vector<int> GameState::refused_call_actors() const {
  std::vector<int> ids(actors_refusing_calls_.begin(),
                       actors_refusing_calls_.end());
  std::ranges::sort(ids);
  return ids;
}

bool GameState::is_refusing_calls(int actor_id) const {
  return actors_refusing_calls_.contains(actor_id);
}

void GameState::clear_all_refused_calls() { actors_refusing_calls_.clear(); }

void GameState::request_resolution_change(ScreenResolution res) {
  pending_resolution_ = res;
}

std::optional<ScreenResolution> GameState::pending_resolution_change() const {
  return pending_resolution_;
}

void GameState::clear_resolution_request() { pending_resolution_.reset(); }

void GameState::request_crt_change(bool enabled) { pending_crt_ = enabled; }

std::optional<bool> GameState::pending_crt_change() const {
  return pending_crt_;
}

void GameState::clear_crt_request() { pending_crt_.reset(); }

core::food::IngredientDemand
GameState::total_ingredient_need_for_player(core::PlayerId player_id,
                                            core::CityId city) const {
  core::food::IngredientDemand demand_grams{};

  for (auto* restaurant : restaurants_in_city(player_id, city)) {
    if (!restaurant || !restaurant->wants_open()) {
      continue;
    }

    demand_grams += restaurant->daily_ingredient_demand_cache();
  }

  return demand_grams;
}

GameMode GameState::game_mode() const { return game_mode_; }

void GameState::game_mode(GameMode mode) { game_mode_ = mode; }

int GameState::human_player_count() const {
  int h_count = 0;
  for (const auto& player : players_) {
    if (player->is_human_controlled()) {
      h_count++;
    }
  }
  return h_count;
}

std::vector<core::CityId> GameState::simulated_cities() const {
  std::vector<core::CityId> cities;
  for (int i = 0; i < NUMBER_OF_PLAYERS; ++i) {
    if (!players_[i] || !players_[i]->is_human_controlled() ||
        !player_city_[i]) {
      continue;
    }
    if (std::ranges::find(cities, *player_city_[i]) == cities.end()) {
      cities.push_back(*player_city_[i]);
    }
  }
  return cities;
}

std::vector<core::CityId>
GameState::simulated_cities_for(core::PlayerId pid) const {
  const auto& p = players_[pid.value];
  if (p && !p->is_human_controlled()) {
    return simulated_cities();
  }
  if (auto city = player_city_[pid.value]) {
    return {*city};
  }
  return {};
}

bool GameState::bank_transfer(int account_number, Money amount,
                              Player& sender) {
  // A mayor's account number only works in their own city.
  MayorOffice* mayor = nullptr;
  bool donation = false;
  if (auto* here = active_city()) {
    auto& office = here->mayor_office();
    if (account_number == office.private_bank_account()) {
      mayor = &office;
      donation = true;
    }
    else if (office.has_public_account() &&
             account_number == office.public_bank_account()) {
      mayor = &office;
    }
  }

  auto* account = mayor ? nullptr : bank_accounts_.find(account_number);
  if (!mayor && !account) {
    return false;
  }

  const auto& date = time().current_date();

  // Debit the sender
  if (!sender.spend_cash(date, Account::Others, amount)) {
    return false;
  }

  // Donations to the private account buy favor; a transfer to the public
  // account is how the award participation fee is paid.
  if (mayor) {
    if (donation) {
      mayor->add_favor(sender.id().value, amount);
    }
    else {
      mayor->fee_paid(sender.id().value);
    }
    return true;
  }

  // Credit the target
  if (account->player_id) {
    auto* target = player(*account->player_id);
    if (target) {
      target->financials().transfer(date, Account::Others, Account::Cash,
                                    amount, AccountingScope::Operational);
    }
  }

  // Track the deposit (for the mafia to query)
  bank_accounts_.deposit(account_number, amount);

  return true;
}
