/*
 * SPDX-FileCopyrightText: 2019 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/popups/PopupButton.h"

// Register interactions for this frame (rect is local; pass popup origin)
void PopupButton::add_interactions(views::ui::InteractionContext ctx,
                                   ViewPoint popup_origin) {

  // If we don't have a string we just hash the rect to make sure we have
  // something unique
  const std::string btn_id =
      !id_.empty()
          ? id_
          : "button_" + (text_.empty()
                             ? std::to_string(std::hash<ViewRect>{}(rect_))
                             : text_);
  ctx.layers.create_frame_item(
      add_origin(rect_, popup_origin), btn_id,
      [this, btn_id,
       inv = ctx.invalidate](const views::ui::InteractionEvent& ev) {
        bool dirty = false;

        switch (ev.type) {
        case views::ui::InteractionEvent::Type::Enter:
          if (!hovered_) {
            hovered_ = true;
            dirty = true;
          }
          break;

        case views::ui::InteractionEvent::Type::Leave:
          if (hovered_ || pressed_) {
            hovered_ = false;
            pressed_ = false;
            dirty = true;
          }
          break;

        case views::ui::InteractionEvent::Type::ButtonDown:
          if (ev.button == views::ui::MouseButton::Left) {
            if (!pressed_) {
              pressed_ = true;
              dirty = true;
            }
          }
          break;

        case views::ui::InteractionEvent::Type::Click:
          // case views::ui::InteractionEvent::Type::DoubleClick:
          //  Only sent if down+up occurred on this item
          if (on_click_) {
            on_click_();
          }
          pressed_ = false;
          dirty = true;
          break;

        case views::ui::InteractionEvent::Type::ButtonUp:
        case views::ui::InteractionEvent::Type::Cancelled:
          if (pressed_) {
            pressed_ = false;
            dirty = true;
          }
          break;

        default:
          break;
        }

        if (dirty) {
          inv();
        }
      });
}
