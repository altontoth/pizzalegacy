/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/newspaper/NewspaperEventSystem.h"

#include "core/city/City.h"
#include "core/city/IngredientTrendSystem.h"
#include "core/food/Ingredient.h"
#include "core/newspaper/NewspaperState.h"
#include "core/state/GameState.h"
#include "core/time/GameTime.h"

using core::food::IngredientType;
using enum TrendDuration;

namespace core::newspaper {

using enum IngredientType;

void NewspaperEventSystem::daily_update(GameState& state, City& city) {
  auto& ns = city.newspaper_state();

  // Cooldown for any article that was posted but not viewed yet:
  if (ns.cooldown > 0) {
    ns.cooldown--;
  }

  // Process scheduled countdowns
  for (int i = 0; i < 50; ++i) {
    if (scheduled_countdowns_[i] > 0) {
      scheduled_countdowns_[i]--;
      if (scheduled_countdowns_[i] == 0) {
        if (execute_named_event(scheduled_types_[i], state, city)) {
          return;
        }
      }
    }
  }

  // 5% chance of a scheduled event
  if (engine::random::rand_percentage_chance(5)) {
    auto id = static_cast<NewspaperEvent>(engine::random::rand_between(1, 50));
    if (execute_named_event(id, state, city)) {
      return;
    }
  }

  // 18% chance of an ingredient trend firing
  if (engine::random::rand_percentage_chance(18)) {
    if (try_generate_ingredient_event(ns, city)) {
      return;
    }
  }

  // If cooldown expired, clear the article
  if (ns.cooldown == 0) {
    ns.article_id = 0;
  }

  // If no article, pick random flavor text from ARTIKEL0.E (1-50)
  if (ns.article_id == 0) {
    ns.article_id = engine::random::rand_between(1, 50);
    ns.cooldown = NewspaperState::FALLBACK_COOLDOWN;
  }
}

void NewspaperEventSystem::schedule_event(int slot, NewspaperEvent event,
                                          int days) {
  if (slot >= 0 && slot < 50) {
    scheduled_types_[slot] = event;
    scheduled_countdowns_[slot] = days;
  }
}

// Returns true if the event fired successfully and set an article.
// Returns false if the event was suppressed so we can try another source of
// events
bool NewspaperEventSystem::execute_named_event(NewspaperEvent event,
                                               GameState& state, City& city) {
  auto& ns = city.newspaper_state();
  auto& trends = city.ingredient_trend_system();
  auto event_id = static_cast<int>(event);

  switch (event) {
    using enum NewspaperEvent;
  case TomatoesContaminated:
    trends.apply_effect(
        IngredientType::TOMATOES,
        {.initial_value = 0, .control = -50, .duration = Exponential});
    // Schedule follow-up: decontamination in 10-19 days
    schedule_event(0, TomatoesDecontaminated,
                   engine::random::rand_between(10, 19));
    break;
  case CucumbersPoisoned:
    trends.apply_effect(
        IngredientType::CUCUMBERS,
        {.initial_value = 0, .control = -100, .duration = Exponential});
    break;
  case TomatoesDecontaminated:
    trends.apply_effect(
        IngredientType::TOMATOES,
        {.initial_value = 0, .control = 50, .duration = Exponential});
    break;

  case GroundWaterClear:
    // TODO: This is a follow-up for the cucumber one, but it is never scheduled
    // in the original. Maybe as part of our fixes we schedule it?
  case NewOvenDeveloped:
    break;

  case HeatWave: {
    // Only triggers in spring/summer
    auto season = state.time().season();
    if (season != Season::Spring && season != Season::Summer) {
      return false;
    }
    city.spoilage_rate(8);
    city.spoilage_duration_counter(3);
    break;
  }

  case ColdSnap:
    // "Cucumbers are in great demand"
    trends.apply_effect(
        IngredientType::CUCUMBERS,
        {.initial_value = 25, .control = 20, .duration = Countdown});
    break;

  // Trade embargoes
  case ImportHaltedVegetables:
    city.trade_embargo_counter(IngredientCategory::VEGETABLES, 12 * 7);
    break;

  case ImportStoppedMeat:
    city.trade_embargo_counter(IngredientCategory::MEAT, 4 * 7);
    break;

  case TradeEmbargoFruit:
    city.trade_embargo_counter(IngredientCategory::FRUIT, 8 * 7);
    break;

  case TradeEmbargoCheese:
    city.trade_embargo_counter(IngredientCategory::CHEESE, 12 * 7);
    break;

  // --- Interest rate events ---
  case InterestRatesRising:
    // TODO: Modifies interest rate (increases by random 2-6)-this system does
    // not seem to be implemented in the game though, doesn't have an effect
    // afaict.
    break;
  case InterestRatesFalling:
    // TODO: Same as rising but negative
    break;
  case LandPricesRising:
    // TODO: Set something related to land prices to (2, 30) but doesn't seem to
    // have effect in game.
  case LandPricesFalling:
    // TODO: set the same things but to (255, 30) but again no effect
  case PizzeriaBlownUp:
  case NetherlandsStopTrade:
  case NetherlandsResumeTrade:
  case MusicWhileYouDine:
  case GamblingAfterDinner:
  case PizzaRobbery:
  case SickMenOfEurope:
  case GarlicBlacklisted:
  case EuropeSickAgain:
  case ChangeOfMind:
  case PizzeriaContaminated:
  case NewNutritionTrend:
  case HealthConsciousness:
  case PizzaComingUp:
    break;

  // Ingredient trend events
  case SalamiOutOfFavour:
    trends.apply_effect(
        IngredientType::SALAMI,
        {.initial_value = -30, .control = 20, .duration = Countdown});
    break;

  case PizzeriaBlownUp2:
    break;

  case ApplesComingUp:
    trends.apply_effect(
        IngredientType::APPLES,
        {.initial_value = 0, .control = 50, .duration = Linear});
    break;

  case NoPineappleDemand: // BUG: "No pineapples" affects raspberries (58)
    trends.apply_effect(
        IngredientType::RASPBERRIES,
        {.initial_value = -20, .control = 30, .duration = Countdown});
    break;

  case PeachesHealthy: // BUG: "Peaches" affects kiwi (peach doesn't exist)
    trends.apply_effect(
        IngredientType::KIWI,
        {.initial_value = 0, .control = 60, .duration = Exponential});
    break;

  case KiwiMegaIn: // BUG: "Kiwi mega-in" affects blackberries (59)
    trends.apply_effect(
        IngredientType::BLACKBERRIES,
        {.initial_value = 50, .control = 30, .duration = Countdown});
    break;

  case ChilliesOut:
    trends.apply_effect(IngredientType::CHILLIES, {-20, 30, Countdown});
    break;

  case PotatoesHealthy:
    trends.apply_effect(IngredientType::POTATOES, {0, 40, Linear});
    break;

  case CornIsOut:
    trends.apply_effect(IngredientType::SWEET_CORN, {-25, 20, Countdown});
    break;

  case AsparagusLooks:
    trends.apply_effect(IngredientType::ASPARAGUS, {0, 70, Exponential});
    break;

  case MushroomsContaminated:
    trends.apply_effect(IngredientType::W_MUSHROOMS, {0, -100, Exponential});
    break;

  case SalmonHealthy: // BUG: off-by-one, uses LOBSTER (37) instead of SALMON
                      // (36)
    trends.apply_effect(IngredientType::LOBSTER, {0, 65, Exponential});
    break;

  case TroutContaminated: // BUG: "Trout" affects SQUID (trout doesn't exist)
    trends.apply_effect(IngredientType::SQUID, {0, -80, Exponential});
    break;

  case CaviareMegaOut: // BUG: "Caviare mega-out" affects COCKLES (39)
    trends.apply_effect(IngredientType::COCKLES, {0, -60, Exponential});
    break;

  case LobsterBoycotted: // BUG: "Lobster boycotted" affects CAVIAR (38)
    trends.apply_effect(IngredientType::CAVIAR, {0, -90, Exponential});
    break;

  case BeefRising:
    trends.apply_effect(IngredientType::BEEF, {0, 50, Linear});
    break;

  case NoDemandForEggs:
    trends.apply_effect(IngredientType::EGG, {-16, -30, Linear});
    break;

  case GingerInDemand:
    trends.apply_effect(IngredientType::GINGER, {20, 40, Linear});
    break;

  case GorgonzolaFantastic:
    trends.apply_effect(IngredientType::GORGONZOLA, {0, 40, Linear});
    break;

  case SpansishSaladContaminated:
    trends.apply_effect(IngredientType::SALAD, {0, -100, Exponential});
    break;

  case ChildrenPreferSpicy:
  case ElderlyPreferSweet:
    // TODO: Sets demographic preferences, not sure this is hooked up in the
    // original
    break;
  }

  // Housekeeping
  ns.article_id = event_id + NewspaperState::EVENT_OFFSET;
  ns.cooldown = NewspaperState::EVENT_COOLDOWN;
  return true;
}

// Procedurally generates ingredient trend events.
bool NewspaperEventSystem::try_generate_ingredient_event(NewspaperState& ns,
                                                         City& city) {
  auto& trends = city.ingredient_trend_system();

  // Pick random ingredient slot (0-95)
  auto slot_id = engine::random::rand_max_n(max_ingredient_count);
  if (!food::is_valid_ingredient_id(slot_id)) {
    return false;
  }
  auto type = static_cast<IngredientType>(slot_id);

  // Skip if already trending
  if (std::abs(trends.get_trend(type)) > 5) {
    return false;
  }

  int category = engine::random::rand_max_n(4);
  int8_t polarity = engine::random::rand_max_n(2) == 0 ? 1 : -1;

  switch (category) {
  case 0:
    trends.apply_effect(type,
                        {.initial_value = static_cast<int8_t>(
                             polarity * engine::random::rand_between(20, 59)),
                         .control = 0,
                         .duration = Exponential});
    break;
  case 1:
    trends.apply_effect(type,
                        {.initial_value = static_cast<int8_t>(
                             polarity * engine::random::rand_between(20, 59)),
                         .control = static_cast<int8_t>(
                             polarity * engine::random::rand_between(20, 39)),
                         .duration = Linear});
    break;
  case 2:
    trends.apply_effect(
        type, {.initial_value =
                   static_cast<int8_t>(engine::random::rand_between(10, 29)),
               .control = static_cast<int8_t>(
                   polarity * engine::random::rand_between(20, 39)),
               .duration = Countdown});
    break;
  case 3:
    trends.apply_effect(type,
                        {.initial_value = static_cast<int8_t>(
                             polarity * engine::random::rand_between(30, 59)),
                         .control = 0,
                         .duration = Exponential});
    break;
  default:
    return false;
  }

  ns.article_id = NewspaperState::INGREDIENT_ARTICLE;
  ns.polarity = polarity;
  ns.category = category;
  ns.target_ingredient = type;
  ns.cooldown = NewspaperState::EVENT_COOLDOWN;

  return true;
}

bool NewspaperEventSystem::generate_ingredient_event(GameState& state,
                                                     City& city) {
  return try_generate_ingredient_event(city.newspaper_state(), city);
}

} // namespace core::newspaper
