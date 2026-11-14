/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/character/Player.h"

#include "core/character/Character.h"
#include "core/character/InsurancePlayerState.h"
#include "core/city/City.h"
#include "core/city/CityTypes.h"
#include "core/city/District.h"
#include "core/city/MayorOffice.h"
#include "core/city/Property.h"
#include "core/events/MilestoneId.h"
#include "core/food/IngredientCategory.h"
#include "core/restaurant/AIEconomy.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"
#include "core/state/RealEstate.h"
#include "core/state/UIState.h"
#include "core/supply/FoodSupplier.h"
#include "core/time/TimeOfDay.h"
#include "engine/FormatCompat.h"
#include "engine/Logger.h"
#include "engine/Random.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

using namespace core;
using namespace core::character;

namespace {

// How much room an AI leaves between a restaurant it is about to open and any
// existing AI restaurants.
constexpr int ai_restaurant_spacing_tiles = 6;

// Number of health and energy rolls (each) per day off.
constexpr int holiday_recovery_rolls = 4;

constexpr int health_chance_per_energy_roll = 10;

constexpr int supplier_visit_energy_rolls = 2;

constexpr int city_travel_energy_rolls = 6;

// When the day ends for a player without energy.
constexpr int earliest_end_of_day_block = 90;

// Chance of the energy bonus for a day ended at its very end.
constexpr int early_day_end_min_chance = 10;

int end_of_day_block_for_energy(int energy) {
  return earliest_end_of_day_block + energy / 2;
}

bool crowds_an_ai_restaurant(GameState& game_state, const Property& premise) {
  const int x = City::index_to_x(premise.index());
  const int y = City::index_to_y(premise.index());

  for (auto* player : game_state.players()) {
    if (!player || player->is_human_controlled()) {
      continue;
    }
    for (const auto* rest :
         game_state.restaurants_in_city(player->id(), premise.city().id())) {
      const Property& standing = rest->property();
      const int dx = std::abs(City::index_to_x(standing.index()) - x);
      const int dy = std::abs(City::index_to_y(standing.index()) - y);
      if (dx + dy < ai_restaurant_spacing_tiles) {
        return true;
      }
    }
  }

  return false;
}

} // namespace

std::ostream& operator<<(std::ostream& os, const Player& player) {
  os << "Player [id=" << player.id_ << ", name=" << player.name_
     << ", jail_time=" << player.jail_time_
     << ", money=" << player.cash_balance() << ", loans={";

  for (const auto& [city_id, loans] : player.loans_) {
    os << "(city=" << city_id << ": [";
    for (const auto& loan : loans) {
      os << loan;
    }
    os << "])";
  }
  os << "}]\n";
  return os;
}

Player::Player(core::PlayerId id, Character character, bool is_human)
    : id_(id), character_(character), is_human_(is_human) {
  financials_.transfer(game_start_date, Account::Capital, Account::Cash,
                       character.starting_money(), balance_sheet_only);
  financials_.start_liquidity(character.starting_money());

  social_status_.social_score((character.personality() / 2) + 220);
  social_status_.underworld_score((character.courage() / 2) + 220);
  determine_end_of_day();
}

Player::~Player() = default;

core::PlayerId Player::id() const { return id_; }

void Player::name(std::string name) { name_ = std::move(name); }

std::string Player::name() const { return name_; }

bool Player::is_human_controlled() const { return is_human_; }

core::time::TimeOfDay Player::start_of_day_time() const {
  return core::time::TimeOfDay{start_of_day_hour_, 0};
}

core::time::TimeOfDay Player::end_of_day_time() const {
  return core::time::TimeOfDay::from_block(end_of_day_block_);
}

void Player::roll_early_start_drain() {
  const int health_roll_chance =
      std::max(0, 100 - (2 * start_of_day_time().to_block()));
  if (health_roll_chance == 0) {
    return;
  }
  if (engine::random::rand_percentage_chance(health_roll_chance)) {
    character_.health(
        static_cast<uint8_t>(std::max(0, (int)character_.health() - 2)));
  }
  const int energy_rolls = health_roll_chance / health_chance_per_energy_roll;
  for (int i = 0; i < energy_rolls; ++i) {
    if (engine::random::rand_percentage_chance(100 - character_.health())) {
      character_.energy(
          static_cast<uint8_t>(std::max(0, (int)character_.energy() - 1)));
    }
  }
}

