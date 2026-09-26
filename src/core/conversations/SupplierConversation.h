/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <memory>
#include <optional>
#include <vector>

#include "core/character/Actor.h"
#include "core/conversations/BaseConversation.h"
#include "core/conversations/SupplierLine.h"
#include "core/finance/Money.h"
#include "core/supply/IngredientBatch.h"
#include "core/supply/SupplierContract.h"

class FoodSupplier;

class SupplierConversation : public BaseConversation<SupplierLine> {
public:
  SupplierConversation(GameState& state, const FoodSupplier* supplier,
                       std::vector<IngredientBatch> order,
                       Money total_order_value);

  Emotion emotion() const override;
  std::vector<ConversationLine<SupplierLine>> lines() const override;
  bool advance() override;
  bool advance_line_choice(SupplierLine choice) override;
  bool advance_proposal(bool agree) override;
  bool has_ended() const override;
  std::optional<ContractTermsDisplay> contract_terms() const override;

  Actor* owned_actor() { return owned_actor_.get(); }
  bool contract_accepted() const { return contract_accepted_; }

private:
  static SupplierLine compute_greeting(const GameState& state,
                                       const FoodSupplier* supplier,
                                       Money total_order_value);
  void handle_demand(SupplierLine demand);
  void pick_demand_options();

  enum class JustifyContext { DISCOUNT, PENALTY, FEES };
  void trigger_justify_demands(JustifyContext);
  void handle_argument_pick(SupplierLine);

  const FoodSupplier* supplier_;
  std::unique_ptr<Actor> owned_actor_;
  std::vector<IngredientBatch> order_;
  Money total_order_value_;
  bool contract_accepted_{false};

  SupplierContract contract_;
  int spending_rank_{0};
  int negotiation_skill_{0};
  std::vector<SupplierLine> active_demand_options_;
  bool ripoff_{false};
  std::optional<JustifyContext> justify_context_;
  std::vector<SupplierLine> active_argument_options_;
  std::array<int, 9> session_demand_counts_{};
};
