/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/finance/TurnInvoicePopups.h"

#include <cassert>
#include <optional>
#include <ranges>
#include <string>
#include <utility>

#include "core/character/Player.h"
#include "core/city/CityNames.h"
#include "core/city/Property.h"
#include "core/finance/TurnInvoices.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "core/util/GameContext.h"
#include "engine/ui/TextAnchor.h"
#include "views/ui/PopupManager.h"
#include "views/ui/finance/FinanceComponents.h"
#include "views/ui/popups/InvoicePopup.h"

namespace views::ui::finance {

namespace {
constexpr ViewPoint invoice_popup_origin{74, 40};
const InvoiceLine empty_line{.label = "", .value = std::nullopt};
} // namespace

void show_staff_invoice_popup(GameContext& context, GameState& gs,
                              core::RestaurantId restaurant_id,
                              std::function<void()> after) {
  auto* rest = gs.restaurant(restaurant_id);
  if (!rest) {
    return;
  }

  auto wages = core::finance::collect_staff_wages(*rest);

  InvoiceConfig cfg;
  cfg.subtitle = "for " + rest->name();
  cfg.on_dismiss = [&gs, restaurant_id, after = std::move(after)] {
    gs.deduct_staff_wages_for_restaurant(restaurant_id);
    if (after) {
      after();
    }
  };

  cfg.lines.push_back({"staff expenses", std::nullopt});
  cfg.lines.push_back(empty_line);
  for (const auto& entry : wages.entries) {
    cfg.lines.push_back({entry.name, entry.payment});
    cfg.lines.push_back(empty_line);
  }
  cfg.lines.push_back({"", wages.total});

  context.popup->origin(invoice_popup_origin);
  context.popup->create<InvoicePopup>(std::move(cfg));
}

void show_electricity_slot_invoice_popup(
    GameContext& context, GameState& gs,
    const core::finance::ElectricitySlotInvoice& invoice,
    std::function<void()> after) {
  auto* rest = gs.restaurant(invoice.restaurant_id);
  assert(rest);

  InvoiceConfig cfg;
  cfg.title = "Invoice";
  cfg.subtitle = "FOR " + rest->name();
  cfg.lines.push_back(
      {std::string(gs.time().current_date().month_name()), std::nullopt});
  cfg.lines.push_back(empty_line);
  cfg.lines.push_back({"ELECTRICITY:", invoice.electricity});
  cfg.lines.push_back(empty_line);
  cfg.lines.push_back({"SLOT MACHINES", {}, TextAnchor::Start});
  cfg.lines.push_back({"INCOME:", invoice.slot_income});
  cfg.lines.push_back(empty_line);
  cfg.lines.push_back({"TOTAL:", invoice.total});

  cfg.on_dismiss = std::move(after);

  context.popup->origin(invoice_popup_origin);
  context.popup->create<InvoicePopup>(std::move(cfg));
}

void show_monthly_invoice_popup(GameContext& context, GameState& gs,
                                core::PlayerId player_id,
                                const core::finance::MonthlyCosts& costs,
                                std::function<void()> on_done) {
  auto* player = gs.player(player_id);
  if (!player) {
    return;
  }

  InvoiceConfig cfg;
  cfg.subtitle = "for " + player->name();
  cfg.lines.push_back({"costs per month", std::nullopt});
  cfg.lines.push_back(empty_line);

  // The original reuses the loan captions for the two investment lines.
  const bool fix_captions = gs.rules().fix_legacy_bugs;
  cfg.lines.push_back(
      {fix_captions ? "LOAN REPAYMENT:" : "REPAYMENT:", costs.loan_repayment});
  cfg.lines.push_back(
      {fix_captions ? "LOAN INTEREST:" : "INTEREST:", costs.loan_interest});
  cfg.lines.push_back({"INSURANCE:", costs.insurance_premiums});
  cfg.lines.push_back({fix_captions ? "INVESTMENT RETURN:" : "LOAN INTEREST:",
                       costs.investment_returns});
  cfg.lines.push_back({fix_captions ? "INVESTMENT PAYOUT:" : "REPAYMENT:",
                       costs.investment_payout});
  cfg.lines.push_back({"RENT:", costs.total_rent});
  cfg.lines.push_back(empty_line);
  cfg.lines.push_back({"TOTAL:", costs.total});

  cfg.on_dismiss = std::move(on_done);

  context.popup->origin(invoice_popup_origin);
  context.popup->create<InvoicePopup>(std::move(cfg));
}

void show_weekly_city_invoice_popup(GameContext& context, GameState& gs,
                                    core::PlayerId player_id,
                                    std::function<void()> on_done) {
  auto* player = gs.player(player_id);
  if (!player) {
    return;
  }

  auto profits = core::finance::collect_weekly_city_profits(gs, *player);

  InvoiceConfig cfg;
  cfg.subtitle = "for " + player->name();
  cfg.lines.push_back({gs.options().qol_improvements
                           ? "profits while away"
                           : "week's profit", std::nullopt});
  cfg.lines.push_back(empty_line);
  for (const auto& entry : profits.entries) {
    cfg.lines.push_back(
        {std::string(core::city_name(entry.city_id)), entry.profit});
  }
  cfg.lines.push_back(empty_line);
  cfg.lines.push_back({"", profits.total});

  cfg.on_dismiss = [&gs, player, profits = std::move(profits),
                    on_done = std::move(on_done)]() {
    core::finance::apply_weekly_city_profits(gs, *player, profits);
    if (on_done) {
      on_done();
    }
  };

  context.popup->origin(invoice_popup_origin);
  context.popup->create<InvoicePopup>(std::move(cfg));
}

void push_weekly_restaurant_reports(GameContext& context, GameState& gs,
                                    const std::vector<Restaurant*>& restaurants,
                                    core::PlayerId previous_player_id,
                                    std::function<void()> on_done,
                                    bool interleave_electricity) {
  // The UI stack is LIFO, so build the report backwards to show restaurants
  // in the same order as the restaurant list. Closed restaurants don't go
  // on the stack, so their popup is added to the tail instead.
  std::function<void()> tail = [&context, &gs, previous_player_id,
                                on_done = std::move(on_done)] {
    show_weekly_city_invoice_popup(context, gs, previous_player_id, on_done);
  };
  for (auto* restaurant : std::ranges::reverse_view(restaurants)) {
    if (!restaurant ||
        restaurant->property().city().id() != gs.active_city()->id()) {
      continue;
    }
    auto rest_id = restaurant->id();
    if (interleave_electricity) {
      tail = [&context, &gs, rest_id, next = std::move(tail)]() mutable {
        auto* rest = gs.restaurant(rest_id);
        auto owner = rest ? rest->property().owner_or_tenant() : std::nullopt;
        if (!owner) {
          if (next) {
            next();
          }
          return;
        }
        auto invoice = core::finance::collect_electricity_slot_invoice(*rest);
        show_electricity_slot_invoice_popup(
            context, gs, invoice,
            [&gs, invoice, owner = *owner, next = std::move(next)]() mutable {
              if (auto* player = gs.player(owner)) {
                core::finance::apply_electricity_slot_invoice(gs, *player,
                                                              invoice);
              }
              if (next) {
                next();
              }
            });
      };
    }
    if (restaurant->wants_open() && !restaurant->is_force_closed()) {
      gs.ui().push<WeeksTakingsState>(WeeksTakingsState{
          .id = rest_id, .end_of_week_mode = true, .on_done = std::move(tail)});
      tail = {};
    }
    else {
      tail = [&context, &gs, rest_id, next = std::move(tail)]() mutable {
        show_staff_invoice_popup(context, gs, rest_id, std::move(next));
      };
    }
  }
  // There may still be closed-restaurant popups left in the tail.
  if (tail) {
    tail();
  }
}

} // namespace views::ui::finance