void Player::determine_end_of_day() {
  end_of_day_block_ = end_of_day_block_for_energy(character_.energy());
}

void Player::roll_early_day_end_bonus(int current_block) {
  const int chance = std::clamp(
      end_of_day_block_ - current_block + early_day_end_min_chance, 0, 100);
  if (engine::random::rand_percentage_chance(chance)) {
    character_.energy(
        static_cast<uint8_t>(std::min(100, (int)character_.energy() + 1)));
  }
}

void Player::end_of_day(GameState& game_state, EndOfDayCause cause) {
  // Each candidate this player fired gets a charisma-driven daily chance to
  // forgive them and become available again.
  for (City* city : game_state.initialized_cities()) {
    city->run_staff_forgiveness(id_, character_.charisma());
  }

  // The AI's own daily work waits until its restaurants have been processed.
  if (!is_human_controlled()) {
    return;
  }
  end_of_day_human(game_state, cause);
}

void Player::ai_end_of_day(GameState& game_state) {
  ai::run_daily_evaluation(*this, game_state);
  check_ai_expansion(game_state);
}

void Player::end_of_day_human(GameState& game_state, EndOfDayCause cause) {

  // From here on only human players

  // Open a fresh day in every recipe's sales history before this day's
  // restaurants settle into it.
  for (auto* recipe : recipes_.all_mut()) {
    recipe->rotate_daily_sales();
  }

  const auto active_city_id = game_state.active_city_id();
  const auto& current_date = game_state.time().current_date();

  financials_.start_profit_day();
  financials_.count_day_present(active_city_id);

  // Deduct daily advertising costs for campaigns in active city
  if (auto it = campaigns_.find(active_city_id); it != campaigns_.end()) {
    for (const auto& campaign_opt : it->second) {
      if (campaign_opt.has_value() && campaign_opt->days_remaining > 0) {
        // Deduct daily cost (can go negative)
        Account expense_account = Account::ProfitAndLoss;
        financials_.transfer(current_date, Account::Cash, expense_account,
                             campaign_opt->rate.cost, MonthlyCategory::Advertising);
        financials_.add_to_profit_today(-campaign_opt->rate.cost.whole());
      }
    }
  }

  for (const auto& campaign : enhanced_campaigns_) {
    if (campaign.city_id != active_city_id || campaign.days_remaining <= 0) continue;
    financials_.transfer(current_date, Account::Cash, Account::ProfitAndLoss,
                         campaign.rate.cost, campaign.target_restaurant,
                         MonthlyCategory::Advertising);
    financials_.add_to_profit_today(-campaign.rate.cost.whole());
  }

  for (const auto* depot : game_state.real_estate().depots_for(id_)) {
    if (depot->is_rented() && depot->city().id() == active_city_id) {
      financials_.add_to_profit_today(-depot->get_monthly_rent().whole() /
                                      PlayerFinancials::days_per_rent_month);
    }
  }

  // Update advertising campaigns (decrement days_remaining, remove expired)
  update_campaigns(active_city_id);

  if (cause == EndOfDayCause::Clock &&
      engine::random::rand_percentage_chance(character_.age())) {
    character_.energy(
        static_cast<uint8_t>(std::max(0, (int)character_.energy() - 1)));
  }

  if (cause == EndOfDayCause::PlayerAction) {
    roll_early_day_end_bonus(game_state.time().current_block_of_day());
  }

  // A holiday restores health and energy.
  if (is_on_holiday()) {
    int recovery_health_prob = 100 - character_.age();
    for (int i = 0; i < holiday_recovery_rolls; ++i) {
      if (engine::random::rand_percentage_chance(recovery_health_prob)) {
        character_.health(
            static_cast<uint8_t>(std::min(100, (int)character_.health() + 1)));
      }
    }
    for (int i = 0; i < holiday_recovery_rolls; ++i) {
      if (engine::random::rand_percentage_chance(character_.health())) {
        character_.energy(
            static_cast<uint8_t>(std::min(100, (int)character_.energy() + 1)));
      }
    }
  }
  // Being in jail costs health.
  else if (is_in_jail()) {
    if (engine::random::rand_percentage_chance(character_.age())) {
      character_.health(
          static_cast<uint8_t>(std::max(0, (int)character_.health() - 1)));
    }
  }

  // Process active supplier contracts: deliver ingredients, charge payment,
  // tick term
  process_contracts(game_state);

  Money balance = cash_balance();

  if (balance.cents < 0) {
    game_state.record_milestone(
        core::events::MilestoneId::FinancialDifficultiesFirstTime, id_);
    Logger::warn("[Finance] {} - Account overdrawn: {}", name_, balance);
  }
}

