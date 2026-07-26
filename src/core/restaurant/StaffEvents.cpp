/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/restaurant/StaffEvents.h"
#include "core/character/Player.h"
#include "core/character/Staff.h"
#include "core/city/Property.h"
#include "core/finance/Ledger.h"
#include "core/finance/PlayerFinancials.h"
#include "core/food/IngredientType.h"
#include "core/restaurant/Furniture.h"
#include "core/restaurant/ManagerFeedback.h"
#include "core/restaurant/PlacedFurniture.h"
#include "core/restaurant/Restaurant.h"
#include "core/restaurant/RestaurantInventory.h"
#include "core/restaurant/RestaurantLayout.h"
#include "core/restaurant/StaffEventDialogs.h"
#include "core/state/GameState.h"
#include "core/status/StatusHub.h"
#include "engine/FormatCompat.h"
#include "engine/Logger.h"
#include "engine/Math.h"
#include "engine/Random.h"
#include "factories/FurnitureFactory.h"

using namespace core::restaurant;
using core::character::JobRole;
using engine::random::coin_flip;
using engine::random::rand_between;
using engine::random::rand_max_n;
using engine::random::rand_percentage_chance;

namespace {

// Calculates the staff member's fair wage based on dominant skill, which
// informs whether or not they lose motivation or not.
int calculate_fair_salary(const Staff& staff) {
  const auto& c = staff.character();
  int cook = c.cooking();
  int waiter = c.serving();
  int mgmt = c.management();

  int base;
  if (cook >= waiter && cook >= mgmt) {
    base = cook;
  }
  else if (waiter >= mgmt) {
    base = waiter + mgmt;
  }
  else {
    base = mgmt * 6 + waiter;
  }

  base += c.pizzas_per_day();
  base += c.trustworthiness();

  int pay = (base * (100 + c.intelligence() / 2)) / 100;
  return math::round_up_to_next_power10<1>(pay);
}
} // namespace

void StaffEvents::daily_processing() {
  if (!restaurant_->wants_open() || restaurant_->is_force_closed()) {
    return;
  }

  auto& cooks = restaurant_->cooks();
  auto& waiters = restaurant_->waiters();
  auto& manager = restaurant_->manager();

  for (int i = 0; i < static_cast<int>(cooks.size()); ++i) {
    if (cooks[i]) {
      process_staff_member(*cooks[i], JobRole::CHEF, i);
    }
  }
  for (int i = 0; i < static_cast<int>(waiters.size()); ++i) {
    if (waiters[i]) {
      process_staff_member(*waiters[i], JobRole::WAITER, i);
    }
  }
  if (manager) {
    process_staff_member(*manager, JobRole::MANAGER, 0);
  }
}

