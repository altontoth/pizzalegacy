/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <iostream>
#include <string>
#include <vector>

#include "conversations/BaseConversation.h"
#include "conversations/PoliceLine.h"
#include "core/state/GameState.h"

class PoliceConversation : public BaseConversation<PoliceLine> {
public:
  PoliceConversation(GameState&, const Actor*,
                     std::optional<ArrestReason> ar = std::nullopt);
  virtual Emotion emotion() const override;
  virtual std::vector<ConversationLine<PoliceLine>> lines() const override;
  virtual bool advance() override;
  virtual bool advance_line_choice(PoliceLine) override;
  virtual bool advance_proposal(bool) override;
  virtual bool has_ended() const override;

private:
  static PoliceLine compute_start_line(const GameState& state, const Actor*);
  std::optional<ArrestReason> arrest_reason_;
  //   bool load_polizei_file(const std::string& file_path);
  // ConversationLine make_line(PoliceLine id, bool is_option = false) const;
};
