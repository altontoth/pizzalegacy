/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdlib>

#include "conversations/BaseConversation.h"
#include "core/character/Actor.h"
#include "views/base/MainPanelView.h"

enum class ConversationPhase {
  TALKING,
  EMOTING,
  WAITING,
  ENDING,
};

constexpr int DEFAULT_CONVERSATION_DELAY = 50;
constexpr int happy_stage_1_delay = 26;
constexpr int happy_stage_2_delay = 16;
constexpr int happy_stage_3_delay = 26;

template <typename LineEnum> class ActorView : public MainPanelView {
public:
  ActorView(GameContext& context, GameState& state, Actor* actor,
            std::unique_ptr<BaseConversation<LineEnum>> conversation);
  ~ActorView() override;

  void tick() override;
  void on_phase_finished();
  bool update_conversation();
  bool update_face_animations();
  bool update_idle_animations();
  bool update_emotions();
  void render(const RenderContext& ctx) override;

private:
  void render_dialog(const RenderContext& ctx);
  void render_contract_terms(const RenderContext& ctx);
  void advance_conversation();
  void advance_conversation(LineEnum choice);
  void advance_proposal(bool accept);
  void apply_emotion_if_needed(
      bool state_changed,
      std::vector<ConversationLine<LineEnum>> previous_lines = {});
  void render_character_head(const RenderContext&);

  Actor* actor_;
  int count_{0};
  int last_blink_at_{0};
  std::unique_ptr<BaseConversation<LineEnum>> conversation_;
  bool already_emoted_{false};
  int conversation_delay_{DEFAULT_CONVERSATION_DELAY};
  ConversationPhase current_phase_{ConversationPhase::TALKING};
  int phase_timer_{0};

  int hovered_line_id_{-1};
  Bob& actor_bob_;
  std::string bg_filename_;
  int happy_stage_ = 0;
  int happy_timer_ = 0;
  int nod_frame_ = 0;
  int nod_timer_ = 0;

  // During emoting, we freeze the previous dialog text
  std::vector<ConversationLine<LineEnum>> frozen_lines_;
};

#include "views/screens/ActorView.inl"
