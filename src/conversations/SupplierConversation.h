/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <memory>
#include <vector>

#include "conversations/BaseConversation.h"
#include "conversations/SupplierLine.h"
#include "core/character/Actor.h"
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

private:
  static SupplierLine compute_greeting(const GameState& state,
                                       const FoodSupplier* supplier,
                                       Money total_order_value);
  void handle_demand(SupplierLine demand);
  void pick_demand_options();

  const FoodSupplier* supplier_;
  std::unique_ptr<Actor> owned_actor_;
  std::vector<IngredientBatch> order_;
  Money total_order_value_;

  SupplierContract contract_;
  int customer_tier_{0};
  int negotiation_skill_{0};
  int demand_threshold_{2};
  std::vector<SupplierLine> active_demand_options_;
  bool special_price_offered_{false};
  std::array<int, 9> session_demand_counts_{};
};
