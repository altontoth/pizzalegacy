/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/tutorial/TutorialGame.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <optional>

#include "core/GameTypes.h"
#include "core/character/Actor.h"
#include "core/character/Character.h"
#include "core/city/City.h"
#include "core/city/CityTypes.h"
#include "core/city/Property.h"
#include "core/common/Ids.h"
#include "core/factories/CharacterFactory.h"
#include "core/finance/Money.h"
#include "core/food/PizzaJudging.h"
#include "core/init/CompanyNames.h"
#include "core/init/NewGameConfig.h"
#include "core/state/GameRules.h"
#include "core/state/GameState.h"
#include "core/tutorial/Tutorial.h"
#include "core/tutorial/TutorialSteps.h"
#include "core/util/Constants.h"
#include "engine/Random.h"
#include "engine/Size.h"

namespace core::tutorial {

namespace {
constexpr uint32_t tutorial_seed = 292;
constexpr int tutorial_character = 0;
// Madrid.
constexpr core::CityId tutorial_city{6};
constexpr init::DifficultyLevel tutorial_difficulty =
    init::DifficultyLevel::NORMAL;
constexpr Size premise_size{15, 13};
constexpr Size premise_kitchen_size{12, 13};
constexpr int premise_entrance_offset = 7;
constexpr int premise_kitchen_entry_offset = 6;
constexpr Money premise_price = Money::from_dollars(292500);
} // namespace

void start_tutorial_game(GameState& state, GameContext& context,
                         std::optional<uint32_t> seed) {
  state.begin_initialization();
  auto& cfg = state.initializer();
  cfg.mode = GameMode::FREEMODE;
  cfg.difficulty = tutorial_difficulty;
  cfg.city_id = tutorial_city;
  engine::random::seed(seed.value_or(tutorial_seed));
  cfg.players.resize(NUMBER_OF_PLAYERS);
  const auto company_names = init::random_company_names();
  for (int i = 0; i < NUMBER_OF_PLAYERS; ++i) {
    int character_id = tutorial_character + i;
    if (character_id >= NUMBER_OF_CHARACTERS) {
      character_id -= NUMBER_OF_CHARACTERS;
    }
    const auto* character = CharacterFactory::create_character(character_id);
    cfg.players[i].name = company_names[i];
    cfg.players[i].character_id = character_id;
    cfg.players[i].character_name = character->name();
    cfg.players[i].is_human = i == 0;
  }
  state.finalize_initialization(context);

  // Ensure the city map has the right premise available regardless of which
  // seed is used.
  auto* city = state.active_city();
  auto* premise = city->property_by_index(tutorial_premise.index());
  premise->status(PremiseStatus::FOR_RENT);
  premise->size(premise_size);
  premise->kitchen_size(premise_kitchen_size);
  premise->entrance_offset(premise_entrance_offset);
  premise->kitchen_entry_offset(premise_kitchen_entry_offset);
  premise->purchase_price(premise_price);
  const auto realtors = city->actors(ActorType::REALTOR);
  const auto realtor = std::ranges::find_if(realtors, [](const Actor* actor) {
    return actor->name() == tutorial_realtor;
  });
  assert(realtor != realtors.end());
  premise->realtor_id((*realtor)->id());

  state.rules().pizza_judging = PizzaJudging::Relaxed;
  state.rules().cook_book = true;
  state.starting_rules(state.rules());

  state.tutorial().start();
}

} // namespace core::tutorial
