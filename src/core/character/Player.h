/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <unordered_set>
#include <vector>
#include "core/character/AiMenuSlot.h"
#include "core/character/Awards.h"
#include "core/character/Character.h"
#include "core/character/InsurancePlayerState.h"
#include "core/character/MafiaPlayerState.h"
#include "core/character/SocialStatus.h"
#include "core/common/Ids.h"
#include "core/finance/Investment.h"
#include "core/finance/Loan.h"
#include "core/finance/PlayerFinancials.h"
#include "core/food/PlayerRecipes.h"
#include "core/inventory/Inventory.h"
#include "core/item/Item.h"
#include "core/marketing/AdvertisingCampaign.h"
#include "core/state/EndOfDayCause.h"
#include "core/status/StatusHub.h"
#include "core/supply/ActiveContract.h"
#include "core/time/TimeOfDay.h"
#include "core/util/Constants.h"

enum class PlayerStatus { Active, InPrison, Eliminated };

inline constexpr int ai_initial_expansion_cooldown = 3;

class Player {
  friend std::ostream& operator<<(std::ostream& os, const Player& player);

public:
  static constexpr int max_investment_count = 4;
  Player(core::PlayerId pid, core::character::Character character,
         bool is_human = true);
  ~Player();
  core::PlayerId id() const;
  std::string name() const;
  void name(std::string name);
  void character(core::character::Character character);
  core::time::TimeOfDay start_of_day_time() const;
  core::time::TimeOfDay end_of_day_time() const;

  // An early start costs health and energy, more the earlier it is.
  void roll_early_start_drain();

  // Determined each morning from the player's energy.
  void determine_end_of_day();
  int end_of_day_block() const { return end_of_day_block_; }
  void end_of_day_block(int blocks) { end_of_day_block_ = blocks; }

  static constexpr uint8_t default_start_of_day_hour = 9;
  uint8_t start_of_day_hour() const { return start_of_day_hour_; }
  void start_of_day_hour(uint8_t h) { start_of_day_hour_ = h; }
  bool is_human_controlled() const;
  int contract_count_for_city(core::CityId city) const;

  void add_contract(ActiveContract contract);
  const std::vector<ActiveContract>& contracts() const { return contracts_; }
  std::vector<ActiveContract>& contracts() { return contracts_; }
  void remove_contract(size_t index);
  int recipe_count() const {
    return static_cast<int>(recipes_.recipe_count());
  };
  int jail_time() const;
  void jail_time(uint8_t jail_time);
  int criminal_record() const;
  void increment_criminal_record();
  void jail(int days);
  bool is_in_jail() const;
  std::optional<int> jail_days_left() const;
  void decrement_jail_day();
  PlayerStatus status() const { return status_; }
  void eliminate() { status_ = PlayerStatus::Eliminated; }
  void restore_active() { status_ = PlayerStatus::Active; }
  void end_of_day(class GameState& game_state, EndOfDayCause cause);

  // Runs once the AI's restaurants have had their day.
  void ai_end_of_day(class GameState& game_state);
  void end_of_week(class GameState& game_state);
  void on_supplier_visit();
  void on_city_travel();
  void add_loan(core::CityId city_id, Loan loan, GameDate date);
  std::vector<Loan>& loans_in_city(core::CityId city_id);
  const std::vector<Loan>& loans_in_city(core::CityId city_id) const;
  const std::map<core::CityId, std::vector<Loan>>& loans() const {
    return loans_;
  }
  int number_of_loans() const;
  bool has_loan_with_bank(core::CityId city_id, core::BankId bank_id) const;
  bool add_investment(Investment investment);
  const std::array<std::optional<Investment>, max_investment_count>&
  investments() const;
  int number_of_investments() const;
  void apply_monthly_investment_payouts(const GameDate& date,
                                        bool fix_legacy_bugs);
  Money monthly_investment_returns() const;
  core::character::Character& character() { return character_; }
  const core::character::Character& character() const { return character_; }
  PlayerFinancials& financials() { return financials_; }
  const PlayerFinancials& financials() const { return financials_; }

  [[nodiscard]] bool
  spend_cash(const GameDate& date, Account to, Money amount,
             std::optional<core::RestaurantId> restaurant_id = std::nullopt);
  Money cash_balance() const;
  core::inventory::Inventory& inventory();
  const core::inventory::Inventory& inventory() const;

  // The average price paid for a weapon type.
  Money weapon_unit_cost(core::item::Item id) const;
  void weapon_unit_cost(core::item::Item id, Money unit_cost);

  // Buy weapons at a unit price and return how many the player was able to
  // carry. The average price is truncated to whole dollars, like the original.
  int buy_weapons(const GameDate& date, core::item::Item id, int qty,
                  Money price_per);

  void use_weapon(const GameDate& date, core::item::Item id);
  core::food::PlayerRecipes& recipes() { return recipes_; };
  const core::food::PlayerRecipes& recipes() const { return recipes_; };

  // Advertising campaigns
  using CityCampaigns = std::array<std::optional<AdvertisingCampaign>, 4>;
  void add_campaign(const AdvertisingCampaign& campaign);
  std::optional<AdvertisingCampaign>
  get_campaign(core::CityId city, AdvertisingChannel channel) const;
  bool has_active_campaign(core::CityId city, AdvertisingChannel channel) const;
  void update_campaigns(core::CityId city);
  int advertising_effectiveness_sum(core::CityId city) const;
  // A channel slowly decays after its campaign ends.
  const std::map<core::CityId, std::array<uint8_t, 4>>&
  ad_effectiveness() const {
    return ad_effectiveness_;
  }
  void ad_effectiveness(core::CityId city, const std::array<uint8_t, 4>& eff);

