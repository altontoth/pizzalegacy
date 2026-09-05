/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/factories/ActorViewFactory.h"

#include "core/character/Actor.h"
#include "core/city/City.h"
#include "core/conversations/ArmsDealerConversation.h"
#include "core/conversations/BankConversation.h"
#include "core/conversations/InsuranceConversation.h"
#include "core/conversations/MafiaConversation.h"
#include "core/conversations/MayorConversation.h"
#include "core/conversations/PoliceConversation.h"
#include "core/conversations/RealtorConversation.h"
#include "engine/Fatal.h"
#include "engine/FormatCompat.h"

#include <magic_enum/magic_enum.hpp>
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
  case ActorType::INSURANCE: {
    mpv = make_actor_view<InsuranceConversation>(
        context, game_state, actor, city->insurance_settings(actor->id()),
        actor->id());
    break;
  }
  default:
    fatal(compat::format("Unhandled ActorType in ActorViewFactory: {}",
                         magic_enum::enum_name(ats.type)));
    break;
  }

  return mpv;
}
