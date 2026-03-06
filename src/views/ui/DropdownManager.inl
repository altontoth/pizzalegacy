/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <magic_enum/magic_enum.hpp>
#include "views/base/ViewDefs.h"

template <typename DropdownType>
void DropdownManager<DropdownType>::add_dropdown(
    DropdownType type, ViewRect header, ViewPoint origin,
    std::optional<std::function<void()>> on_activate) {
  dropdowns_.emplace(type, DropdownList(header, origin, on_activate));
}

template <typename DropdownType>
void DropdownManager<DropdownType>::add_item(DropdownType type,
                                             std::string_view text,
                                             std::function<void()> on_click) {
  dropdowns_.at(type).add_entry(text, std::move(on_click));
}

template <typename DropdownType> bool DropdownManager<DropdownType>::tick() {
  bool something_changed = false;
  for (auto& [type, dropdown] : dropdowns_) {
    something_changed |= dropdown.tick();
  }
  return something_changed;
}

template <typename DropdownType>
void DropdownManager<DropdownType>::render(Surface& target_surface,
                                           FontManager* font) {
  for (auto& [type, dropdown] : dropdowns_) {
    dropdown.render(target_surface, font);
  }
}

template <typename DropdownType>
void DropdownManager<DropdownType>::add_interactions(
    views::ui::InteractionContext& ui_ctx) {

  // backdrop if someone is active
  if (active_) {
    auto& dd = dropdowns_.at(*active_);
    ui_ctx.layers.create_frame_item(
        ViewRect{0, 0, views::base::MAIN_PANEL_WIDTH,
                 views::base::MAIN_PANEL_HEIGHT},
        "dd_backdrop",
        [this, inv = ui_ctx.invalidate](const views::ui::InteractionEvent& ev) {
          if (ev.type == views::ui::InteractionEvent::Type::ButtonDown &&
              ev.button == views::ui::MouseButton::Left) {
            dropdowns_.at(*active_).expanded(false);
            active_.reset();
            inv();
          }
        });

    // Call add_interactions with parent and an on-click lambda to
    // tell us to close the dropdown box when something has been clicked.
    dd.add_interactions(ui_ctx,
                        [this, key = *active_, inv = ui_ctx.invalidate] {
                          dropdowns_.at(key).expanded(false);
                          active_.reset();
                          inv();
                        });
  }
  else {
    // Draw the headers
    for (auto& [key, dd] : dropdowns_) {
      ui_ctx.layers.create_frame_item(
          dropdowns_.at(key).header(), magic_enum::enum_name(key),
          [this, key, inv = ui_ctx.invalidate](views::ui::InteractionEvent ui) {
            if (ui.type != views::ui::InteractionEvent::Type::Click) {
              return;
            }
            dropdowns_.at(key).expanded(true);
            active_ = key;
            inv();
          });
    }
  }
}
