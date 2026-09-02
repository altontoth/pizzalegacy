/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "core/finance/Money.h"
#include "engine/input/TextSession.h"
#include "gfx/PaletteHandle.h"
#include "views/ui/popups/BasePopup.h"

class ChequePopup : public BasePopup {
public:
  using callback_t = std::function<void(Money amount, int account)>;

  ChequePopup(const GameContext& context, callback_t cb);

  void render(Surface& surface, ViewPoint origin) override;
  void add_interactions(views::ui::InteractionContext, ViewPoint) override{};

  // The cheque graphic is drawn in its own palette regardless of menu/game
  // mode.
  std::optional<gfx::PaletteHandle> palette_override() const override {
    return vga_->get(BACKGROUND_ASSET).palette();
  }

  bool on_text(const input::TextEvent& ev) override;
  bool on_action(const input::ActionEvent& ev) override;

private:
  enum class Field { AMOUNT, ACCOUNT };

  static constexpr std::string_view BACKGROUND_ASSET = "gfx/back36";
  static constexpr int MAX_DIGITS = 8;
  static constexpr ViewRect amount_box_ = {254, 134, 100, 20};
  static constexpr ViewRect account_box_ = {228, 164, 100, 20};
  static constexpr ViewPoint amount_label_pos_ = {182, 134};
  static constexpr ViewPoint account_label_pos_ = {86, 164};

  const GameContext& context_;
  callback_t callback_;
  Field active_field_{Field::AMOUNT};
  std::string amount_buffer_;
  std::string account_buffer_;
  input::TextSession text_session_;

  void confirm_amount();
  void submit();
};