// Pay suppliers and check bankruptcy.
void Player::end_of_week(GameState& game_state) {
  if (!is_human_controlled()) {
    return;
  }

  // A claim has to be made within the week.
  insurance_state_.pending_claim.reset();

  for (auto city_id : game_state.simulated_cities_for(id_)) {
    City* city = game_state.city(city_id);
    if (!city) {
      continue;
    }
    for (const auto& s_ptr : city->suppliers()) {
      if (s_ptr->relationship(id_).outstanding_debt.cents > 0) {
        Logger::warn("[Finance] {} - BANKRUPT! Unpaid supplier debt", name_);
        game_state.deliver_milestone(
            core::events::MilestoneId::NotAbleToPaySuppliers,
            [this, &game_state]() {
              game_state.eliminate_player(id_,
                                          GameOverCause::BankruptSuppliers);
            });
        return;
      }
    }
  }
}

void Player::jail_time(uint8_t jail_time) { jail_time_ = jail_time; }

int Player::jail_time() const { return jail_time_; }

void Player::add_loan(core::CityId city_id, Loan loan, GameDate date) {
  loans_[city_id].push_back(loan);
  financials_.transfer(date, Account::Loan, Account::Cash, loan.start_amount(),
                       balance_sheet_only);
}

void Player::repay_loan(Loan& loan, const GameDate& date,
                        bool fix_legacy_bugs) {
  financials_.transfer(date, Account::Cash, Account::Loan,
                       loan.repay_principal(fix_legacy_bugs),
                       balance_sheet_only);
  financials_.transfer(date, Account::Cash, Account::ProfitAndLoss,
                       loan.months_left() * loan.monthly_interest(),
                       balance_sheet_only);
  loan.repay();
}

bool Player::spend_cash(const GameDate& date, MonthlyCategory category,
                        Money amount,
                        std::optional<core::RestaurantId> restaurant_id) {
  if (cash_balance() >= amount) {
    financials_.spend_cash(date, category, amount, restaurant_id);
    return true;
  }
  return false;
}

bool Player::spend_cash(const GameDate& date, Account to, Money amount) {
  if (cash_balance() >= amount) {
    financials_.transfer(date, Account::Cash, to, amount, balance_sheet_only);
    return true;
  }
  return false;
}

std::vector<Loan>& Player::loans_in_city(core::CityId city_id) {
  return loans_[city_id];
}

const std::vector<Loan>& Player::loans_in_city(core::CityId city_id) const {
  static const std::vector<Loan> empty;
  if (loans_.contains(city_id)) {
    return loans_.at(city_id);
  }
  return empty;
}

int Player::number_of_loans() const {
  int total_loans = 0;
  for (const auto& [city_id, loans] : loans_) {
    total_loans += static_cast<int>(std::ranges::count_if(
        loans, [](const Loan& loan) { return !loan.is_settled(); }));
  }
  return total_loans;
}

bool Player::has_loan_with_bank(core::CityId city_id, BankId bank_id) const {
  const auto& loans = loans_in_city(city_id);
  return std::ranges::any_of(loans, [bank_id](const Loan& loan) {
    return !loan.is_settled() && loan.bank_id() == bank_id;
  });
}

bool Player::add_investment(Investment investment) {
  for (auto& slot : investments_) {
    if (!slot.has_value()) {
      slot = investment;
      return true;
    }
  }
  return false;
}

const std::array<std::optional<Investment>, Player::max_investment_count>&
Player::investments() const {
  return investments_;
}

int Player::number_of_investments() const {
  int total = 0;
  for (const auto& slot : investments_) {
    if (slot.has_value()) {
      total++;
    }
  }
  return total;
}

void Player::apply_monthly_investment_payouts(const GameDate& date) {
  for (auto& slot : investments_) {
    if (!slot.has_value()) {
      continue;
    }
    if (slot->is_matured()) {
      // Principal is returned the month after the last interest payment.
      financials_.transfer(date, Account::ProfitAndLoss, Account::Cash,
                           slot->amount(), balance_sheet_only);
      Logger::info("[Finance] {} - Investment matured, principal returned: {}",
                   name_, slot->amount());
      slot.reset();
    }
    else {
      financials_.transfer(date, Account::ProfitAndLoss, Account::Cash,
                           slot->monthly_return(), balance_sheet_only);
      slot->end_of_month();
    }
  }
}