  // Cumulative gross turnover (sales revenue) per city, shown on the competitor
  // "turnover in $" chart. Accumulated for both human and AI restaurants,
  // because the ledger tracks human receipts only.
  Money turnover_in_city(core::CityId city) const {
    return turnover_by_city_.at(static_cast<std::size_t>(city.value));
  }
  void add_turnover(core::CityId city, Money amount) {
    turnover_by_city_.at(static_cast<std::size_t>(city.value)) += amount;
  }
  void set_turnover_in_city(core::CityId city, Money amount) {
    turnover_by_city_.at(static_cast<std::size_t>(city.value)) = amount;
  }
  const std::map<core::CityId, CityCampaigns>& get_campaigns() const {
    return campaigns_;
  }

  std::map<core::CityId, CityCampaigns>& get_campaigns() { return campaigns_; }
  const Awards& awards() const;
  void award(AwardCategory, AwardLevel);

  void ai_expansion_cooldown(int days) { ai_expansion_cooldown_ = days; }
  int ai_expansion_cooldown() const { return ai_expansion_cooldown_; }

  using MenuSlots = std::array<AiMenuSlot, ai_menu_slot_count>;
  MenuSlots& ai_menu_slots() { return ai_menu_slots_; }
  const MenuSlots& ai_menu_slots() const { return ai_menu_slots_; }

  int negotiation_skill() const;

  MafiaPlayerState& mafia_state() { return mafia_state_; }
  const MafiaPlayerState& mafia_state() const { return mafia_state_; }
  InsurancePlayerState& insurance_state() { return insurance_state_; }
  const InsurancePlayerState& insurance_state() const {
    return insurance_state_;
  }
  void make_insurance_claim(InsuranceType type) {
    insurance_state_.pending_claim = type;
  }
  SocialStatus& social_status() { return social_status_; }
  const SocialStatus& social_status() const { return social_status_; }
  Money weapon_dealing_profit() const { return weapon_dealing_profit_; }
  void weapon_dealing_profit(Money amount) { weapon_dealing_profit_ = amount; }
  void add_weapon_dealing_profit(Money amount) {
    weapon_dealing_profit_ += amount;
  }

  void mark_city_visited(core::CityId city_id) {
    visited_city_ids_.insert(city_id);
  }
  const std::unordered_set<core::CityId>& visited_city_ids() const {
    return visited_city_ids_;
  }

  struct MonthlyLoanCosts {
    Money repayment;
    Money interest;
  };
  MonthlyLoanCosts monthly_loan_costs() const;
  void apply_monthly_loan_payments(const GameDate& date);

  int score() const { return score_; }
  void add_score(int points);

  uint8_t competition_willingness() const { return competition_willingness_; }
  void set_competition_willingness(uint8_t v) { competition_willingness_ = v; }
  void decay_competition_willingness() {
    if (competition_willingness_ > 0)
      competition_willingness_--;
  }
  uint8_t competition_win_count() const { return competition_win_count_; }
  void competition_win_count(uint8_t count) { competition_win_count_ = count; }
  void increment_competition_win_count() { competition_win_count_++; }
  void holiday(int days) { holiday_days_left_ = days; }
  bool is_on_holiday() const {
    return holiday_days_left_.has_value() && *holiday_days_left_ > 0;
  }
  std::optional<int> holiday_days_left() const { return holiday_days_left_; }
  void decrement_holiday_day();

  // Pending notifications to be shown at the start of this player's next turn.
  void queue_notification(StatusMsg m);
  std::vector<StatusMsg> take_pending_notifications();
  const std::vector<StatusMsg>& pending_notifications() const {
    return pending_notifications_;
  }

private:
  core::PlayerId id_;
  core::character::Character character_;
  PlayerFinancials financials_;
  PlayerStatus status_{PlayerStatus::Active};
  int jail_time_{0};
  int criminal_record_{0};
  std::optional<int> jail_days_left_;
  std::string name_;
  std::map<core::CityId, std::vector<Loan>> loans_;
  std::array<std::optional<Investment>, max_investment_count> investments_;
  core::inventory::Inventory inventory_;
  core::food::PlayerRecipes recipes_;
  bool is_human_{true};
  int score_{0};
  Awards awards_{};
  std::optional<int> holiday_days_left_;
  uint8_t start_of_day_hour_{default_start_of_day_hour};
  int end_of_day_block_{0};

  // days until next restaurant expansion check
  int ai_expansion_cooldown_{ai_initial_expansion_cooldown};
  // Pizza slots with production history
  MenuSlots ai_menu_slots_{};
  void check_ai_expansion(class GameState& game_state);
  void end_of_day_human(class GameState& game_state, EndOfDayCause cause);

  void roll_early_day_end_bonus(int current_block);
  void process_contracts(class GameState& game_state);

  std::map<core::CityId, CityCampaigns> campaigns_;
  // Live per-channel advertising effectiveness (indexed by AdvertisingChannel),
  // per city. Starts at campaign's rate when it starts, then decays after the
  // campaign expires.
  std::map<core::CityId, std::array<uint8_t, 4>> ad_effectiveness_;
  std::array<Money, NUMBER_OF_CITIES> turnover_by_city_{};
  MafiaPlayerState mafia_state_;
  InsurancePlayerState insurance_state_;
  SocialStatus social_status_;
  Money weapon_dealing_profit_{0};
  std::array<Money, core::item::item_count> weapon_unit_cost_{};
  std::unordered_set<core::CityId> visited_city_ids_;
  uint8_t competition_willingness_{0};
  uint8_t competition_win_count_{0};
  std::vector<ActiveContract> contracts_;
  std::vector<StatusMsg> pending_notifications_;
};
