/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>
#include "core/conversations/BaseConversation.h"
#include "core/state/UIState.h"
#include "core/util/GameContext.h"
#include "views/screens/ActorView.h"

class GameState;
class MainPanelView;

class ActorViewFactory {
public:
  static std::unique_ptr<MainPanelView>
  create(GameContext& context, GameState& game_state, ActorTerminalState ats);

  template <typename ConversationT, typename... Args>
  static auto make_actor_view(GameContext& context, GameState& game_state,
                              Actor* actor, Args&&... args) {
    using line_enum = typename ConversationT::line_enum;

    auto conversation = std::make_unique<ConversationT>(
        game_state, actor, std::forward<Args>(args)...);

    return std::make_unique<ActorView<line_enum>>(context, game_state, actor,
                                                  std::move(conversation));
  }
};
