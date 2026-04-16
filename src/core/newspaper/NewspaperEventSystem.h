/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <cstdint>
#include <string_view>

class City;
class GameState;

namespace core::newspaper {

class NewspaperState;

enum class NewspaperEvent {
  TomatoesContaminated = 1,
  CucumbersPoisoned = 2,
  TomatoesDecontaminated = 3,
  GroundWaterClear = 4,
  NewOvenDeveloped = 5,
  HeatWave = 6,
  ColdSnap = 7,
  ImportHaltedVegetables = 8,
  ImportStoppedMeat = 9,
  TradeEmbargoFruit = 10,
  TradeEmbargoCheese = 11,
  InterestRatesRising = 12,
  InterestRatesFalling = 13,
  HealthConsciousness = 14,
  PizzaComingUp = 15,
  LandPricesRising = 16,
  LandPricesFalling = 17,
  PizzeriaBlownUp = 18,
  NetherlandsStopTrade = 19,
  NetherlandsResumeTrade = 20,
  MusicWhileYouDine = 21,
  GamblingAfterDinner = 22,
  PizzaRobbery = 23,
  SickMenOfEurope = 24,
  GarlicBlacklisted = 25,
  SalamiOutOfFavour = 26,
  PizzeriaContaminated = 27,
  PizzeriaBlownUp2 = 28,
  EuropeSickAgain = 29,
  SpansishSaladContaminated = 30,
  ApplesComingUp = 31,
  NoPineappleDemand = 32,
  PeachesHealthy = 33,
  NewNutritionTrend = 34,
  KiwiMegaIn = 35,
  ChilliesOut = 36,
  PotatoesHealthy = 37,
  CornIsOut = 38,
  AsparagusLooks = 39,
  MushroomsContaminated = 40,
  SalmonHealthy = 41,
  TroutContaminated = 42,
  CaviarMegaOut = 43,
  LobsterBoycotted = 44,
  BeefRising = 45,
  NoDemandForEggs = 46,
  GingerInDemand = 47,
  GorgonzolaFantastic = 48,
  ChangeOfMind = 49,
  ChildrenPreferSpicy = 50,
  ElderlyPreferSweet = 51,
};

constexpr std::string_view to_string(NewspaperEvent event) {
  switch (event) {
  case NewspaperEvent::TomatoesContaminated:
    return "TomatoesContaminated";
  case NewspaperEvent::CucumbersPoisoned:
    return "CucumbersPoisoned";
  case NewspaperEvent::TomatoesDecontaminated:
    return "TomatoesDecontaminated";
  case NewspaperEvent::GroundWaterClear:
    return "GroundWaterClear";
  case NewspaperEvent::NewOvenDeveloped:
    return "NewOvenDeveloped";
  case NewspaperEvent::HeatWave:
    return "HeatWave";
  case NewspaperEvent::ColdSnap:
    return "ColdSnap";
  case NewspaperEvent::ImportHaltedVegetables:
    return "ImportHaltedVegetables";
  case NewspaperEvent::ImportStoppedMeat:
    return "ImportStoppedMeat";
  case NewspaperEvent::TradeEmbargoFruit:
    return "TradeEmbargoFruit";
  case NewspaperEvent::TradeEmbargoCheese:
    return "TradeEmbargoCheese";
  case NewspaperEvent::InterestRatesRising:
    return "InterestRatesRising";
  case NewspaperEvent::InterestRatesFalling:
    return "InterestRatesFalling";
  case NewspaperEvent::HealthConsciousness:
    return "HealthConsciousness";
  case NewspaperEvent::PizzaComingUp:
    return "PizzaComingUp";
  case NewspaperEvent::LandPricesRising:
    return "LandPricesRising";
  case NewspaperEvent::LandPricesFalling:
    return "LandPricesFalling";
  case NewspaperEvent::PizzeriaBlownUp:
    return "PizzeriaBlownUp";
  case NewspaperEvent::NetherlandsStopTrade:
    return "NetherlandsStopTrade";
  case NewspaperEvent::NetherlandsResumeTrade:
    return "NetherlandsResumeTrade";
  case NewspaperEvent::MusicWhileYouDine:
    return "MusicWhileYouDine";
  case NewspaperEvent::GamblingAfterDinner:
    return "GamblingAfterDinner";
  case NewspaperEvent::PizzaRobbery:
    return "PizzaRobbery";
  case NewspaperEvent::SickMenOfEurope:
    return "SickMenOfEurope";
  case NewspaperEvent::GarlicBlacklisted:
    return "GarlicBlacklisted";
  case NewspaperEvent::SalamiOutOfFavour:
    return "SalamiOutOfFavour";
  case NewspaperEvent::PizzeriaContaminated:
    return "PizzeriaContaminated";
  case NewspaperEvent::PizzeriaBlownUp2:
    return "PizzeriaBlownUp2";
  case NewspaperEvent::ApplesComingUp:
    return "ApplesComingUp";
  case NewspaperEvent::NoPineappleDemand:
    return "NoPineappleDemand";
  case NewspaperEvent::PeachesHealthy:
    return "PeachesHealthy";
  case NewspaperEvent::NewNutritionTrend:
    return "NewNutritionTrend";
  case NewspaperEvent::KiwiMegaIn:
    return "KiwiMegaIn";
  case NewspaperEvent::ChilliesOut:
    return "ChilliesOut";
  case NewspaperEvent::PotatoesHealthy:
    return "PotatoesHealthy";
  case NewspaperEvent::CornIsOut:
    return "CornIsOut";
  case NewspaperEvent::AsparagusLooks:
    return "AsparagusLooks";
  case NewspaperEvent::MushroomsContaminated:
    return "MushroomsContaminated";
  case NewspaperEvent::SalmonHealthy:
    return "SalmonHealthy";
  case NewspaperEvent::TroutContaminated:
    return "TroutContaminated";
  case NewspaperEvent::CaviarMegaOut:
    return "CaviarMegaOut";
  case NewspaperEvent::LobsterBoycotted:
    return "LobsterBoycotted";
  case NewspaperEvent::BeefRising:
    return "BeefRising";
  case NewspaperEvent::NoDemandForEggs:
    return "NoDemandForEggs";
  case NewspaperEvent::GingerInDemand:
    return "GingerInDemand";
  case NewspaperEvent::GorgonzolaFantastic:
    return "GorgonzolaFantastic";
  case NewspaperEvent::ChildrenPreferSpicy:
    return "ChildrenPreferSpicy";
  case NewspaperEvent::ElderlyPreferSweet:
    return "ElderlyPreferSweet";
  case NewspaperEvent::EuropeSickAgain:
    return "EuropeSickAgain";
  case NewspaperEvent::SpansishSaladContaminated:
    return "SpansishSaladContaminated";
  case NewspaperEvent::ChangeOfMind:
    return "ChangeOfMind";
    break;
  }
  return "None";
}

// Daily orchestrator for newspaper events.
// Owns 50 scheduled event slots and drives the IngredientTrendSystem on City.
class NewspaperEventSystem {
public:
  void daily_update(GameState& state, City& city);

