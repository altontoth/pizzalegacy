/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/TurnPresenterView.h"

#include "core/city/MayorOffice.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "core/util/GameContext.h"
#include "views/ui/PopupManager.h"
#include "views/ui/finance/TurnInvoicePopups.h"
#include "views/ui/popups/AlertPopup.h"

namespace views::ui {

void TurnPresenterView::show_manager_alert(std::string message,
                                           const RawRenderable* portrait,
                                           std::function<void()> on_done) {
  context_.popup->create<AlertPopup>(std::move(message), std::move(on_done),
                                     portrait);
}

void TurnPresenterView::show_weekly_reports(
    const std::vector<Restaurant*>& restaurants, core::PlayerId previous_player,
    std::function<void()> on_done, bool interleave_electricity) {
  views::ui::finance::push_weekly_restaurant_reports(
      context_, gs_, restaurants, previous_player, std::move(on_done),
      interleave_electricity);
}

void TurnPresenterView::show_electricity_slot_invoice(
    const core::finance::ElectricitySlotInvoice& invoice,
    std::function<void()> on_done) {
  views::ui::finance::show_electricity_slot_invoice_popup(
      context_, gs_, invoice, std::move(on_done));
}

void TurnPresenterView::show_monthly_invoice(
    core::PlayerId player_id, const core::finance::MonthlyCosts& costs,
    std::function<void()> on_done) {
  views::ui::finance::show_monthly_invoice_popup(context_, gs_, player_id,
                                                 costs, std::move(on_done));
}

void TurnPresenterView::show_winner_announcement(
    const MayorAwardEvent& award, std::function<void()> on_done) {
  auto call = gs_.mayor_award_call(award);
  if (!call) {
    on_done();
    return;
  }
  call->on_complete = std::move(on_done);
  gs_.ui().push<MayorState>(std::move(*call));
}

void TurnPresenterView::show_player_turn_card(core::PlayerId player,
                                              const GameDate& date,
                                              std::function<void()> on_closed) {
  gs_.ui().push<PlayerTurnCardState>(player, date, std::move(on_closed));
}

} // namespace views::ui
