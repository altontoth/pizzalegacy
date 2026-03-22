/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/factories/ActorViewFactory.h"

#include "conversations/ArmsDealerConversation.h"
#include "conversations/BankConversation.h"
#include "conversations/MafiaConversation.h"
#include "conversations/MayorConversation.h"
#include "conversations/PoliceConversation.h"
#include "conversations/RealtorConversation.h"
#include "core/character/Actor.h"
#include "core/city/City.h"
#include "engine/Fatal.h"

#include <memory>

std::unique_ptr<MainPanelView>
ActorViewFactory::create(GameContext& context, GameState& game_state,
                         ActorTerminalState ats) {

  std::unique_ptr<MainPanelView> mpv;
  auto* city = game_state.active_city();
  auto* actor = city->actor_by_id(ats.id);

  switch (ats.type) {
  case ActorType::POLICE: {
    mpv = make_actor_view<PoliceConversation>(context, game_state, actor);
    break;
  }
  case ActorType::ARMS_DEALER: {
    mpv = make_actor_view<ArmsDealerConversation>(context, game_state, actor);
    break;
  }
  case ActorType::BANK: {
    mpv = make_actor_view<BankConversation>(context, game_state, actor,
                                            city->bank_settings(actor->id()));
    break;
  }
  case ActorType::REALTOR: {
    mpv = make_actor_view<RealtorConversation>(
        context, game_state, actor, city->realtor_settings(actor->id()));
    break;
  }
  case ActorType::MAYOR: {
    mpv = make_actor_view<MayorConversation>(context, game_state, actor,
                                             city->mayor_office(),
                                             MayorConversationType::PhoneCall);
    break;
  }
  case ActorType::MAFIA: {
    mpv = make_actor_view<MafiaConversation>(context, game_state, actor,
                                             MafiaMode::PlayerCall);
    break;
  }
  default:
    fatal("Unhandled ActorType in ActorViewFactory");
    break;
  }

  return mpv;
}
