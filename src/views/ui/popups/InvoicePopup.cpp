/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/popups/InvoicePopup.h"

#include <utility>

InvoicePopup::InvoicePopup(const GameContext& ctx,
                           views::ui::finance::InvoiceConfig cfg)
    : BasePopup(ctx), cfg_(std::move(cfg)),
      on_dismiss_(std::move(cfg_.on_dismiss)) {}

void InvoicePopup::add_interactions(views::ui::InteractionContext ictx,
                                    ViewPoint /* origin */) {
  // Click anywhere dismisses
  ViewRect click_area{0, 0, DisplayConstants::SCREEN_WIDTH,
                      DisplayConstants::SCREEN_HEIGHT};
  ictx.layers.create_frame_item(
      click_area, "invoice_popup", [this](views::ui::InteractionEvent ie) {
        if (ie.button != views::ui::MouseButton::Left ||
            ie.type != views::ui::InteractionEvent::Type::ButtonDown) {
          return;
        }
        if (on_dismiss_) {
          on_dismiss_();
        }
        finished();
      });
}

void InvoicePopup::render(Surface& surface, ViewPoint origin) {
  auto invoice = views::ui::finance::render_invoice(*font_, cfg_);
  surface.blit_raw(invoice, origin);
}
