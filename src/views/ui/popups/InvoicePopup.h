/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "views/ui/finance/FinanceComponents.h"
#include "views/ui/popups/BasePopup.h"

class InvoicePopup : public BasePopup {
public:
  using callback_t = std::function<void(void)>;

  InvoicePopup(const GameContext& ctx, views::ui::finance::InvoiceConfig cfg);
  void add_interactions(views::ui::InteractionContext ictx,
                        ViewPoint origin) override;
  void render(Surface& surface, ViewPoint origin) override;

private:
  views::ui::finance::InvoiceConfig cfg_;
  callback_t on_dismiss_;
};
