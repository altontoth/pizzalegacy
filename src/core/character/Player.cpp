/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/character/Player.h"

#include "core/character/Character.h"
#include "core/city/City.h"
#include "core/city/District.h"
#include "core/city/Property.h"
#include "core/events/LeventId.h"
#include "core/food/IngredientCategory.h"
#include "core/restaurant/AIEconomy.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "core/supply/FoodSupplier.h"
#include "core/time/TimeOfDay.h"
#include "engine/FormatCompat.h"
#include "engine/Logger.h"

#include <algorithm>
#include <iostream>
#include <ranges>
#include <utility>

using namespace core;
using namespace core::character;

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
                       character.starting_money(), AccountingScope::Capital);

  social_status_ = SocialStatus{
      .social_score = (character.personality() / 2) + 220,
      .underworld_score = (character.courage() / 2) + 220,
  };
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
  // threshold = 90 + energy/2 blocks from midnight
  // Full energy (100) -> 140 blocks = 23:20; zero energy -> 90 blocks = 15:00
  int threshold_blocks = 90 + (character_.energy() / 2);
  return core::time::TimeOfDay::from_block(threshold_blocks);
}

void Player::end_of_day(GameState& game_state) {
  // Deduct daily advertising costs for campaigns in active city
  const auto active_city_id = game_state.active_city_id();
  const auto& current_date = game_state.time().current_date();

  for (const auto& campaign_opt : campaigns_) {
    if (campaign_opt.has_value() && campaign_opt->days_remaining > 0 &&
        campaign_opt->city_id == active_city_id) {
      // Deduct daily cost (can go negative)
      Account expense_account = Account::ProfitAndLoss;
      financials_.transfer(current_date, Account::Cash, expense_account,
                           campaign_opt->rate.cost,
                           AccountingScope::Operational);
    }
  }

  // Update advertising campaigns (decrement days_remaining, remove expired)
  update_campaigns();

  // Daily health/energy drain based on start time
  {
    int start_blocks = start_of_day_hour_ * 6;
    int stress = std::max(0, 100 - (2 * start_blocks));
    if (stress > 0) {
      if (engine::random::rand_percentage_chance(stress)) {
        character_.health(
            static_cast<uint8_t>(std::max(0, (int)character_.health() - 2)));
      }
      int energy_rolls = stress / 10;
      for (int i = 0; i < energy_rolls; ++i) {
        if (engine::random::rand_percentage_chance(100 - character_.health())) {
          character_.energy(
              static_cast<uint8_t>(std::max(0, (int)character_.energy() - 1)));
        }
      }
    }
    // Unconditional daily energy drain
    if (engine::random::rand_percentage_chance(character_.age())) {
      character_.energy(
          static_cast<uint8_t>(std::max(0, (int)character_.energy() - 1)));
    }
  }

  // Early day-end energy bonus
  // prob = (end_threshold - current_block) + 10; higher the earlier you end.
  {
    int end_threshold = 90 + (character_.energy() / 2);
    int current_block = game_state.time().current_block_of_day();
    int early_prob = std::clamp(end_threshold - current_block + 10, 0, 100);
    if (engine::random::rand_percentage_chance(early_prob)) {
      character_.energy(
          static_cast<uint8_t>(std::min(100, (int)character_.energy() + 1)));
    }
  }

  // Daily recovery
  {
    int recovery_health_prob = 100 - character_.age();
    for (int i = 0; i < 4; ++i) {
      if (engine::random::rand_percentage_chance(recovery_health_prob)) {
        character_.health(
            static_cast<uint8_t>(std::min(100, (int)character_.health() + 1)));
      }
    }
    for (int i = 0; i < 4; ++i) {
      if (engine::random::rand_percentage_chance(character_.health())) {
        character_.energy(
            static_cast<uint8_t>(std::min(100, (int)character_.energy() + 1)));
      }
    }
  }

  // Process active supplier contracts: deliver ingredients, charge payment,
  // tick term
  process_contracts(game_state);

  // AI daily evaluation
  if (!is_human_controlled()) {
    // Bankruptcy check on first restaurant
    auto restaurants = game_state.restaurants_for(id_);
    if (!restaurants.empty() && restaurants[0] &&
        restaurants[0]->wants_open() && !restaurants[0]->is_force_closed()) {
      ai::check_bankruptcy(*restaurants[0], *this, game_state);
    }

    // AI quality growth chance
    for (auto* rest : restaurants) {
      if (rest && rest->wants_open() && !rest->is_force_closed() &&
          rest->economy().quality_rating() < 100 &&
          engine::random::rand_percentage_chance(character_.courage())) {
        rest->economy().quality_rating(rest->economy().quality_rating() + 1);
      }
    }

    check_ai_expansion(game_state);
    return;
  }

  // From here on only humans
  Money balance = cash_balance();

  // First time going into debt warning
  if (balance.cents < 0) {
    game_state.message_manager().schedule_message(
        core::events::LeventId::FinancialDifficultiesFirstTime, id_,
        game_state.time().current_date());
  }

  // Overdrawn account warning
  if (balance.cents < 0) {
    Logger::warn("[Finance] {} - Account overdrawn: {}", name_, balance);
  }
}

