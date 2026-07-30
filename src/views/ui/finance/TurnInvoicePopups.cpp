/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/finance/TurnInvoicePopups.h"

#include <cassert>
#include <optional>
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
#include "engine/ui/Alignment.h"
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

void show_electricity_slot_invoice_popup(GameContext& context, GameState& gs,
                                         core::RestaurantId restaurant_id,
                                         std::function<void()> after) {
  auto* rest = gs.restaurant(restaurant_id);
  assert(rest);

  auto owner_id = rest->property().owner_or_tenant();
  assert(owner_id);

  auto* player = gs.player(*owner_id);
  assert(player);

  auto invoice = core::finance::collect_electricity_slot_invoice(*rest);

  InvoiceConfig cfg;
  cfg.title = "Invoice";
  cfg.subtitle = "FOR " + rest->name();
  cfg.lines.push_back(
      {std::string(gs.time().current_date().month_name()), std::nullopt});
  cfg.lines.push_back(empty_line);
  cfg.lines.push_back({"ELECTRICITY:", invoice.electricity});
  cfg.lines.push_back(empty_line);
  cfg.lines.push_back({"SLOT MACHINES", {}, AlignX::Left});
  cfg.lines.push_back({"INCOME:", invoice.slot_income});
  cfg.lines.push_back(empty_line);
  cfg.lines.push_back({"TOTAL:", invoice.total});

  cfg.on_dismiss = [&gs, player, invoice, after = std::move(after)]() {
    core::finance::apply_electricity_slot_invoice(gs, *player, invoice);
    if (after) {
      after();
    }
  };

  context.popup->origin(invoice_popup_origin);
  context.popup->create<InvoicePopup>(std::move(cfg));
}

void show_monthly_invoice_popup(GameContext& context, GameState& gs,
                                core::PlayerId player_id,
                                std::function<void()> on_done) {
  auto* player = gs.player(player_id);
  if (!player) {
    return;
  }

  auto costs = core::finance::collect_monthly_costs(gs, *player);

  InvoiceConfig cfg;
  cfg.subtitle = "for " + player->name();
  cfg.lines.push_back({"costs per month", std::nullopt});
  cfg.lines.push_back(empty_line);

  // TODO: Figure out investments and which is which
  cfg.lines.push_back({"LOAN REPAYMENT:", costs.loan_repayment});
  cfg.lines.push_back({"LOAN INTEREST:", costs.loan_interest});
  cfg.lines.push_back({"RENT:", costs.total_rent});
  cfg.lines.push_back({"INVESTMENT RETURNS:", costs.investment_returns});
  cfg.lines.push_back(empty_line);
  cfg.lines.push_back({"TOTAL:", costs.total});

  cfg.on_dismiss = [&gs, player, costs = std::move(costs),
                    on_done = std::move(on_done)]() {
    core::finance::apply_monthly_costs(gs, *player, costs);
    if (on_done) {
      on_done();
    }
  };

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
  cfg.lines.push_back({"week's profit", std::nullopt});
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
  std::function<void()> profit_cb = [&context, &gs, previous_player_id,
                                     on_done = std::move(on_done)] {
    show_weekly_city_invoice_popup(context, gs, previous_player_id, on_done);
  };
  for (auto it = restaurants.rbegin(); it != restaurants.rend(); ++it) {
    if (!*it || (*it)->property().city().id() != gs.active_city()->id()) {
      continue;
    }
    std::function<void()> cb = std::move(profit_cb);
    if (interleave_electricity) {
      auto rest_id = (*it)->id();
      cb = [&context, &gs, rest_id, next = std::move(cb)]() mutable {
        show_electricity_slot_invoice_popup(context, gs, rest_id,
                                            std::move(next));
      };
    }
    if ((*it)->wants_open() && !(*it)->is_force_closed()) {
      gs.ui().push<WeeksTakingsState>(
          WeeksTakingsState{(*it)->id(), true, std::move(cb)});
    }
    else {
      show_staff_invoice_popup(context, gs, (*it)->id(), std::move(cb));
    }
  }
  if (profit_cb) {
    profit_cb();
  }
}

} // namespace views::ui::finance