void StaffEvents::apply_event_effect(StaffEventData::Type type, Staff& staff,
                                     Restaurant& restaurant,
                                     GameState& game_state) {
  using Type = StaffEventData::Type;
  using IT = core::food::IngredientType;

  switch (type) {
  case Type::Theft: {
    int theft_cents =
        rand_max_n(static_cast<int>(staff.payment().whole()) * 10) * 100;
    Money theft_amount{theft_cents};
    if (auto* player = game_state.restaurant_owner(restaurant)) {
      player->financials().spend_cash(game_state.time().current_date(),
                                      Account::Others, theft_amount,
                                      restaurant.id());
      player->make_insurance_claim(InsuranceType::THEFT);
    }
    staff.stole_money(true);
    staff.adjust_motivation(-30);
    game_state.status().post(StatusMsg{
        .payload = compat::format("{}: Some money is missing from your till.",
                                  restaurant.name()),
        .severity = StatusSeverity::Warning,
        .scope = StatusScope::Restaurant,
    });
    game_state.status().post(StatusMsg{
        .payload =
            compat::format("{}: {} {} is driving round in a new sports car.",
                           restaurant.name(), staff.character().first_name(),
                           staff.character().last_name()),
        .severity = StatusSeverity::Warning,
        .scope = StatusScope::Restaurant,
        .delay_blocks = 6,
    });
    break;
  }
  case Type::DestroyCheeseStock: {
    // Zero every cheese stock
    auto& inv = restaurant.inventory();
    for (auto t = static_cast<int>(IT::MOZZARELLA);
         t <= static_cast<int>(IT::GOATS_CHEESE); ++t) {
      inv.get_stock(static_cast<IT>(t)).quantity = 0;
    }
    break;
  }
  case Type::BlowUpOven: {
    auto* layout = restaurant.layout().get();
    if (!layout) {
      break;
    }
    std::vector<core::FurnitureId> oven_ids;
    for (const auto& pf : layout->placed_kitchen_furniture()) {
      if (pf && pf->furniture()->category() == FurnitureCategory::KITCHENWARE) {
        oven_ids.push_back(pf->id());
      }
    }
    if (oven_ids.empty()) {
      break;
    }
    auto chosen_id = oven_ids[rand_max_n(static_cast<int>(oven_ids.size()))];
    auto removed = layout->remove_by_id(chosen_id, true);
    const auto* ash_furn =
        FurnitureFactory::create_furniture(FurnitureType::ASHES_LARGE);
    if (removed && ash_furn) {
      auto ash = std::make_unique<PlacedFurniture>(
          ash_furn, layout->next_furniture_id(), removed->origin(),
          core::FurnitureId{UNLINKED_FURNITURE}, 0);
      layout->place_kitchen_furniture(std::move(ash));
      if (auto* player = game_state.restaurant_owner(restaurant)) {
        player->make_insurance_claim(InsuranceType::FIRE);
      }
    }
    break;
  }
  case Type::SpillWine: {
    if (auto* player = game_state.restaurant_owner(restaurant)) {
      player->financials().spend_cash(game_state.time().current_date(),
                                      Account::Others, Money{100000},
                                      restaurant.id());
    }
    break;
  }
  case Type::MessedUpOrders:
    restaurant.add_reputation_damage(-30, 10);
    break;
  case Type::ForgotToOpen:
    restaurant.force_close(1);
    break;
  default:
    break;
  }
}

std::optional<StaffEvents::Outcome>
StaffEvents::apply_outcome(const StaffEventData& event, int outcome,
                           Restaurant& restaurant) {
  using Type = StaffEventData::Type;

  Staff* s = restaurant.staff_at(event.job_role, event.slot_index);
  if (!s) {
    Logger::warn("Staff is a nullptr in event handling, ignoring.");
    return std::nullopt;
  }

  Outcome result;

  if (event.type == Type::Quit) {
    if (outcome == 0) {
      // fire immediately, no reply popup.
      restaurant.fire_staff(event.job_role, event.slot_index);
      result.staff_left = true;
      return result;
    }
    // Apply salary raise, then check if they decide to stay.
    Money new_pay = (outcome == 1) ? s->payment() * 3 / 2 : s->payment() * 2;
    s->payment(new_pay);

    bool stays = rand_percentage_chance((outcome == 1) ? 40 : 80);
    if (stays) {
      s->motivation(20);
    }
    else {
      restaurant.fire_staff(event.job_role, event.slot_index);
      result.staff_left = true;
    }
    // Reply is always shown for Quit.
    // Line 13 is "sorry I'm quitting" and line 14 is "ok I'll stay"
    result.reply = StaffEventDialogs::line(event.type, stays ? 14 : 13);
    return result;
  }

  // Non-Quit effects
  switch (event.type) {
  case Type::Late:
    s->adjust_motivation(-((outcome * 4) - 2));
    break;
  case Type::Sick:
    s->adjust_motivation(-((outcome * 8) - 3));
    break;
  case Type::Raise:
    s->adjust_motivation((1 - outcome) * 8);
    break;
  case Type::Maternity:
    // Informational only; absence already set when event fired.
    break;
  default:
    // PERSF: fire staff on harsh response
    if (outcome == 2) {
      restaurant.fire_staff(event.job_role, event.slot_index);
      result.staff_left = true;
    }
    break;
  }

  // Optional staff reply (50% for PERS events, 60% for PERSF)
  int reply_chance = StaffEventDialogs::is_persf(event.type) ? 60 : 50;
  if (rand_percentage_chance(reply_chance)) {
    result.reply = StaffEventDialogs::line(event.type, 13 + outcome);
  }
  return result;
}