Money Player::monthly_investment_returns() const {
  Money total{0};
  for (const auto& slot : investments_) {
    if (slot.has_value() && !slot->is_matured()) {
      total += slot->monthly_return();
    }
  }
  return total;
}

Money Player::monthly_investment_payout() const {
  Money total{0};
  for (const auto& slot : investments_) {
    if (slot.has_value() && slot->is_matured()) {
      total += slot->amount();
    }
  }
  return total;
}

Money Player::cash_balance() const {
  return financials_.balance(Account::Cash);
}

core::inventory::Inventory& Player::inventory() { return inventory_; }

const core::inventory::Inventory& Player::inventory() const {
  return inventory_;
}

Money Player::weapon_unit_cost(core::item::Item id) const {
  return weapon_unit_cost_[static_cast<std::size_t>(id)];
}

void Player::weapon_unit_cost(core::item::Item id, Money unit_cost) {
  weapon_unit_cost_[static_cast<std::size_t>(id)] = unit_cost;
}

void Player::use_weapon(const GameDate& date, core::item::Item id) {
  if (inventory_.remove(id, 1) == 0) {
    return;
  }
  Money cost = weapon_unit_cost(id);
  if (cost.cents > 0) {
    financials_.transfer(date, Account::Weapons, Account::ProfitAndLoss, cost,
                         balance_sheet_only);
  }
}

int Player::buy_weapons(const GameDate& date, core::item::Item id, int qty,
                        Money price_per) {
  const int added = inventory_.add(id, qty);
  if (added == 0) {
    return 0;
  }
  const Money paid = price_per * added;
  // The caller has already checked the player can afford these.
  (void)spend_cash(date, Account::Weapons, paid);

  const int held = inventory_.count(id);
  auto& unit_cost = weapon_unit_cost_[static_cast<std::size_t>(id)];
  Money total = unit_cost * (held - added) + paid;
  unit_cost = Money{total.cents / held}.truncated_to_dollars();
  return added;
}

void Player::restore_enhanced_campaign(AdvertisingCampaign campaign) {
  std::erase_if(enhanced_campaigns_, [&](const auto& old) {
    return old.city_id == campaign.city_id && old.channel == campaign.channel &&
           old.target_restaurant == campaign.target_restaurant;
  });
  enhanced_campaigns_.push_back(std::move(campaign));
}

std::optional<AdvertisingCampaign> Player::enhanced_campaign(
    core::CityId city, AdvertisingChannel channel,
    std::optional<core::RestaurantId> target) const {
  for (const auto& c : enhanced_campaigns_)
    if (c.city_id == city && c.channel == channel &&
        c.target_restaurant == target && c.days_remaining > 0) return c;
  return std::nullopt;
}

int Player::enhanced_advertising_bonus(core::CityId city,
                                       core::RestaurantId restaurant) const {
  int bonus = 0;
  for (const auto& c : enhanced_campaigns_)
    if (c.city_id == city && c.channel != AdvertisingChannel::POSTERS &&
        (!c.target_restaurant || *c.target_restaurant == restaurant))
      bonus += c.awareness;
  return std::min(100, bonus);
}

int Player::poster_awareness(core::CityId city, core::RestaurantId restaurant) const {
  for (const auto& c : enhanced_campaigns_)
    if (c.city_id == city && c.channel == AdvertisingChannel::POSTERS &&
        c.target_restaurant && *c.target_restaurant == restaurant)
      return c.awareness;
  return 0;
}

void Player::remove_restaurant_advertising(core::RestaurantId restaurant) {
  std::erase_if(enhanced_campaigns_, [&](const auto& c) {
    return c.target_restaurant && *c.target_restaurant == restaurant;
  });
}

void Player::add_campaign(const AdvertisingCampaign& campaign) {
  if (campaign.enhanced) {
    auto c = campaign;
    c.awareness = enhanced_advertising_awareness(c);
    restore_enhanced_campaign(std::move(c));
    return;
  }
  int channel_index = static_cast<int>(campaign.channel);
  campaigns_[campaign.city_id][channel_index] = campaign;
  ad_effectiveness_[campaign.city_id][channel_index] =
      static_cast<uint8_t>(campaign.rate.effectiveness);
}