  // Schedule a follow-up event (used by event chaining, e.g. event 1 -> 3)
  void schedule_event(int slot, NewspaperEvent event, int days);

  [[nodiscard]] const std::array<NewspaperEvent, 50>& scheduled_types() const {
    return scheduled_types_;
  }
  [[nodiscard]] const std::array<int, 50>& scheduled_countdowns() const {
    return scheduled_countdowns_;
  }

  // Fire an event directly (debug use). Delegates to execute_named_event().
  bool fire_event(NewspaperEvent event, GameState& state, City& city) {
    return execute_named_event(event, state, city);
  }

  // Restore scheduled events from save data
  void restore_scheduled(const std::array<NewspaperEvent, 50>& types,
                         const std::array<int, 50>& countdowns) {
    scheduled_types_ = types;
    scheduled_countdowns_ = countdowns;
  }

  // Generate a random ingredient trend event (debug use).
  bool generate_ingredient_event(GameState& state, City& city);

private:
  // Returns true if event fired and set an article (caller should exit early).
  // Returns false if event was suppressed (e.g. heat wave in wrong season).
  bool execute_named_event(NewspaperEvent event, GameState& state, City& city);

  bool try_generate_ingredient_event(NewspaperState& ns, City& city);

  // Scheduled event slots
  std::array<NewspaperEvent, 50> scheduled_types_{};
  std::array<int, 50> scheduled_countdowns_{};
};

} // namespace core::newspaper