void StaffEvents::process_staff_member(Staff& staff, JobRole role, int slot) {
  // Only one event fires per restaurant per day
  if (event_fired_) {
    return;
  }

  // Skip staff on sick leave, maternity leave, or training
  if (staff.is_away()) {
    return;
  }

  StaffEventData base{.job_role = role, .slot_index = slot};

  // Event chain priority order:
  // Late -> Sick -> Maternity -> Salary (silent) -> Quit -> Raise -> Theft ->
  // PERSF accident
  if (check_late(staff, base)) {
    staff.recently_demanded_raise(false);
    event_fired_ = true;
    return;
  }
  if (check_sick(staff, base)) {
    staff.recently_demanded_raise(false);
    event_fired_ = true;
    return;
  }
  if (check_maternity(staff, base)) {
    staff.recently_demanded_raise(false);
    event_fired_ = true;
    return;
  }
  // Silent salary/motivation check. recently_demanded_raise is intentionally
  // not cleared here so the -8 penalty persists until a different event fires.
  if (check_salary(staff)) {
    event_fired_ = true;
    return;
  }
  if (check_quitting(staff, base)) {
    staff.recently_demanded_raise(false);
    event_fired_ = true;
    return;
  }
  if (check_raise(staff, base)) {
    event_fired_ = true;
    return;
  }
  if (check_theft(staff)) {
    staff.recently_demanded_raise(false);
    event_fired_ = true;
    return;
  }
  if (check_persf(staff, base)) {
    staff.recently_demanded_raise(false);
    event_fired_ = true;
    return;
  }
}

bool StaffEvents::check_late(Staff& staff, const StaffEventData& base) {
  // Both checks must fail for the staff member to be late.
  if (rand_percentage_chance(staff.character().trustworthiness())) {
    return false;
  }
  if (rand_percentage_chance(staff.character().motivation())) {
    return false;
  }

  staff.late(true);

  if (restaurant_->manager()) {
    // Manager auto-handles: apply harshness effect, no player dialog.
    int h = ManagerFeedback::calculate_harshness(
        restaurant_->manager_settings().behavior_towards_staff);
    staff.adjust_motivation(-((h * 4) - 2));
  }
  else {
    auto event = base;
    event.type = StaffEventData::Type::Late;
    pending_dialogs_.push_back(event);
  }
  return true;
}

bool StaffEvents::check_sick(Staff& staff, const StaffEventData& base) {
  // 10% base chance to even check
  if (rand_percentage_chance(90)) {
    return false;
  }
  if (rand_percentage_chance(staff.character().health() + 20)) {
    return false;
  }
  if (rand_percentage_chance(staff.character().motivation())) {
    return false;
  }

  // Sick! Absence lasts 1-3 additional days.
  staff.sick(true);
  staff.days_away(rand_between(1, 4));

  if (restaurant_->manager()) {
    int h = ManagerFeedback::calculate_harshness(
        restaurant_->manager_settings().behavior_towards_staff);
    staff.adjust_motivation(-((h * 8) - 3));
  }
  else {
    auto event = base;
    event.type = StaffEventData::Type::Sick;
    pending_dialogs_.push_back(event);
  }
  return true;
}

