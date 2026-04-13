/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/state/GameState.h"

#include <algorithm>
#include <cassert>
#include <ranges>

#include <magic_enum/magic_enum.hpp>
#include <utility>
#include "conversations/MayorConversation.h"
#include "core/character/Actor.h"
#include "core/character/Awards.h"
#include "core/character/Player.h"
#include "core/city/District.h"
#include "core/city/Property.h"
#include "core/init/NewGameConfig.h"
#include "core/restaurant/AIEconomy.h"
#include "core/restaurant/Restaurant.h"
#include "core/restaurant/StaffEventDialogs.h"
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
#include "resources/util/Platform.h"
#include "views/screens/StatusDisplay.h"
#include "views/ui/PopupManager.h"
#include "views/ui/finance/FinanceComponents.h"
#include "views/ui/popups/AlertPopup.h"
#include "views/ui/popups/InvoicePopup.h"
#include "views/ui/popups/OptionsPopup.h"

// Constructor and destructor defined here to allow unique_ptr<StatusDisplay>
// with forward declaration
GameState::GameState() = default;
GameState::~GameState() = default;

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
  for (std::size_t i = 0; i < NUMBER_OF_PLAYERS; ++i) {
    if (state.cities_[i]) {
      os << "  city[" << i << "]: " << *state.cities_[i];
    }
  }

  os << "UIStates (";
  // os << ui_;
  os << "UIStates )" << "\n";

  return os;
}

Player* GameState::player(core::PlayerId player_id) const {
  assert(player_id.value >= 0 && player_id.value < NUMBER_OF_PLAYERS);

  return players_[player_id.value].get();
}

Player* GameState::active_player() const {
  return players_[active_player_.value].get();
}

bool GameState::spend_cash(Player& player, Account to, Money amount) {
  if (player.spend_cash(time().current_date(), to, amount)) {
    return true;
  }

  status_hub_.post_direct({
      .payload = "YOU DON'T HAVE ENOUGH MONEY!!!",
      .scope = StatusScope::Global,
      .ttl = std::chrono::milliseconds{3000},
  });
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

  status_hub_.post_direct({
      .payload = "YOU DON'T HAVE ENOUGH MONEY!!!",
      .scope = StatusScope::Global,
      .ttl = std::chrono::milliseconds{3000},
  });
  return false;
}

void GameState::deduct_staff_wages_for_restaurant(
    core::RestaurantId restaurant_id) {
  auto* restaurant = this->restaurant(restaurant_id);
  if (!restaurant) {
    return;
  }

  Money total_wages{0};

  // Calculate total weekly wages
  for (const auto& cook : restaurant->cooks()) {
    if (cook) {
      total_wages = total_wages + cook->payment();
    }
  }
  for (const auto& waiter : restaurant->waiters()) {
    if (waiter) {
      total_wages = total_wages + waiter->payment();
    }
  }
  if (restaurant->manager()) {
    total_wages = total_wages + restaurant->manager()->payment();
  }

  // Deduct wages (this will show insufficient funds message if needed)
  // TODO: Figure out exactly what the effect of not being able to pay staff is
  spend_cash(Account::Staff, total_wages, restaurant_id);
}

void GameState::show_staff_invoice_popup(core::RestaurantId restaurant_id) {
  auto* rest = restaurant(restaurant_id);
  if (!rest || !context_) {
    return;
  }

  views::ui::finance::InvoiceConfig cfg;
  cfg.subtitle = "for " + rest->name();
  cfg.on_dismiss = [this, restaurant_id] {
    deduct_staff_wages_for_restaurant(restaurant_id);
  };
  const auto empty_line =
      views::ui::finance::InvoiceLine{.label = "", .value = std::nullopt};

  cfg.lines.push_back({"staff expenses", std::nullopt});
  cfg.lines.push_back(empty_line);

  Money total;
  for (const auto& cook : rest->cooks()) {
    if (cook) {
      auto payment = cook->payment();
      cfg.lines.push_back(
          {cook->character().first_name() + " " + cook->character().last_name(),
           payment});

      total += payment;
      cfg.lines.push_back(empty_line);
    }
  }
  for (const auto& waiter : rest->waiters()) {
    if (waiter) {
      auto payment = waiter->payment();
      cfg.lines.push_back({waiter->character().first_name() + " " +
                               waiter->character().last_name(),
                           payment});
      total += payment;
      cfg.lines.push_back(empty_line);
    }
  }
  if (rest->manager()) {
    auto payment = rest->manager()->payment();
    cfg.lines.push_back({rest->manager()->character().first_name() + " " +
                             rest->manager()->character().last_name(),
                         payment});
    total += payment;
    cfg.lines.push_back(empty_line);
  }

  if (cfg.lines.empty()) {
    cfg.lines.push_back({"", Money{0}});
  }
  else {
    cfg.lines.push_back({"", total});
  }

  context_->popup->origin({74, 40});
  context_->popup->create<InvoicePopup>(std::move(cfg));
}

void GameState::show_monthly_invoice_popup(core::PlayerId player_id) {
  auto* player = this->player(player_id);
  if (!player || !context_) {
    return;
  }

  auto loan_costs = player->monthly_loan_costs();

  // Collect rent from all rented restaurant properties
  Money total_rent{0};
  struct RentEntry {
    core::RestaurantId restaurant_id;
    Money amount;
  };
  std::vector<RentEntry> rents;
  for (auto* rest : restaurants_for(player->id())) {
    if (rest && rest->property().is_rented()) {
      Money rent = rest->property().get_monthly_rent();
      if (rent.cents > 0) {
        rents.push_back({rest->id(), rent});
        total_rent += rent;
      }
    }
  }

  Money investment_returns = player->monthly_investment_returns();
  Money total = loan_costs.repayment + loan_costs.interest + total_rent;

  views::ui::finance::InvoiceConfig cfg;
  cfg.subtitle = "for " + player->name();
  const auto empty_line =
      views::ui::finance::InvoiceLine{.label = "", .value = std::nullopt};

  cfg.lines.push_back({"costs per month", std::nullopt});
  cfg.lines.push_back(empty_line);

  // TODO: Figure out investments and which is which
  cfg.lines.push_back({"LOAN REPAYMENT:", loan_costs.repayment});
  cfg.lines.push_back({"LOAN INTEREST:", loan_costs.interest});
  cfg.lines.push_back({"RENT:", total_rent});
  cfg.lines.push_back({"INVESTMENT RETURNS:", investment_returns});
  cfg.lines.push_back(empty_line);
  cfg.lines.push_back({"TOTAL:", total});

  cfg.on_dismiss = [this, player, rents = std::move(rents)]() {
    const auto& date = time_.current_date();
    player->apply_monthly_loan_payments(date);
    player->apply_monthly_investment_payouts(date, options().fix_legacy_bugs);
    for (const auto& entry : rents) {
      player->financials().transfer(date, Account::Cash, Account::Rents,
                                    entry.amount, entry.restaurant_id,
                                    AccountingScope::Operational);
    }
    if (status_display_) {
      status_display_->mark_dirty();
    }
  };

  context_->popup->origin({74, 40});
  context_->popup->create<InvoicePopup>(std::move(cfg));
}