std::optional<AdvertisingCampaign>
Player::get_campaign(core::CityId city, AdvertisingChannel channel) const {
  auto it = campaigns_.find(city);
  if (it == campaigns_.end()) {
    return std::nullopt;
  }
  return it->second[static_cast<int>(channel)];
}

bool Player::has_active_campaign(core::CityId city,
                                 AdvertisingChannel channel) const {
  auto it = campaigns_.find(city);
  if (it == campaigns_.end()) {
    return false;
  }
  const auto& slot = it->second[static_cast<int>(channel)];
  return slot.has_value() && slot->days_remaining > 0;
}

void Player::update_campaigns(core::CityId city) {
  for (auto& c : enhanced_campaigns_) {
    if (c.city_id != city) continue;
    if (c.days_remaining > 0) {
      --c.days_remaining;
    } else {
      // Local flyers fade fastest; city media leave longer-lived awareness.
      constexpr int decay[] = {5, 3, 2, 1};
      c.awareness = std::max(0, c.awareness - decay[static_cast<int>(c.channel)]);
    }
  }
  std::erase_if(enhanced_campaigns_, [](const auto& c) {
    return c.days_remaining <= 0 && c.awareness <= 0;
  });
  // Campaigns expiring today do not begin losing effectiveness until the next
  // daily update.
  constexpr int decay_per_day = 8;
  auto slots = campaigns_.find(city);
  auto eff_entry = ad_effectiveness_.find(city);

  for (int ch = 0; ch < 4; ++ch) {
    std::optional<AdvertisingCampaign>* campaign =
        slots != campaigns_.end() ? &slots->second[ch] : nullptr;
    bool running = campaign != nullptr && campaign->has_value() &&
                   (*campaign)->days_remaining > 0;
    if (running) {
      (*campaign)->days_remaining--;
      if ((*campaign)->days_remaining <= 0) {
        campaign->reset();
      }
      continue;
    }
    if (eff_entry != ad_effectiveness_.end()) {
      auto& eff = eff_entry->second;
      eff[ch] = static_cast<uint8_t>(std::max(0, eff[ch] - decay_per_day));
    }
  }
}

int Player::advertising_effectiveness_sum(core::CityId city) const {
  auto it = ad_effectiveness_.find(city);
  if (it == ad_effectiveness_.end()) {
    return 0;
  }
  int total = 0;
  for (uint8_t e : it->second) {
    total += e;
  }
  return total;
}

void Player::ad_effectiveness(core::CityId city,
                              const std::array<uint8_t, 4>& eff) {
  ad_effectiveness_[city] = eff;
}

int Player::negotiation_skill() const {
  return ((2 * character_.personality()) + character_.intelligence() +
          character_.charisma()) /
         4;
}

Player::MonthlyLoanCosts
Player::monthly_loan_costs(bool fix_legacy_bugs) const {
  MonthlyLoanCosts result{};
  for (const auto& [city_id, loans] : loans_) {
    for (const auto& loan : loans) {
      if (!loan.is_settled()) {
        result.repayment += loan.principal_due(fix_legacy_bugs);
        result.interest += loan.monthly_interest();
      }
    }
  }
  return result;
}

void Player::apply_monthly_loan_payments(const GameDate& date,
                                         bool fix_legacy_bugs) {
  for (auto& [city_id, loans] : loans_) {
    for (auto& loan : loans) {
      if (!loan.is_settled()) {
        financials_.pay_loan_installment(
            date, loan.principal_due(fix_legacy_bugs), loan.monthly_interest());
        loan.end_of_month();
      }
    }
  }
}

Money Player::monthly_insurance_premiums() const {
  Money total{0};
  for (int i = 0; i < static_cast<int>(insurance_state_.policies.size()); ++i) {
    // The original never charges for Life, and it can never be claimed either.
    if (static_cast<InsuranceType>(i) == InsuranceType::LIFE) {
      continue;
    }
    const auto& policy = insurance_state_.policies[i];
    if (policy.is_active()) {
      total += policy.monthly_premium;
    }
  }
  return total;
}

void Player::apply_monthly_insurance_premiums(const GameDate& date) {
  Money total = monthly_insurance_premiums();
  if (total.cents <= 0) {
    return;
  }
  financials_.transfer(date, Account::Cash, Account::ProfitAndLoss, total,
                       MonthlyCategory::Repayment);
}

