/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <string>

#include "core/city/MayorOffice.h"
#include "core/conversations/BaseConversation.h"
#include "core/conversations/MayorLine.h"
#include "core/state/GameState.h"

class MayorConversation : public BaseConversation<MayorLine> {
public:
  // Pre-computed outcome of a quality inspection (star change + first-time
  // flag). Computed and applied by the delivery code before the conversation is
  // shown.
  struct InspectionOutcome {
    bool star_gained{false};
    bool star_lost{false};
    bool is_first{false};
  };

  MayorConversation(GameState&, const Actor*, MayorOffice& mo,
                    MayorConversationType mt,
                    std::optional<InspectionOutcome> outcome = std::nullopt,
                    std::optional<int> award_winner_player_id = std::nullopt,
                    std::string restaurant_name = {});

  Emotion emotion() const override;
  std::vector<ConversationLine<MayorLine>> lines() const override;
  bool advance() override;
  bool advance_line_choice(MayorLine) override;
  bool advance_proposal(bool) override;
  bool has_ended() const override;

private:
  static MayorLine compute_start_line(const GameState& state, const Actor*,
                                      MayorConversationType,
                                      const MayorOffice& mo);
  MayorLine outcome_result_line() const;
  // True when the active player is the award winner.
  bool active_player_won() const;

  MayorOffice& mayor_office_;
  std::optional<InspectionOutcome> inspection_outcome_;
  std::optional<int> award_winner_player_id_;
  std::string restaurant_name_;
  std::string appointment_date_str_;
};