City* GameState::city(core::CityId city_id) const {
  assert(city_id.value >= 0 && city_id.value < NUMBER_OF_CITIES);
  return cities_[city_id.value].get();
}

std::vector<City*> GameState::visited_cities() const {
  std::vector<City*> result;
  for (const auto& city_ptr : cities_) {
    if (city_ptr) {
      result.push_back(city_ptr.get());
    }
  }
  return result;
}

bool GameState::is_city_loaded(core::CityId city_id) const {
  assert(city_id.value >= 0 && city_id.value < NUMBER_OF_CITIES);
  return cities_[city_id.value] != nullptr;
}

core::CityId GameState::active_city_id() const { return active_city_; }

City* GameState::active_city() const {
  assert(active_city_.value >= 0 && active_city_.value < NUMBER_OF_CITIES);

  return cities_[active_city_.value].get();
}

void GameState::active_city(core::CityId city_id) {
  active_city_ = city_id;
  if (cities_[city_id.value] == nullptr) {
    cities_[city_id.value] = CityFactory::create_city(city_id);
  }
}

void GameState::ensure_city_loaded(core::CityId city_id) {
  // Create city if it doesn't exist, but don't change active_city_
  if (cities_[city_id.value] == nullptr) {
    Logger::info("Creating city {} during ensure_city_loaded", city_id.value);
    cities_[city_id.value] = CityFactory::create_city(city_id);
  }
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

void GameState::advance_game_time() {
  time_.advance_10_minutes();

  using core::time::TimeOfDay;
  const auto now = time_.time_of_day();

  if (now == TimeOfDay{9, 0}) {
    status_hub_.post_direct(StatusMsg{
        .payload = "It's 9 am. The restaurants are opening.",
        .scope = StatusScope::Global,
    });
  }
  else if (now == TimeOfDay{21, 0}) {
    status_hub_.post_direct(StatusMsg{
        .payload = "It's 9 pm. The restaurants are closing.",
        .scope = StatusScope::Global,
    });
  }

  // Prevent interruptions while player is in area where that would be
  // inconvenient.
  if (!prevent_auto_end_of_turn_) {
    check_mafia_events();
  }
  check_police_events();
  process_pending_messages();

  // Fire deferred staff event dialogs at 10 AM the next day. The check runs
  // on every block while time >= 10:00, matching the original's behaviour
  // where the popup fires at the first update_status_and_time call >= 0x3C.
  if (!end_of_turn_state_ && !showing_deferred_staff_event_ &&
      !showing_deferred_inspection_ && !prevent_auto_end_of_turn_ &&
      !deferred_staff_events_.empty() && time_.is_past_time(10, 0)) {
    show_next_deferred_staff_event();
  }

  // Reset inspection gate when the MayorState conversation has ended.
  if (showing_deferred_inspection_ && !ui_.is_top<MayorState>()) {
    showing_deferred_inspection_ = false;
  }

  // Fire deferred mayor inspection events at 10 AM (after staff events drain).
  if (!end_of_turn_state_ && !showing_deferred_staff_event_ &&
      !showing_deferred_inspection_ && !prevent_auto_end_of_turn_ &&
      !deferred_mayor_inspections_.empty() && time_.is_past_time(10, 0)) {
    show_next_deferred_inspection();
  }

  // Fire deferred mayor award events at 10 AM (after inspections).
  if (!end_of_turn_state_ && !showing_deferred_staff_event_ &&
      !showing_deferred_inspection_ && !ui_.is_top<MayorState>() &&
      !prevent_auto_end_of_turn_ && !deferred_mayor_awards_.empty() &&
      time_.is_past_time(10, 0)) {
    auto ev = deferred_mayor_awards_.front();
    deferred_mayor_awards_.erase(deferred_mayor_awards_.begin());
    process_mayor_award_event(ev);
  }
}

void GameState::deliver_message(core::events::LeventId msg_id,
                                std::function<void()> on_dismiss) {
  auto* player = active_player();
  if (!player) {
    return;
  }

  // Ensure CV entry exists (no-op if already scheduled)
  message_manager_.schedule_message(msg_id, player->id(),
                                    time().current_date());

  const auto& data = message_manager_.get_data(msg_id);
  if (data.score > 0) {
    player->add_score(data.score);
  }
  if (data.background != core::events::ZwBackground::NONE) {
    FullscreenStatusState fss{.message_id = msg_id,
                              .on_dismiss = std::move(on_dismiss)};
    ui_.push<FullscreenStatusState>(std::move(fss));
  }
  message_manager_.mark_as_shown(msg_id, player->id());
}

void GameState::process_pending_messages() {
  auto* player = active_player();
  if (!player || !player->is_human_controlled()) {
    return;
  }

  auto pending = message_manager_.get_pending_for_player(player->id());
  for (auto msg_id : pending) {
    deliver_message(msg_id);
  }
}

void GameState::pause_time_advancement() { time_advancement_enabled_ = false; }

void GameState::resume_time_advancement() {
  time_advancement_enabled_ = true;

  // Immediately check if we should end the turn - don't wait for the
  // next accumulator tick. This handles the case where time advanced
  // past end-of-day during furniture placement.
  if (is_player_day_over()) {
    request_end_of_turn();
  }
}

bool GameState::is_past_player_end_of_day() const {
  auto end_time = active_player()->end_of_day_time();
  return time_.is_past_time(end_time.hour, end_time.minute);
}

bool GameState::is_player_day_over() const {
  if (prevent_auto_end_of_turn_) {
    return false;
  }
  return is_past_player_end_of_day();
}

bool GameState::advance_time_blocks(int n) {
  for (int i = 0; i < n; ++i) {
    advance_game_time();

    if (is_player_day_over()) {
      // Reached player end-of-day - stop advancing but don't auto-trigger
      // end_of_turn. This matches original behavior where terminal
      // conversations can continue past end-of-day.
      return true;
    }
  }
  return false;
}

const GameTime& GameState::time() const { return time_; }
void GameState::request_end_of_turn() { should_end_of_turn_ = true; }

void GameState::skip_to_next_player() {
  suppress_end_of_turn_reports_ = true;
  end_of_turn();
}

void GameState::update(uint32_t elapsed_ms) {
  if (active_player_.value < 0) {
    return;
  }

  status_hub_.update(elapsed_ms);

  // Handle end-of-turn processing delay between restaurants,
  // so that we have visible changes to the money per restaurant processed.
  // Yes it would be so much easier to just do them all in a quick loop but
  // I wanted to match the original behavior of visibly doing them one at a
  // time.
  if (end_of_turn_state_ && end_of_turn_state_->delay_frames > 0) {
    if (--end_of_turn_state_->delay_frames == 0) {
      process_next_restaurant();
    }
    return; // Don't advance game time while processing end of turn
  }

  // Only accumulate time when advancement is enabled
  if (time_advancement_enabled_) {
    time_accumulator_ += elapsed_ms;

    constexpr uint32_t PASS_TEN_MINUTES_MS = 11 * 1000;
    while (time_accumulator_ >= PASS_TEN_MINUTES_MS) {
      time_accumulator_ -= PASS_TEN_MINUTES_MS;

      if (is_past_player_end_of_day()) {
        time_accumulator_ = 0;
        if (!prevent_auto_end_of_turn_) {
          request_end_of_turn();
        }
        break;
      }

      advance_game_time();
    }
  }

  if (should_end_of_turn_) {
    end_of_turn();
    should_end_of_turn_ = false;
  }
}

void GameState::end_of_turn() {
  pause_time_advancement();
  // This is called at the end of a player's turn (when they reach their closing
  // time or the user clicks the 'end turn' button). Each player has their own
  // start/end times during the same calendar day. Only after all players finish
  // their turns do we actually end the calendar day.

  auto* current_player = active_player();

  // Compute interactivity early so we can suppress status messages before
  // any end-of-day processing (AI expansion posts, financial warnings, etc.)
  bool interactive = current_player->is_human_controlled() &&
                     !current_player->is_on_holiday() &&
                     !current_player->is_in_jail() &&
                     !suppress_end_of_turn_reports_;
  suppress_end_of_turn_reports_ = false;
  if (!interactive) {
    status_hub_.suppress(true);
  }

  // Process player end of day accounting (includes advertising costs,
  // campaign updates, financial warnings, AI expansion)
  current_player->end_of_day(*this);

  auto player_restaurants = restaurants_for(current_player->id());
  end_of_turn_state_ = EndOfTurnState{
      .restaurants = player_restaurants,
      .current_index = 0,
      .is_human = interactive,
  };

  // Start processing the first restaurant
  process_next_restaurant();
}

void GameState::process_next_restaurant() {
  constexpr int DELAY_FRAMES_BETWEEN_RESTAURANTS = 2;
  auto& state = *end_of_turn_state_;

  while (true) {
    // Skip null entries
    while (state.current_index < state.restaurants.size() &&
           state.restaurants[state.current_index] == nullptr) {
      state.current_index++;
    }

    // All restaurants processed?
    if (state.current_index >= state.restaurants.size()) {
      finish_end_of_turn();
      return;
    }

    auto* restaurant = state.restaurants[state.current_index];

    restaurant->end_of_day(*this);
    restaurant->property().calculate_demographics(restaurant);

    if (status_display_) {
      status_display_->mark_dirty();
    }

    // Queue mayor inspection event for delivery next day at 10 AM.
    if (state.is_human) {
      if (auto insp = restaurant->take_pending_inspection()) {
        deferred_mayor_inspections_.push_back({*insp, restaurant->id()});
      }
    }

    // Queue player-facing staff event dialogs for delivery next day at 10 AM.
    // Replace any existing queued event for the same staff member so that
    // only the most recent event survives
    if (state.is_human) {
      for (auto& ev : restaurant->take_pending_staff_events()) {
        auto it = std::find_if(deferred_staff_events_.begin(),
                               deferred_staff_events_.end(),
                               [&](const DeferredStaffEvent& d) {
                                 return d.event.job_role == ev.job_role &&
                                        d.event.slot_index == ev.slot_index &&
                                        d.restaurant_id == restaurant->id();
                               });
        if (it != deferred_staff_events_.end()) {
          it->event = ev;
        }
        else {
          deferred_staff_events_.push_back({ev, restaurant->id()});
        }
      }
    }

    // Show daily report if needed -- yields to UI callback
    if (state.is_human && !options().automatic_report &&
        restaurant->wants_open() && !restaurant->is_force_closed()) {
      ui().push<RestaurantStatsState>(restaurant->id(), std::nullopt, true);
      // on_restaurant_processed() will increment index and re-enter
      return;
    }

    state.current_index++;

    // Human turns: short delay between restaurants so the status display
    // can update visually. update() will re-enter process_next_restaurant().
    if (state.is_human && state.current_index < state.restaurants.size()) {
      state.delay_frames = DELAY_FRAMES_BETWEEN_RESTAURANTS;
      return;
    }
    // AI turns: continue the loop without yielding frames, so nothing
    // from the AI's turn leaks into the rendered output.
  }
}

void GameState::show_next_deferred_staff_event() {
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

  auto* restaurant = this->restaurant(deferred.restaurant_id);
  if (!restaurant) {
    // Restaurant gone (sold/closed since EOD), skip and try next.
    show_next_deferred_staff_event();
    return;
  }

  show_staff_event_dialog(deferred.event, restaurant,
                          [this]() { show_next_deferred_staff_event(); });
}

void GameState::trigger_mayor_inspection(core::RestaurantId rid,
                                         bool is_first) {
  deferred_mayor_inspections_.push_back({is_first, rid});
  if (!showing_deferred_inspection_) {
    show_next_deferred_inspection();
  }
}

void GameState::show_next_deferred_inspection() {
  if (deferred_mayor_inspections_.empty()) {
    showing_deferred_inspection_ = false;
    if (is_player_day_over()) {
      request_end_of_turn();
    }
    return;
  }

  auto* city = active_city();
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

  auto* restaurant = this->restaurant(deferred.restaurant_id);
  if (!restaurant) {
    // Restaurant gone since EOD, skip.
    showing_deferred_inspection_ = false;
    show_next_deferred_inspection();
    return;
  }

  // Corruption check: if mayor favor score passes, award a star regardless of
  // quality. Only applies on the upside.
  auto* player = active_player();
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

  MayorConversationType conv_type =
      deferred.is_first ? MayorConversationType::FirstQualityInspection
                        : MayorConversationType::QualityInspection;
  ui_.push<MayorState>(MayorState{.id = mayor_id,
                                  .type = conv_type,
                                  .restaurant_id = deferred.restaurant_id,
                                  .is_first_inspection = deferred.is_first,
                                  .award_winner_player_id = std::nullopt,
                                  .inspection_star_gained = outcome.star_gained,
                                  .inspection_star_lost = outcome.star_lost});
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
  // Restaurant-based awards (MostBeautiful, GoldenBowTie) are computed here
  // because MayorOffice has no access to restaurant data.
  std::optional<int> award_winner = award_event.winner_player_id;

  if (award_event.is_ceremony) {
    auto all_players = players();
    int pc = human_player_count();
    auto& mayor_office = city->mayor_office();

    if (award_event.type == MayorAwardType::MostBeautifulRestaurant) {
      int best_score = -1;
      award_winner = std::nullopt;
      for (int i = 0; i < pc; ++i) {
        if (!all_players[i] || !mayor_office.fee_paid(i)) {
          continue;
        }
        int score = 0;
        for (auto* rest : restaurants_for(all_players[i]->id())) {
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
        for (auto* rest : restaurants_for(all_players[i]->id())) {
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
  using Type = core::restaurant::StaffEventData::Type;
  using Dialogs = core::restaurant::StaffEventDialogs;
  using engine::random::rand_between;
  using engine::random::rand_percentage_chance;

  bool is_persf = (event.type >= Type::DestroyCheeseStock);
  int section_index = 0;

  if (!is_persf) {
    switch (event.type) {
    case Type::Late:
      section_index = Dialogs::SECTION_LATE;
      break;
    case Type::Sick:
      section_index = Dialogs::SECTION_SICK;
      break;
    case Type::Raise:
      section_index = Dialogs::SECTION_RAISE;
      break;
    case Type::Quit:
      section_index = Dialogs::SECTION_QUIT;
      break;
    case Type::Maternity:
      section_index = Dialogs::SECTION_MATERNITY;
      break;
    default:
      on_done();
      return;
    }
  }
  else {
    section_index = static_cast<int>(event.type) -
                    static_cast<int>(Type::DestroyCheeseStock);
  }

  // Pick random line variants (1-3) for each group
  int ev = rand_between(1, 4);
  int e1 = rand_between(1, 4);
  int e2 = rand_between(1, 4);
  int e3 = rand_between(1, 4);

  auto get_line = [is_persf, section_index](int offset) -> std::string {
    if (is_persf) {
      return Dialogs::get_persf_line(section_index, offset);
    }
    return Dialogs::get_pers_line(section_index, offset);
  };

  std::string excuse_text =
      compat::format("{}: {}", restaurant->name(), get_line(ev));
  std::string o1_text = get_line(3 + e1);
  std::string o2_text = get_line(6 + e2);
  std::string o3_text = get_line(9 + e3);

  Staff* initial_staff = restaurant->staff_at(event.job_role, event.slot_index);
  const RawRenderable* staff_pic = nullptr;
  std::optional<uint8_t> staff_char_id;
  if (initial_staff) {
    staff_char_id = initial_staff->character().id();
    // Get the picture from the factory because the Staff object can go
    // away if they get fired. Technically I think we could hire a new character
    // in that same slot, but I don't care enough to special case that.
    staff_pic = CharacterFactory::create_character(*staff_char_id);
  }

  // Callback that applies the game effect and shows the optional reply popup.
  // Using shared_ptr so it can be safely shared between three option
  // lambdas.
  auto handle = std::make_shared<std::function<void(int)>>(
      [this, event, restaurant, is_persf, get_line, staff_pic,
       on_done = std::move(on_done)](int outcome) {
        using T = core::restaurant::StaffEventData::Type;
        using engine::random::rand_percentage_chance;
        Staff* s = restaurant->staff_at(event.job_role, event.slot_index);
        if (!s) {
          Logger::warn("Staff is a nullptr in event handling, ignoring.");
          return;
        }

        if (event.type == T::Quit) {
          if (outcome == 0) {
            // fire immediately, no reply popup.
            restaurant->fire_staff(event.job_role, event.slot_index);
            on_done();
            return;
          }
          // Apply salary raise, then check if they decide to stay.
          Money new_pay =
              (outcome == 1) ? s->payment() * 3 / 2 : s->payment() * 2;
          s->payment(new_pay);

          bool stays = rand_percentage_chance((outcome == 1) ? 40 : 80);
          if (stays) {
            int delta = 20 - static_cast<int>(s->character().motivation());
            s->adjust_motivation(delta); // Set motivation to 20
          }
          else {
            restaurant->fire_staff(event.job_role, event.slot_index);
          }
          // Show result line; always shown for Quit
          // Line 13 = "Sorry..." (left), line 14 = "OK, I'll stay!"
          std::string result = stays ? get_line(14) : get_line(13);
          context_->popup->create<AlertPopup>(std::move(result), on_done,
                                              staff_pic);
          return;
        }

        // Non-Quit effects
        switch (event.type) {
        case T::Late:
          s->adjust_motivation(-((outcome * 4) - 2));
          break;
        case T::Sick:
          s->adjust_motivation(-((outcome * 8) - 3));
          break;
        case T::Raise:
          s->adjust_motivation((1 - outcome) * 8);
          break;
        case T::Maternity:
          // Informational only; absence already set when event fired.
          break;
        default:
          // PERSF: fire staff on harsh response
          if (outcome == 2) {
            restaurant->fire_staff(event.job_role, event.slot_index);
          }
          break;
        }

        // Optional staff reply (50% for PERS events, 60% for PERSF)
        int reply_chance = is_persf ? 60 : 50;
        if (rand_percentage_chance(reply_chance)) {
          std::string reply = get_line(13 + outcome);
          context_->popup->origin({28, 40});
          context_->popup->create<AlertPopup>(std::move(reply), on_done,
                                              staff_pic);
        }
        else {
          on_done();
        }
      });

  // Show excuse, then the options popup when dismissed.
  context_->popup->origin({28, 40});
  context_->popup->create<AlertPopup>(
      std::move(excuse_text),
      [this, o1_text = std::move(o1_text), o2_text = std::move(o2_text),
       o3_text = std::move(o3_text), handle]() {
        std::vector<Option> options{
            {.text = o1_text, .callback = [handle]() { (*handle)(0); }},
            {.text = o2_text, .callback = [handle]() { (*handle)(1); }},
            {.text = o3_text, .callback = [handle]() { (*handle)(2); }},
        };
        context_->popup->origin({28, 40});
        context_->popup->create<OptionsPopup>(std::move(options));
      },
      staff_pic);
}

void GameState::on_restaurant_processed() {
  if (!end_of_turn_state_) {
    return;
  }

  // Move to next restaurant
  end_of_turn_state_->current_index++;
  process_next_restaurant();
}

void GameState::deliver_depot_stock(core::PlayerId player_id) {
  auto* city_ptr = active_city();
  if (!city_ptr) {
    return;
  }

  City& city = *city_ptr;

  auto* player = players_[player_id.value].get();
  if (!player) {
    return;
  }

  std::vector<core::RestaurantId> city_rest_ids;
  for (auto* r : restaurants_for(player_id)) {
    if (r && r->property().city().id() == city.id()) {
      city_rest_ids.push_back(r->id());
    }
  }

  if (city_rest_ids.empty()) {
    return;
  }

  auto& depot = city.central_depot(player_id.value);
  depot.sync_delivery_priorities(city_rest_ids);

  for (auto rest_id : depot.delivery_priorities()) {
    auto* rest = restaurant(rest_id);
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

      auto& depot_stock =
          depot.get_stock(static_cast<core::food::IngredientType>(i));
      if (depot_stock.quantity <= 0) {
        continue;
      }

      // We just top up the stock until it's at 2x yesterday's demand
      int current = rest->inventory()
                        .get_stock(static_cast<core::food::IngredientType>(i))
                        .quantity;
      int shortfall = needed - current;
      if (shortfall <= 0) {
        continue;
      }

      int transfer = std::min(shortfall, depot_stock.quantity);
      int quality = depot_stock.weighted_avg_quality;
      Money cost = depot_stock.weighted_avg_cost;

      depot_stock.consume(transfer);
      rest->inventory().add_ingredient(
          static_cast<core::food::IngredientType>(i), transfer, quality, cost);
    }
  }
}

void GameState::finish_end_of_turn() {
  auto& state = *end_of_turn_state_;

  // Freeze status display header for human players
  // This prevents AI players' money from being shown during turn cycling
  if (status_display_ && active_player()->is_human_controlled()) {
    status_display_->freeze();
  }

  // Move to next player and check if we wrapped around
  auto previous_player_id = active_player_;
  next_player();

  // If we wrapped around to player 0, all players have finished their turns.
  // Run end-of-day and weekly/monthly checks on the current (ending) date
  // before advancing to the next day.
  bool wrapped_to_player_zero =
      (previous_player_id.value > 0 && active_player_.value == 0);
  if (wrapped_to_player_zero) {
    request_fade_transition(20);
    end_of_day();
  }

  // PlayerTurnCard needs the upcoming day's date, but we haven't advanced yet
  // (reports still need the current date). Compute it without mutating state.
  GameDate next_date = wrapped_to_player_zero ? time_.current_date().add_days(1)
                                              : time_.current_date();

  // Push PlayerTurnCard first so it sits below reports on the stack (shown
  // last)
  ui().push<PlayerTurnCardState>(active_player_, next_date);

  // Weekly/monthly reports pushed on top, shown before PlayerTurnCard.
  // Checks run on the current (ending) date, which is still set here.
  if (state.is_human) {
    // Weekly reports on Saturday (pushed after monthly, shown before monthly)
    if (time_.current_date().is_end_of_week()) {
      // Show for ALL restaurants (not just open ones) - staff gets paid
      // regardless Open restaurants: WeeksTakings shown first, creates invoice
      // popup on dismiss Closed restaurants: invoice popup shown directly
      for (auto it = state.restaurants.rbegin(); it != state.restaurants.rend();
           ++it) {
        if (!*it) {
          continue;
        }

        if ((*it)->wants_open() && !(*it)->is_force_closed()) {
          // WeeksTakings will create the invoice popup when dismissed
          ui().push<WeeksTakingsState>((*it)->id(), true);
        }
        else {
          // No WeeksTakings for closed restaurants, show invoice directly
          show_staff_invoice_popup((*it)->id());
        }
      }
    }
    // Monthly costs invoice on last day of month (loan repayment + rent)
    if (time_.current_date().is_end_of_month()) {
      show_monthly_invoice_popup(previous_player_id);
    }
  }

  // Now advance time to the next player's start time
  auto start_time = players_[active_player_.value]->start_of_day_time();
  if (wrapped_to_player_zero) {
    time_.time_on_next_day(start_time.hour, start_time.minute);
  }
  else {
    time_.set_time_of_day(start_time.hour, start_time.minute);
  }

  // Clear end-of-turn state before starting next player's day
  end_of_turn_state_.reset();

  deliver_depot_stock(active_player_);

  auto next_player_restaurants = restaurants_for(active_player_);
  for (auto* restaurant : next_player_restaurants) {
    if (restaurant) {
      restaurant->start_of_day(*this);
    }
  }

  // Mark if this is an AI player that should auto-end after card is shown
  pending_ai_auto_end_turn_ =
      !players_[active_player_.value]->is_human_controlled();
  // Only unsuppress for human players; AI turns stay suppressed so the
  // baseline never leaks competitor info into the StatusDisplay.
  if (!pending_ai_auto_end_turn_) {
    status_hub_.suppress(false);
  }
  resume_time_advancement();
}

void GameState::end_of_day() {
  // This is called only after all players have finished their turns.
  // This handles the actual day transition and day-level operations.

  // Process end of day for all cities
  for (auto& city : cities_) {
    if (city) {
      city->end_of_day(active_city_ == city->id());
      // Run newspaper event system for each loaded city
      city->newspaper_event_system().daily_update(*this, *city);

      // Mayor account rotation and scheduling.
      city->mayor_office().end_of_day(time_.current_date(), bank_accounts_);
      auto award_events = city->mayor_office().process_daily_scheduling(
          time_.current_date(), human_player_count(), bank_accounts_);
      for (auto& ev : award_events) {
        deferred_mayor_awards_.push_back(ev);
      }
    }
  }

  // End of week processing on Saturday (tbd if that's right, probably not)
  // The original uses day_of_week == 6 which is Saturday
  if (time_.current_date().is_end_of_week()) {
    for (auto& city : cities_) {
      if (city) {
        city->reset_weekly_vendor_spending();
      }
    }

    // Pay weekly staff salaries and show weekly reports on Saturday
    for (auto& player : players_) {
      if (player) {
        player->end_of_week(*this);
      }
    }

    // Weekly taste drift: 30% chance to re-randomize one demographic
    if (engine::random::rand_percentage_chance(30)) {
      auto demo = static_cast<ConsumerType>(engine::random::rand_max_n(6));
      auto& tp = customer_taste_profiles_[demo];
      tp.fat = engine::random::rand_max_n(10);
      tp.sweet = engine::random::rand_max_n(10);
      tp.salty = engine::random::rand_max_n(10);
      tp.hot = engine::random::rand_max_n(10);
      tp.spicy = engine::random::rand_max_n(10);
      recalculate_all_pizza_demographics();
    }

    business_week_nr++;
  }

  // Monthly property availability cycling on last day of month
  if (time_.current_date().is_end_of_month()) {
    for (auto& city : cities_) {
      if (city) {
        city->end_of_month();
      }
    }
  }

  if (time_.current_date().is_end_of_year()) {
    business_week_nr = 1;
    for (auto& player : players_) {
      if (player) {
        auto& ch = player->character();
        ch.age(static_cast<uint8_t>(std::min(255, (int)ch.age() + 1)));
      }
    }
  }

  // Update AI menu slot history for pizza charts
  for (auto& player : players_) {
    if (player && !player->is_human_controlled()) {
      int rest_count = static_cast<int>(restaurants_for(player->id()).size());
      ai::update_ai_menu(*player, rest_count);
    }
  }

  // Rebuild pizza charts and control map for active city
  if (auto* city = active_city()) {
    city->pizza_charts().build(*this);
    city->control_map().build(*this, *city);
    check_market_share_milestones(*city);
  }

  // Clear actor refused calls (realtors cool down overnight)
  clear_all_refused_calls();
}

void GameState::check_market_share_milestones(const City& city) {
  using core::events::LeventId;
  const auto& pcts = city.control_map().player_percentages();
  for (int p = 0; p < NUMBER_OF_PLAYERS; ++p) {
    auto pid = core::PlayerId{p};
    if (!player(pid)) {
      continue;
    }
    float pct = pcts[p];
    // Original game bug: checks 5% but displays "10% market share" message
    int first_threshold = options().fix_legacy_bugs ? 10 : 5;
    if (pct >= 50) {
      message_manager_.schedule_message(
          LeventId::Achieved50PctMarketShareInCity, pid, time_.current_date());
    }
    if (pct >= 20) {
      message_manager_.schedule_message(
          LeventId::Achieved20PctMarketShareInCity, pid, time_.current_date());
    }
    if (pct >= first_threshold) {
      message_manager_.schedule_message(
          LeventId::Achieved10PctMarketShareInCity, pid, time_.current_date());
    }
  }
}

void GameState::check_restaurant_milestones(const Player& player) {
  using core::events::LeventId;
  auto pid = player.id();
  auto date = time_.current_date();

  auto player_restaurants = restaurants_for(pid);
  int count = 0;
  std::array<bool, NUMBER_OF_CITIES> cities_present = {};

  for (const auto* rest : player_restaurants) {
    if (!rest->ever_opened()) {
      continue;
    }
    count++;
    int city_val = rest->property().city().id().value;
    if (city_val >= 0 && city_val < NUMBER_OF_CITIES) {
      cities_present[city_val] = true;
    }
  }

  if (count == 1) {
    message_manager_.schedule_message(LeventId::OpenedFirstRestaurant, pid,
                                      date);
  }
  else if (count == 2) {
    message_manager_.schedule_message(LeventId::OpensSecondRestaurant, pid,
                                      date);
  }
  else if (count == 5) {
    message_manager_.schedule_message(LeventId::HasFiveRestaurants, pid, date);
  }
  int distinct_cities = static_cast<int>(
      std::count(cities_present.begin(), cities_present.end(), true));

  if (distinct_cities >= 2) {
    message_manager_.schedule_message(LeventId::FirstRestaurantInOtherCity, pid,
                                      date);
  }
  if (distinct_cities >= NUMBER_OF_CITIES) {
    message_manager_.schedule_message(LeventId::OutletsInEveryWesternCity, pid,
                                      date);
  }

  process_pending_messages();
}

void GameState::on_city_entered(core::CityId city_id, const Player& player) {
  using core::events::LeventId;
  auto arrival = static_cast<LeventId>(
      static_cast<int>(LeventId::ArrivalInParis) + city_id.value);
  message_manager_.schedule_message(arrival, player.id(), time_.current_date());
  process_pending_messages();
}

void GameState::check_mafia_events() {
  auto* player = active_player();
  auto* city = active_city();

  auto mafia_actors = city->actors(ActorType::MAFIA);
  if (mafia_actors.empty()) {
    return;
  }
  int mafia_id = mafia_actors[0]->id();
  const auto& date = time_.current_date();

  // Loan repayment check
  auto& loan = player->mafia_state().loan;
  if (loan && date >= loan->due_date) {
    ui_.push<MafiaState>(MafiaState{.id = mafia_id,
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
      ui_.push<MafiaState>(MafiaState{.id = mafia_id,
                                      .mode = MafiaMode::MissionOverdue,
                                      .restaurant_id = std::nullopt});
      return;
    }
    if (date > mission->deadline && !mission->completed) {
      ui_.push<MafiaState>(MafiaState{.id = mafia_id,
                                      .mode = MafiaMode::MissionOverdue,
                                      .restaurant_id = std::nullopt});
      return;
    }
    if (date >= mission->deadline && mission->completed) {
      ui_.push<MafiaState>(MafiaState{.id = mafia_id,
                                      .mode = MafiaMode::MissionDeadlineReached,
                                      .restaurant_id = std::nullopt});
      return;
    }
  }

  // Protection racket check (2% daily chance per restaurant)
  if (player->social_status().underworld_score < 500) {
    auto restaurants = restaurants_for(player->id());
    for (auto* restaurant : restaurants) {
      if (!restaurant->wants_open()) {
        continue;
      }
      auto timer = restaurant->protection_timer();
      // trigger if timer expired (==1) or never contacted (nullopt)
      if (timer.has_value() && *timer > 1) {
        continue;
      }
      if (!engine::random::rand_percentage_chance(2)) {
        continue;
      }
      bool first = !restaurant->protection_timer().has_value();
      // Set timer now to prevent re-triggering while the negotiation
      // screen is pending. The conversation will overwrite this with
      // the real cooldown value when it resolves.
      restaurant->protection_timer(30);
      ui_.push<MafiaState>(MafiaState{.id = mafia_id,
                                      .mode = MafiaMode::ProtectionRacketOffer,
                                      .restaurant_id = restaurant->id(),
                                      .first_contact = first});
      return;
    }
  }
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

void GameState::finalize_initialization(GameContext& context) {
  context_ = &context;
  if (!initializer_) {
    return;
  }

  init::Config cfg = *initializer_;

  // Use initializer to set permanent state
  game_mode_ = cfg.mode;
  active_city(cfg.city_id); // Set the active city before pushing UI state
  ui().push<CityViewState>(cfg.city_id);
  difficulty_level_ = static_cast<int>(cfg.difficulty);

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

      auto* rest = create_restaurant_with_layout(*players_[i], *active_city(),
                                                 layout_id, big);
      if (!rest) {
        Logger::warn("Failed to create restaurant for player {}", i);
        continue;
      }

      load_initial_pizzas(context, core::PlayerId{i});
    }
    else if (!player_item.is_human) {
      // AI players always start with a restaurant (Free or QuickStart),
      // gated by up to 8 courage% attempts matching the original algorithm.
      create_ai_starting_restaurant(*players_[i], *active_city());
    }
  }

  // Register fixed bank accounts for each player (111111 through 888888)
  for (int i = 0; i < NUMBER_OF_PLAYERS; ++i) {
    if (players_[i]) {
      bank_accounts_.register_player_account(core::PlayerId{i});
    }
  }

  // Register mayor bank accounts for all loaded cities
  for (auto& city : cities_) {
    if (city) {
      city->mayor_office().register_accounts(bank_accounts_);
    }
  }

  // Initialize AI player menu slots for pizza charts
  for (auto& player : players_) {
    if (player && !player->is_human_controlled()) {
      ai::init_ai_menu(*player);
    }
  }

  // Load article database
  article_database_.load_articles();

  // We start at 9 in the morning on the first day, so 9 times 6 time blocks:
  time_.time_blocks(9 * 6);

  // Run start_of_day for all restaurants so popularity, demographics, and
  // economy metrics are initialized before the player's first turn.
  for (int i = 0; i < NUMBER_OF_PLAYERS; ++i) {
    active_player(core::PlayerId{i});
    for (auto* restaurant : restaurants_for(core::PlayerId{i})) {
      if (restaurant) {
        restaurant->start_of_day(*this);
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

bool GameState::buy_restaurant(Property& property, Player& buyer, Money price) {
  GameDate today = time().current_date();

  // Property purchase is Capital, use transfer directly
  if (buyer.cash_balance() < price) {
    return false;
  }
  buyer.financials().transfer(today, Account::Cash, Account::Property, price,
                              AccountingScope::Capital);

  property.buy(buyer.id());
  // If the buyer was renting, a restaurant already exists
  if (!property.has_restaurant()) {
    create_restaurant(buyer, property);
  }
  return true;
}

// When you're renting a property you pay the first month immediately,
// regardless of which day of the month it is. The lease is free
// (at least initial payment) if this is a quick start.
Restaurant* GameState::rent_restaurant(Property& property, Player& player,
                                       bool free) {
  Money price = property.get_monthly_rent();
  GameDate today = time().current_date();

  // Mark property as rented and create restaurant first
  property.rent(player.id());
  auto* restaurant = create_restaurant(player, property);

  if (!restaurant) {
    return nullptr;
  }

  // Pay first month's rent (with restaurant_id for proper accounting)
  // spend_cash is always operational
  if (!free &&
      !player.spend_cash(today, Account::Rents, price, restaurant->id())) {
    // If payment fails, we should probably clean up the restaurant
    // but for now match original behavior
    return nullptr;
  }

  return restaurant;
}

bool GameState::buy_depot(Property& property, Player& buyer) {
  Money price = property.get_purchase_price();
  GameDate today = time().current_date();

  // Property purchase is Capital, use transfer directly
  if (buyer.cash_balance() < price) {
    return false;
  }
  buyer.financials().transfer(today, Account::Cash, Account::Property, price,
                              AccountingScope::Capital);

  property.buy(buyer.id());

  // Add depot capacity to player's central depot in this city
  auto& city = property.city();
  int capacity = property.size().w * property.size().h * 100;
  city.central_depot(buyer.id().value).add_capacity(capacity);

  return true;
}

bool GameState::rent_depot(Property& property, Player& player, bool free) {
  Money price = property.get_monthly_rent();
  GameDate today = time().current_date();

  if (!free && !player.spend_cash(today, Account::Rents, price)) {
    return false;
  }

  property.rent(player.id());

  // Add depot capacity to player's central depot in this city
  auto& city = property.city();
  int capacity = property.size().w * property.size().h;
  city.central_depot(player.id().value).add_capacity(capacity);

  return true;
}

Restaurant* GameState::create_restaurant(Player& player, Property& property) {
  // For now let's assume a random size between 12 and 20:
  int kitchen_w = engine::random::rand_max_n(8) + 12;
  int kitchen_h = engine::random::rand_max_n(8) + 12;
  auto restaurant_id = next_restaurant_id_++;
  auto restaurant_name =
      player.name() + " No. " +
      std::to_string(restaurants_for(player.id()).size() + 1);
  property.restaurant_id(restaurant_id);

  auto restaurant = std::make_unique<Restaurant>(
      property, Size{kitchen_w, kitchen_h}, restaurant_id, restaurant_name);
  restaurants_.push_back(std::move(restaurant));
  property.calculate_demographics(restaurant.get());
  return restaurants_.back().get();
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

void GameState::clear_restaurants_for_load() {
  // Clear all restaurant references from properties in all loaded cities
  for (auto& city_ptr : cities_) {
    if (city_ptr) {
      for (auto* property : city_ptr->properties()) {
        if (property && property->has_restaurant()) {
          // Clear the restaurant link from the property
          property->clear_restaurant();
        }
      }
    }
  }

  // Clear the restaurants list
  restaurants_.clear();
  next_restaurant_id_ = core::RestaurantId{0};
}

bool GameState::sell_property(Property& property, Player& seller,
                              Money sale_price) {
  if (!property.is_owned()) {
    return false;
  }

  auto owner = property.owner_or_tenant();
  if (!owner || *owner != seller.id()) {
    return false;
  }

  GameDate today = time().current_date();

  seller.financials().transfer(today, Account::Property, Account::Cash,
                               sale_price, AccountingScope::Capital);

  // If this is a depot, remove capacity from central depot
  if (property.type() == PropertyType::DEPOT) {
    auto& city = property.city();
    int capacity = property.size().w * property.size().h * 100;
    city.central_depot(seller.id().value).remove_capacity(capacity);
  }

  if (property.has_restaurant()) {
    remove_restaurant(*property.restaurant_id());
  }

  property.clear_owner();
  property.status(PremiseStatus::NOT_AVAILABLE);
  return true;
}

bool GameState::end_property_lease(Property& property, Player& player) {
  if (!property.is_rented()) {
    return false;
  }

  auto owner = property.owner_or_tenant();
  if (!owner || *owner != player.id()) {
    return false;
  }

  // If this is a depot, remove capacity from central depot
  if (property.type() == PropertyType::DEPOT) {
    auto& city = property.city();
    int capacity = property.size().w * property.size().h * 100;
    city.central_depot(player.id().value).remove_capacity(capacity);
  }

  if (property.has_restaurant()) {
    remove_restaurant(*property.restaurant_id());
  }

  property.clear_owner();
  property.status(PremiseStatus::NOT_AVAILABLE);
  return true;
}

const CityViewMapState& GameState::city_view_view_state() const {
  return city_view_view_state_[active_player_.value];
}
void GameState::city_view_view_state(const CityViewMapState cmvs) {
  city_view_view_state_[active_player_.value] = cmvs;
}

RankChange GameState::adjust_underworld_score(Player& player, int delta) {
  auto rc = player.social_status().adjust_underworld(delta, difficulty_level_);
  if (rc.promoted()) {
    // Underworld rank LeventIds run from AsGoodAsDead(15) to Godfather(27)
    auto id = static_cast<core::events::LeventId>(15 + rc.new_rank);
    message_manager_.schedule_message(id, player.id(), time_.current_date());
  }
  return rc;
}

RankChange GameState::adjust_social_score(Player& player, int delta) {
  auto rc = player.social_status().adjust_social(delta, difficulty_level_);
  if (rc.promoted()) {
    // Business rank LeventIds run from LaughingStock(1) to StatusTycoon(14)
    auto id = static_cast<core::events::LeventId>(1 + rc.new_rank);
    message_manager_.schedule_message(id, player.id(), time_.current_date());
  }
  return rc;
}

void GameState::initiate_arrest(ArrestReason reason) {
  auto* city = active_city();
  if (!city) {
    return;
  }
  auto police_actors = city->actors(ActorType::POLICE);
  if (police_actors.empty()) {
    return;
  }
  ui_.push<PoliceState>(
      PoliceState{.id = police_actors[0]->id(), .reason = reason});
}

void GameState::check_police_events() {
  auto* player = active_player();
  if (!player || !player->is_human_controlled()) {
    return;
  }

  using core::item::Item;
  auto& inv = player->inventory();
  int contraband = 0;
  for (auto item :
       {Item::FlameThrower, Item::Chemicals, Item::Bazooka, Item::Bomb}) {
    contraband += inv[item];
  }
  if (contraband == 0) {
    return;
  }

  if (!engine::random::rand_percentage_chance(2)) {
    return;
  }

  int evasion = player->character().dexterity();
  if (engine::random::rand_percentage_chance(evasion)) {
    return;
  }

  initiate_arrest(ArrestReason::PosessingWeapons);
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

std::vector<const Restaurant*> GameState::all_restaurants() const {
  std::vector<const Restaurant*> result;
  for (const auto& uptr : restaurants_) {
    if (uptr) {
      result.push_back(uptr.get());
    }
  }
  return result;
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

Restaurant* GameState::create_ai_starting_restaurant(Player& player,
                                                     City& city) {
  constexpr int max_attempts = 8;
  const uint8_t courage = player.character().courage();

  for (int i = 0; i < max_attempts; ++i) {
    if (!engine::random::rand_percentage_chance(courage)) {
      continue;
    }
    const bool big = engine::random::rand_percentage_chance(30);
    const int idx4 = engine::random::rand_max_n(4);
    const int layout_id = big ? ((idx4 * 2) + 1) : ((idx4 * 2) + 2);
    auto* rest = create_restaurant_with_layout(player, city, layout_id, big);
    if (rest) {
      rest->wants_open(true);
    }
    return rest;
  }
  return nullptr;
}

Restaurant* GameState::create_restaurant_with_layout(Player& player, City& city,
                                                     int layout_id,
                                                     bool is_big) {
  // Build the layout filename from the layout_id
  std::string filename = compat::format("q/f{}.pc", layout_id);
  std::unique_ptr<RestaurantLayout> layout = nullptr;
  if (platform::is_amiga()) {
    layout = std::make_unique<RestaurantLayout>(Size{16, 16}, Size{16, 16});
  }
  else {
    layout = RestaurantLayoutFactory::create_layout(filename);
  }

  if (!layout) {
    return nullptr;
  }

  // Get all business premises in the city
  auto properties = city.business_premises();
  if (properties.empty()) {
    return nullptr;
  }

  // Try to find an available property (not already a restaurant) up to 10 times
  Property* available_property = nullptr;
  const int max_attempts = 10;
  for (int attempt = 0; attempt < max_attempts; ++attempt) {
    auto try_index = engine::random::rand_max_n(properties.size());
    auto* candidate = properties.at(try_index);

    // Check if this property is available (no owner/tenant or no restaurant)
    if (!candidate->has_restaurant()) {
      available_property = candidate;
      break;
    }
  }

  // If we couldn't find an available property after 10 attempts, give up
  if (!available_property) {
    return nullptr;
  }

  available_property->size(layout->restaurant_size());

  // Rent the property (first month free for quickstart)
  auto* rest = rent_restaurant(*available_property, player,
                               /*free=*/true);

  if (rest) {
    // Apply financial transactions for furniture and ovens
    player.financials().transfer(
        time().current_date(), Account::Capital, Account::Furniture,
        layout->furniture_value() / 2, AccountingScope::Capital);
    player.financials().transfer(time().current_date(), Account::Capital,
                                 Account::Machines, layout->oven_value() / 2,
                                 AccountingScope::Capital);

    rest->kitchen_size(layout->kitchen_size());
    rest->layout(std::move(layout));

    // AI restaurants need some members initialized from their character stats
    if (!player.is_human_controlled()) {
      rest->init_ai_economy(player);
    }
  }

  return rest;
}

void GameState::restaurant_placement(std::vector<FurnitureBatch> batch) {
  furniture_placement_ = std::move(batch);
}
std::vector<FurnitureBatch>& GameState::restaurant_placement() {
  return furniture_placement_;
}

std::optional<core::RestaurantId>
GameState::next_restaurant(core::PlayerId pid, core::RestaurantId from) const {
  auto list = restaurants_for(pid);
  if (list.empty()) {
    return std::nullopt;
  }

  // find current index; if not found, just take the first
  std::size_t idx = 0;
  bool found = false;
  for (std::size_t i = 0; i < list.size(); ++i) {
    if (list[i]->id() == from) {
      idx = i;
      found = true;
      break;
    }
  }

  const std::size_t next_idx = found ? (idx + 1) % list.size() : 0;
  return list[next_idx]->id();
}

std::optional<core::RestaurantId>
GameState::prev_restaurant(core::PlayerId pid, core::RestaurantId from) const {
  auto list = restaurants_for(pid);
  if (list.empty()) {
    return std::nullopt;
  }

  std::size_t idx = 0;
  bool found = false;
  for (std::size_t i = 0; i < list.size(); ++i) {
    if (list[i]->id() == from) {
      idx = i;
      found = true;
      break;
    }
  }

  const std::size_t prev_idx =
      found ? (idx + list.size() - 1) % list.size() : 0;
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
        PizzaFactory::create_pizza(i, get_bob_size, customer_taste_profiles_);
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

bool GameState::is_refusing_calls(int actor_id) const {
  return actors_refusing_calls_.contains(actor_id);
}

void GameState::clear_all_refused_calls() { actors_refusing_calls_.clear(); }

StatusDisplay* GameState::status_display(GameContext& context) {
  if (!status_display_) {
    status_display_ =
        std::make_unique<StatusDisplay>(context, *this, status_hub_);
  }
  return status_display_.get();
}

void GameState::next_player() {
  if (++active_player_.value >= NUMBER_OF_PLAYERS) {
    active_player_.value = 0;
  }
}

void GameState::on_player_turn_card_finished() {
  // Only unfreeze for human players - keep frozen for AI to hide their money
  if (status_display_ && active_player()->is_human_controlled()) {
    status_display_->unfreeze();
  }

  if (pending_ai_auto_end_turn_) {
    pending_ai_auto_end_turn_ = false;
    end_of_turn();
    return;
  }

  // Human player taking control: unsuppress status so they see updates
  status_hub_.suppress(false);

  auto* player = active_player();
  if (player && player->is_in_jail()) {
    player->decrement_jail_day();
    if (player->is_in_jail()) {
      end_of_turn();
    }
    else {
      // Release day: no bookkeeping, just advance to next player
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
  status_hub_.post_direct(m);
}

void GameState::request_fade_transition(int duration_frames) {
  fade_requested_ = true;
  fade_duration_frames_ = duration_frames;
}

bool GameState::fade_requested() const { return fade_requested_; }

int GameState::fade_duration_frames() const { return fade_duration_frames_; }

void GameState::clear_fade_request() { fade_requested_ = false; }

void GameState::request_resolution_change(ScreenResolution res) {
  pending_resolution_ = res;
}

std::optional<ScreenResolution> GameState::pending_resolution_change() const {
  return pending_resolution_;
}

void GameState::clear_resolution_request() { pending_resolution_.reset(); }

core::food::IngredientDemand
GameState::total_ingredient_need_for_player(core::PlayerId player_id) const {
  core::food::IngredientDemand demand_grams{};

  for (auto* restaurant : restaurants_for(player_id)) {
    if (!restaurant) {
      continue;
    }

    demand_grams += restaurant->calculate_daily_ingredient_demand_grams(
        player(player_id)->recipes());
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

bool GameState::bank_transfer(int account_number, Money amount,
                              Player& sender) {
  auto* account = bank_accounts_.find(account_number);
  if (!account) {
    return false;
  }

  const auto& date = time().current_date();

  // Debit the sender
  if (!sender.spend_cash(date, Account::Others, amount)) {
    return false;
  }

  // Credit the target
  if (account->player_id) {
    auto* target = player(*account->player_id);
    if (target) {
      target->financials().transfer(date, Account::Others, Account::Cash,
                                    amount, AccountingScope::Operational);
    }
  }

  // Track the deposit (for mafia/mayor to query)
  bank_accounts_.deposit(account_number, amount);

  // Mayor side-effects: donations to the private account increase favor;
  // transfers to the public account register payment of the award fee.
  for (auto& city : cities_) {
    if (!city) {
      continue;
    }
    auto& mayor = city->mayor_office();
    if (account_number == mayor.private_bank_account()) {
      mayor.add_favor(sender.id().value, amount);
      break;
    }
    if (account_number == mayor.public_bank_account()) {
      mayor.set_fee_paid(sender.id().value);
      break;
    }
  }

  return true;
}