void Player::on_supplier_visit() {
  for (int i = 0; i < supplier_visit_energy_rolls; ++i) {
    if (engine::random::rand_percentage_chance(character_.age())) {
      character_.energy(
          static_cast<uint8_t>(std::max(0, (int)character_.energy() - 1)));
    }
  }
}

void Player::on_city_travel(core::CityId destination, int week_day) {
  financials_.arrive_in_city(
      destination, !visited_city_ids_.contains(destination), week_day);
  int health = character_.health();
  for (int i = 0; i < city_travel_energy_rolls; ++i) {
    if (engine::random::rand_percentage_chance(100 - health)) {
      character_.energy(
          static_cast<uint8_t>(std::max(0, (int)character_.energy() - 1)));
    }
  }
}

int Player::criminal_record() const { return criminal_record_; }

void Player::increment_criminal_record() { criminal_record_++; }

void Player::jail(int days) {
  away_days_left_ = std::min(days, 99);
  jail_time_ = away_days_left_;
  status_ = PlayerStatus::InPrison;
}

bool Player::is_in_jail() const { return status_ == PlayerStatus::InPrison; }

void Player::holiday(int days) {
  away_days_left_ = days;
  status_ = PlayerStatus::OnHoliday;
}

void Player::decrement_away_day() {
  Logger::debug("Player {} decrement away time", name());
  --away_days_left_;
  if (away_days_left_ <= 0) {
    away_days_left_ = 0;
    status_ = PlayerStatus::Active;
  }
}

void Player::queue_notification(StatusMsg m) {
  pending_notifications_.push_back(std::move(m));
}

std::vector<StatusMsg> Player::take_pending_notifications() {
  return std::exchange(pending_notifications_, {});
}

void Player::add_score(int points) {
  score_ += points;
  Logger::info("[Score] {} - Score changed by {}, new total: {}", name_, points,
               score_);
}

void Player::check_ai_expansion(GameState& game_state) {
  if (ai_expansion_cooldown_ != 0) {
    ai_expansion_cooldown_--;
  }

  const auto city_id = game_state.active_city_id();
  int count = game_state.restaurant_count_for_player_in_city(id_, city_id);

  if (count > 20) {
    return;
  }

  if (count > 10) {
    int human_count = game_state.human_player_count();
    if (human_count > 0) {
      auto all_players = game_state.players();
      int random_idx = engine::random::rand_max_n(human_count);
      auto* other = all_players[random_idx];
      if (other) {
        int other_count = game_state.restaurant_count_for_player_in_city(
            other->id(), city_id);
        if (count > other_count + 3) {
          return;
        }
      }
    }
  }

  // An AI without restaurants expands immediately.
  if (count != 0 && ai_expansion_cooldown_ != 0) {
    return;
  }

  auto* city = game_state.active_city();
  if (!city) {
    return;
  }

  Property* premise = game_state.real_estate().find_available_premise(*city);
  if (!premise || crowds_an_ai_restaurant(game_state, *premise)) {
    return;
  }

  uint8_t courage = character_.courage();

  if (!engine::random::rand_percentage_chance(courage)) {
    return;
  }

  const bool buying = cash_balance() >= premise->purchase_price();

  // Any premise on the market, even if only for rent, can be bought.
  const bool available = buying
                             ? premise->status() != PremiseStatus::NOT_AVAILABLE
                             : premise->is_for_rent();
  if (!available) {
    return;
  }

  auto* rest = game_state.real_estate().create_ai_restaurant(
      *this, *premise,
      buying ? RealEstate::Tenure::Buy : RealEstate::Tenure::Rent);

  if (!rest) {
    return;
  }

  rest->open_for_business(game_state);
  const std::string district_name = rest->property().district().name();
  const char* verb = buying ? "buys" : "rents";
  Logger::info("[AI] {} {} a new restaurant in {}", name_, verb, district_name);
  game_state.status().post(StatusMsg{
      .payload = compat::format("{} {} a new restaurant in {}!", name_, verb,
                                district_name),
      .scope = StatusScope::City,
  });

  ai_expansion_cooldown_ = (100 - courage) / 2;
}

const Awards& Player::awards() const { return awards_; }

void Player::award(AwardCategory category, AwardLevel level) {
  awards_[static_cast<int>(category)] = level;
}