bool StaffEvents::check_maternity(Staff& staff, const StaffEventData& base) {
  if (staff.character().gender_flag() == 0) {
    return false;
  }

  // Two independent 4% rolls, then two charisma-based rolls (all must pass).
  if (!rand_percentage_chance(4)) {
    return false;
  }
  if (!rand_percentage_chance(4)) {
    return false;
  }
  uint8_t charisma = staff.character().charisma();
  if (!rand_percentage_chance(charisma)) {
    return false;
  }
  if (!rand_percentage_chance(charisma)) {
    return false;
  }

  // Absent for 90 days.
  staff.pregnant(true);
  staff.days_away(90);

  auto event = base;
  event.type = StaffEventData::Type::Maternity;
  pending_dialogs_.push_back(event);
  return true;
}

bool StaffEvents::check_salary(Staff& staff) {
  if (!rand_percentage_chance(staff.character().dexterity())) {
    return false;
  }

  auto salary = staff.payment().whole();
  int calculated = calculate_fair_salary(staff);

  if (calculated > salary) {
    // Underpaid
    staff.adjust_motivation(-2);
    // Significantly underpaid
    if (salary * 1.4 < calculated) {
      staff.adjust_motivation(-2);
    }
    // Extra penalty if staff recently demanded a raise and was refused
    if (staff.recently_demanded_raise()) {
      staff.adjust_motivation(-8);
    }
  }
  // Overpaid
  if (salary > calculated) {
    if (staff.character().motivation() < 70) {
      staff.adjust_motivation(1);
    }
  }
  // Very overpaid
  if (salary > calculated * 1.5) {
    if (staff.character().motivation() < 90) {
      staff.adjust_motivation(1);
    }
  }

  // Consume the daily event slot silently, no intera
  return true;
}

bool StaffEvents::check_quitting(Staff& staff, const StaffEventData& base) {
  // Only staff at zero motivation consider quitting.
  if (staff.character().motivation() >= 1) {
    return false;
  }
  // Not sure why but high courage means less likely to quit
  if (rand_percentage_chance(staff.character().courage())) {
    return false;
  }

  // Manager does not handle quit events, so schedule regardless
  staff.wants_to_quit(true);
  auto event = base;
  event.type = StaffEventData::Type::Quit;
  pending_dialogs_.push_back(event);
  return true;
}

bool StaffEvents::check_raise(Staff& staff, const StaffEventData& base) {
  if (staff.character().motivation() >= 15) {
    return false;
  }
  // High courage resists demanding a raise.
  if (rand_percentage_chance(staff.character().courage())) {
    return false;
  }

  // Manager does not auto-handle raise demands.
  staff.demands_raise(true);
  // Mark this state so the salary check applies an extra -8 motivation penalty
  // on subsequent days until a different event fires
  staff.recently_demanded_raise(true);
  auto event = base;
  event.type = StaffEventData::Type::Raise;
  pending_dialogs_.push_back(event);
  return true;
}

bool StaffEvents::check_theft(Staff& staff) {
  // 3% base chance for theft
  if (!rand_percentage_chance(3)) {
    return false;
  }
  // Honesty (hidden stat) prevents theft
  if (rand_percentage_chance(staff.character().honesty() + 20)) {
    return false;
  }
  apply_event_effect(StaffEventData::Type::Theft, staff, *restaurant_,
                     *game_state_);
  return true;
}

bool StaffEvents::check_persf(Staff& staff, const StaffEventData& base) {
  // 5% base chance for an accident, gated by trustworthiness
  if (!rand_percentage_chance(5)) {
    return false;
  }
  if (rand_percentage_chance(staff.character().trustworthiness() + 10)) {
    return false;
  }

  // Event type depends on the staff member's role
  using Type = StaffEventData::Type;
  Type type;
  switch (base.job_role) {
  case JobRole::CHEF:
    type = coin_flip() ? Type::DestroyCheeseStock : Type::BlowUpOven;
    break;
  case JobRole::WAITER:
    type = coin_flip() ? Type::SpillWine : Type::MessedUpOrders;
    break;
  default: // MANAGER
    type = Type::ForgotToOpen;
    break;
  }

  auto event = base;
  event.type = type;
  pending_dialogs_.push_back(event);
  return true;
}