// Pay weekly staff salaries
void Player::end_of_week(GameState& game_state) {
  Money total_wages{0};
  int total_staff_count = 0;

  auto restaurants = game_state.restaurants_for(id_);
  for (auto* restaurant : restaurants) {
    if (!restaurant) {
      continue;
    }

    Money restaurant_wages{0};

    // Count and sum wages for cooks
    for (const auto& cook : restaurant->cooks()) {
      if (cook.has_value()) {
        restaurant_wages += cook->payment();
        total_staff_count++;
      }
    }

    // Count and sum wages for waiters
    for (const auto& waiter : restaurant->waiters()) {
      if (waiter.has_value()) {
        restaurant_wages += waiter->payment();
        total_staff_count++;
      }
    }

    // Count and sum wages for manager
    if (restaurant->manager().has_value()) {
      restaurant_wages += restaurant->manager()->payment();
      total_staff_count++;
    }

    // Transfer wages from Cash to Staff account per restaurant (can go
    // negative)
    if (restaurant_wages.cents > 0) {
      const auto& current_date = game_state.time().current_date();
      financials_.transfer(current_date, Account::Cash, Account::Staff,
                           restaurant_wages, restaurant->id(),
                           AccountingScope::Operational);
    }

    total_wages += restaurant_wages;
  }

  if (total_wages.cents > 0) {
    Logger::info(
        "[Finance] {} - Weekly salary payment: {} staff paid ${:.2f} total",
        name_, total_staff_count, total_wages.cents / 100.0);
  }

  // Supplier debt bankruptcy: any unpaid supplier balance at end of week
  for (City* city : game_state.initialized_cities()) {
    for (const auto& s_ptr : city->suppliers()) {
      if (s_ptr->relationship(id_).outstanding_debt.cents > 0) {
        Logger::warn("[Finance] {} - BANKRUPT! Unpaid supplier debt", name_);
        game_state.deliver_message(
            core::events::LeventId::NotAbleToPaySuppliers,
            [this, &game_state]() {
              game_state.eliminate_player(id_,
                                          GameOverCause::BankruptSuppliers);
            });
        return;
      }
    }
  }

  // Bank/loan debt bankruptcy
  Money balance = cash_balance();
  if (balance.cents < 0) {
    Logger::warn("[Finance] {} - BANKRUPT! Balance: {}", name_, balance);
    game_state.deliver_message(
        core::events::LeventId::NotAbleToPayDebts, [this, &game_state]() {
          game_state.eliminate_player(id_, GameOverCause::BankruptDebts);
        });
  }
}

void Player::jail_time(uint8_t jail_time) { jail_time_ = jail_time; }

int Player::jail_time() const { return jail_time_; }