int Player::contract_count_for_city(core::CityId city_id) const {
  int count = 0;
  for (const auto& c : contracts_) {
    if (c.city_id == city_id) {
      count++;
    }
  }
  return count;
}

void Player::add_contract(ActiveContract contract) {
  contracts_.push_back(std::move(contract));
}

void Player::cancel_contract(GameState& game_state, size_t index) {
  if (index >= contracts_.size()) {
    return;
  }
  const auto& c = contracts_[index];

  FoodSupplier* supplier = nullptr;
  if (City* city = game_state.city(c.city_id)) {
    auto type = core::food::CategoryRegistry::supplier_type(c.category);
    for (auto* s : city->food_suppliers(type)) {
      if (s->id() == c.supplier_id) {
        supplier = s;
        break;
      }
    }
  }

  bool paid = true;
  if (c.original_term > 0 && c.cancellation_penalty.cents > 0) {
    Money fee = (c.cancellation_penalty * c.remaining_term) / c.original_term;
    paid = spend_cash(game_state.time().current_date(), MonthlyCategory::Stock,
                      fee, c.restaurant_id);
    if (!paid && supplier) {
      supplier->relationship(id_).outstanding_debt += fee;
      financials_.transfer(game_state.time().current_date(),
                           Account::DeliveryDebts, Account::Stock, fee,
                           balance_sheet_only);
    }
  }

  if (supplier) {
    game_state.status().post(StatusMsg{
        .payload = compat::format(
            "Contract with {} cancelled.{}", supplier->name(),
            paid ? ""
                 : " Because of insolvency the amount will be credited "
                   "to the supplier's current account."),
        .severity = StatusSeverity::Info,
    });
  }
  contracts_.erase(contracts_.begin() + static_cast<ptrdiff_t>(index));
}

void Player::cancel_restaurant_contracts(GameState& game_state,
                                         core::RestaurantId restaurant_id) {
  for (size_t i = contracts_.size(); i > 0; --i) {
    if (contracts_[i - 1].restaurant_id == restaurant_id) {
      cancel_contract(game_state, i - 1);
    }
  }
}

void Player::process_contracts(GameState& game_state) {
  using namespace core::food;
  using namespace engine;

  const auto here = game_state.simulated_cities_for(id_);

  for (int i = static_cast<int>(contracts_.size()) - 1; i >= 0; --i) {
    auto& c = contracts_[static_cast<size_t>(i)];
    if (std::ranges::find(here, c.city_id) == here.end()) {
      continue;
    }

    // Find the city and supplier
    City* city = game_state.city(c.city_id);
    if (!city) {
      Logger::warn("Contract city {} not found, removing", c.city_id.value);
      contracts_.erase(contracts_.begin() + i);
      continue;
    }

    ActorType supplier_actor_type = CategoryRegistry::supplier_type(c.category);
    FoodSupplier* supplier = nullptr;
    for (const auto* s : city->food_suppliers(supplier_actor_type)) {
      if (s->id() == c.supplier_id) {
        supplier = const_cast<FoodSupplier*>(s);
        break;
      }
    }
    if (!supplier) {
      Logger::warn("Contract supplier {} not found, removing", c.supplier_id);
      contracts_.erase(contracts_.begin() + i);
      continue;
    }

    // Find the restaurant for the name prefix in messages
    Restaurant* restaurant = nullptr;
    for (auto* r : game_state.restaurants_for(id_)) {
      if (r->id() == c.restaurant_id) {
        restaurant = r;
        break;
      }
    }
    if (!restaurant) {
      Logger::warn("Contract restaurant not found, removing contract");
      contracts_.erase(contracts_.begin() + i);
      continue;
    }
    const std::string& rest_name = restaurant->name();

    // Expiry check
    if (c.remaining_term == 0) {
      game_state.status().post(StatusMsg{
          .payload = compat::format("{}: The contract with {} has expired.",
                                    rest_name, supplier->name()),
          .severity = StatusSeverity::Info,
      });
      contracts_.erase(contracts_.begin() + i);
      continue;
    }

    // Delivery miss check
    auto& rel = supplier->relationship(id_);
    if (random::rand_percentage_chance(supplier->chance_of_no_delivery())) {
      rel.delivery_issues_flag = 30;
      game_state.status().post(StatusMsg{
          .payload = compat::format("{}: {} has not delivered today!",
                                    rest_name, supplier->name()),
          .severity = StatusSeverity::Warning,
      });
      c.remaining_term--;
      continue;
    }

    // Deliver the order
    float diff_mult =
        FoodSupplier::price_multiplier(game_state.difficulty_level());

    std::vector<std::pair<core::food::IngredientType, Money>> delivered;
    for (const auto& item : c.items) {
      Money price =
          supplier
              ->calculate_price(
                  item.type, city->ingredient_base_price(item.type), diff_mult)
              .value_or(Money{0});
      Money cost = restaurant->purchase_ingredients(item.type, item.kg_per_day,
                                                    supplier->quality(), price);
      delivered.emplace_back(item.type, cost);
    }

    // deliver_as_required: top up to 2x daily demand beyond ordered amount
    Money extra_cost{0};
    if (c.deliver_as_required) {
      const auto& demand = restaurant->daily_ingredient_demand_cache();
      for (const auto& item : c.items) {
        int daily_grams = demand[item.type];
        int current_grams =
            restaurant->inventory().get_stock(item.type).quantity;
        int cap_grams = 2 * daily_grams;
        int headroom_grams = cap_grams - current_grams;
        if (headroom_grams > 1000) {
          int extra_kg = headroom_grams / 1000;
          Money price =
              supplier
                  ->calculate_price(item.type,
                                    city->ingredient_base_price(item.type),
                                    diff_mult)
                  .value_or(Money{0});
          Money cost = restaurant->purchase_ingredients(
              item.type, extra_kg, supplier->quality(), price);
          delivered.emplace_back(item.type, cost);
          extra_cost += price * extra_kg;
        }
      }
    }

    // Charge payment: sum, plus charge if they delivered extra
    Money payment = c.daily_sum * (100 - c.discount_pct) / 100;
    if (c.deliver_as_required && extra_cost.cents > 0) {
      payment += extra_cost + c.charge;
    }

    if (!game_state.spend_cash(MonthlyCategory::Stock, payment,
                               c.restaurant_id)) {
      rel.outstanding_debt += payment;
      if (game_state.rules().suppliers_remember_missed_payments) {
        rel.missed_payment_penalty =
            static_cast<uint8_t>(std::max(1, supplier->spending_rank(id_)));
      }
      // The original shows an unpaid delivery on the balance sheet, not in
      // the monthly overview.
      financials_.transfer(game_state.time().current_date(),
                           Account::DeliveryDebts, Account::Stock, payment,
                           balance_sheet_only);
      game_state.status().post(StatusMsg{
          .payload = compat::format(
              "{}: Delivery from {}: because of insolvency the amount will be "
              "credited to the supplier.",
              rest_name, supplier->name()),
          .severity = StatusSeverity::Warning,
      });
    }
    else {
      city->mayor_office().add_investment_activity(id_.value, payment);
      for (const auto& [type, cost] : delivered) {
        restaurant->record_ingredient_spend(type, cost);
      }
      game_state.status().post(StatusMsg{
          .payload = compat::format("{}: Delivery from {} arrived.", rest_name,
                                    supplier->name()),
          .severity = StatusSeverity::Info,
      });
    }

    rel.total_spent += c.daily_sum;

    c.remaining_term--;
  }

  // Daily retry for outstanding supplier debts; also decay
  // missed_payment_penalty
  for (auto city_id : here) {
    City* city = game_state.city(city_id);
    if (!city) {
      continue;
    }
    for (const auto& s_ptr : city->suppliers()) {
      auto& rel = s_ptr->relationship(id_);
      if (rel.missed_payment_penalty > 0) {
        rel.missed_payment_penalty--;
      }
      if (rel.outstanding_debt.cents <= 0) {
        continue;
      }
      Money debt = rel.outstanding_debt;
      if (game_state.spend_cash(Account::DeliveryDebts, debt)) {
        rel.outstanding_debt = Money{0};
        game_state.status().post(StatusMsg{
            .payload =
                compat::format("Your debts of {} have now been paid.", debt),
            .severity = StatusSeverity::Info,
        });
      }
      else {
        game_state.status().post(StatusMsg{
            .payload = compat::format(
                "This is {}! You still owe me {}!!! If you do not pay this "
                "amount by the end of this week, I will sue you!",
                s_ptr->name(), debt),
            .severity = StatusSeverity::Warning,
        });
      }
    }
  }
}