void Player::add_loan(core::CityId city_id, Loan loan, GameDate date) {
  loans_[city_id].push_back(loan);
  financials_.transfer(date, Account::Loan, Account::Cash, loan.start_amount(),
                       AccountingScope::Operational);
}

bool Player::spend_cash(const GameDate& date, Account to, Money amount,
                        std::optional<core::RestaurantId> restaurant_id) {
  if (cash_balance() >= amount) {
    financials_.spend_cash(date, to, amount, restaurant_id);
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
    total_loans += static_cast<int>(loans.size());
  }
  return total_loans;
}

bool Player::has_loan_with_bank(core::CityId city_id, BankId bank_id) const {
  const auto& loans = loans_in_city(city_id);
  return std::ranges::any_of(
      loans, [bank_id](const Loan& loan) { return loan.bank_id() == bank_id; });
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

void Player::apply_monthly_investment_payouts(const GameDate& date,
                                              bool fix_legacy_bugs) {
  for (auto& slot : investments_) {
    if (!slot.has_value()) {
      continue;
    }
    if (slot->is_matured()) {
      // Legacy: principal returned the month after the last interest payment
      financials_.transfer(date, Account::Investments, Account::Cash,
                           slot->amount(), AccountingScope::Operational);
      Logger::info("[Finance] {} - Investment matured, principal returned: {}",
                   name_, slot->amount());
      slot.reset();
    }
    else {
      financials_.transfer(date, Account::Investments, Account::Cash,
                           slot->monthly_return(),
                           AccountingScope::Operational);
      slot->end_of_month();
      // Fixed: if this was the final month, pay principal now too
      if (fix_legacy_bugs && slot->is_matured()) {
        financials_.transfer(date, Account::Investments, Account::Cash,
                             slot->amount(), AccountingScope::Operational);
        Logger::info(
            "[Finance] {} - Investment matured, principal returned: {}", name_,
            slot->amount());
        slot.reset();
      }
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

Money Player::cash_balance() const {
  return financials_.balance(Account::Cash);
}

core::inventory::Inventory& Player::inventory() { return inventory_; }

const core::inventory::Inventory& Player::inventory() const {
  return inventory_;
}

void Player::add_campaign(const AdvertisingCampaign& campaign) {
  int channel_index = static_cast<int>(campaign.channel);
  campaigns_[channel_index] = campaign;
}

std::optional<AdvertisingCampaign>
Player::get_campaign(AdvertisingChannel channel) const {
  int channel_index = static_cast<int>(channel);
  return campaigns_[channel_index];
}

bool Player::has_active_campaign(AdvertisingChannel channel) const {
  int channel_index = static_cast<int>(channel);
  return campaigns_[channel_index].has_value() &&
         campaigns_[channel_index]->days_remaining > 0;
}

void Player::update_campaigns() {
  for (auto& campaign_opt : campaigns_) {
    if (campaign_opt.has_value()) {
      campaign_opt->days_remaining--;
      if (campaign_opt->days_remaining <= 0) {
        campaign_opt.reset(); // Remove expired campaign
      }
    }
  }
}

uint8_t Player::total_advertising_effectiveness() const {
  int total = 0;
  for (const auto& campaign_opt : campaigns_) {
    if (campaign_opt.has_value() && campaign_opt->days_remaining > 0) {
      total += campaign_opt->rate.effectiveness;
    }
  }
  return static_cast<uint8_t>(std::min(total, 255));
}

int Player::negotiation_skill() const {
  return (character_.charisma() + character_.intelligence()) / 2;
}

Player::MonthlyLoanCosts Player::monthly_loan_costs() const {
  MonthlyLoanCosts result{};
  for (const auto& [city_id, loans] : loans_) {
    for (const auto& loan : loans) {
      if (!loan.is_settled()) {
        result.repayment += loan.monthly_principal();
        result.interest += loan.monthly_interest();
      }
    }
  }
  return result;
}

void Player::apply_monthly_loan_payments(const GameDate& date) {
  for (auto& [city_id, loans] : loans_) {
    for (auto& loan : loans) {
      if (!loan.is_settled()) {
        financials_.transfer(date, Account::Cash, Account::Repayment,
                             loan.monthly_repayment(),
                             AccountingScope::Operational);
        loan.end_of_month();
      }
    }
  }
}

void Player::on_food_shop_visit() {
  for (int i = 0; i < 2; ++i) {
    if (engine::random::rand_percentage_chance(character_.age())) {
      character_.energy(
          static_cast<uint8_t>(std::max(0, (int)character_.energy() - 1)));
    }
  }
}

void Player::on_city_travel() {
  int health = character_.health();
  for (int i = 0; i < 6; ++i) {
    if (engine::random::rand_percentage_chance(100 - health)) {
      character_.energy(
          static_cast<uint8_t>(std::max(0, (int)character_.energy() - 1)));
    }
  }
}

int Player::criminal_record() const { return criminal_record_; }

void Player::increment_criminal_record() { criminal_record_++; }

void Player::jail(int days) {
  jail_days_left_ = std::min(days, 99);
  jail_time_ = *jail_days_left_;
  status_ = PlayerStatus::InPrison;
}

bool Player::is_in_jail() const { return status_ == PlayerStatus::InPrison; }

std::optional<int> Player::jail_days_left() const { return jail_days_left_; }

void Player::decrement_jail_day() {
  Logger::debug("Player {} decrement jail time", name());
  if (jail_days_left_) {
    --(*jail_days_left_);
    if (*jail_days_left_ <= 0) {
      jail_days_left_.reset();
      if (status_ == PlayerStatus::InPrison) {
        status_ = PlayerStatus::Active;
      }
    }
  }
}

void Player::decrement_holiday_day() {
  if (holiday_days_left_) {
    --(*holiday_days_left_);
    if (*holiday_days_left_ <= 0) {
      holiday_days_left_.reset();
    }
  }
}

void Player::add_score(int points) {
  score_ += points;
  Logger::info("[Score] {} - Score changed by {}, new total: {}", name_, points,
               score_);
}

const std::array<Money, 52>& Player::liquidity_snapshots() {
  return liquidity_snapshots_;
}
void Player::add_liquidity_snapshot(Money amount, int week_nr) {
  assert(week_nr > 0 && week_nr < 52);
  liquidity_snapshots_[week_nr] = amount;
}

void Player::check_ai_expansion(GameState& game_state) {
  ai_expansion_cooldown_--;
  if (ai_expansion_cooldown_ > 0) {
    return;
  }

  auto restaurants = game_state.restaurants_for(id_);
  int count = static_cast<int>(restaurants.size());

  if (count >= 20) {
    ai_expansion_cooldown_ = 30;
    return;
  }

  if (count > 10) {
    int human_count = game_state.human_player_count();
    if (human_count > 0) {
      auto all_players = game_state.players();
      int random_idx = engine::random::rand_max_n(human_count);
      auto* other = all_players[random_idx];
      if (other) {
        int other_count =
            static_cast<int>(game_state.restaurants_for(other->id()).size());
        if (count - other_count >= 3) {
          ai_expansion_cooldown_ = 10;
          return;
        }
      }
    }
  }

  uint8_t courage = character_.courage();

  // Double courage/2 gate (aggressive AI = higher chance)
  if (!engine::random::rand_percentage_chance(courage / 2)) {
    ai_expansion_cooldown_ = (100 - courage) / 2;
    return;
  }
  if (!engine::random::rand_percentage_chance(courage / 2)) {
    ai_expansion_cooldown_ = (100 - courage) / 2;
    return;
  }

  // Need enough cash: at least 2000 cents ($20)
  if (cash_balance() < 20_usd) {
    ai_expansion_cooldown_ = 5;
    return;
  }

  auto* city = game_state.active_city();
  if (!city) {
    ai_expansion_cooldown_ = 5;
    return;
  }

  bool big = engine::random::rand_percentage_chance(30);
  const int idx4 = engine::random::rand_max_n(4);
  int layout_id = big ? ((idx4 * 2) + 1) : ((idx4 * 2) + 2);
  auto* rest =
      game_state.create_restaurant_with_layout(*this, *city, layout_id, big);

  if (rest) {
    rest->wants_open(true);
    std::string district_name = "unknown";
    district_name = rest->property().district().name();
    Logger::info("[AI] {} opens a new restaurant in {}", name_, district_name);
    game_state.status().post(StatusMsg{
        .payload = compat::format("{} opens a new restaurant in {}!", name_,
                                  district_name),
        .scope = StatusScope::City,
    });
  }

  ai_expansion_cooldown_ = std::max(1, (100 - courage) / 2);
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

void Player::remove_contract(size_t index) {
  if (index < contracts_.size()) {
    contracts_.erase(contracts_.begin() + static_cast<ptrdiff_t>(index));
  }
}

void Player::process_contracts(GameState& game_state) {
  using namespace core::food;
  using namespace engine;

  for (int i = static_cast<int>(contracts_.size()) - 1; i >= 0; --i) {
    auto& c = contracts_[static_cast<size_t>(i)];

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
    int difficulty = game_state.difficulty_level();
    static constexpr float difficulty_multipliers[] = {0.6f, 0.8f, 1.0f, 1.5f,
                                                       1.9f};
    float diff_mult = difficulty_multipliers[std::clamp(difficulty - 1, 0, 4)];

    for (const auto& item : c.items) {
      Money price = supplier->calculate_price(
          item.type, city->ingredient_base_price(item.type), diff_mult);
      restaurant->purchase_ingredients(item.type, item.kg_per_day,
                                       supplier->quality(), price);
    }

    // deliver_as_required: top up to 2x daily demand beyond ordered amount
    Money extra_cost{0};
    if (c.deliver_as_required) {
      auto demand =
          restaurant->calculate_daily_ingredient_demand_grams(recipes_);
      for (const auto& item : c.items) {
        int daily_grams = demand[item.type];
        int current_grams =
            restaurant->inventory().get_stock(item.type).quantity;
        int cap_grams = 2 * daily_grams;
        int headroom_grams = cap_grams - current_grams;
        if (headroom_grams > 1000) {
          int extra_kg = headroom_grams / 1000;
          Money price = supplier->calculate_price(
              item.type, city->ingredient_base_price(item.type), diff_mult);
          restaurant->purchase_ingredients(item.type, extra_kg,
                                           supplier->quality(), price);
          extra_cost += price * extra_kg;
        }
      }
    }

    // Charge payment: sum, plus charge if they delivered extra
    Money payment = c.daily_sum * (100 - c.discount_pct) / 100;
    if (c.deliver_as_required && extra_cost.cents > 0) {
      payment += c.charge;
    }

    if (!game_state.spend_cash(Account::Stock, payment)) {
      rel.outstanding_debt += payment;
      rel.missed_payment_penalty =
          static_cast<uint8_t>(std::max(1, supplier->spending_rank(id_)));
      // Ingredients delivered but not paid: record the expense against
      // DeliveryDebts so it shows on the balance sheet.
      financials_.transfer(game_state.time().current_date(),
                           Account::DeliveryDebts, Account::Stock, payment,
                           AccountingScope::Operational);
      game_state.status().post(StatusMsg{
          .payload = compat::format(
              "{}: Delivery from {}: because of insolvency the amount will be "
              "credited to the supplier.",
              rest_name, supplier->name()),
          .severity = StatusSeverity::Warning,
      });
    }
    else {
      game_state.status().post(StatusMsg{
          .payload = compat::format("{}: Delivery from {} arrived.", rest_name,
                                    supplier->name()),
          .severity = StatusSeverity::Info,
      });
    }

    rel.total_spent += c.daily_sum;
    if (rel.delivery_issues_flag > 0) {
      rel.delivery_issues_flag--;
    }

    c.remaining_term--;
  }

  // Daily retry for outstanding supplier debts; also decay
  // missed_payment_penalty
  for (City* city : game_state.initialized_cities()) {
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
