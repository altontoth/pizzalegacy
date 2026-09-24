/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/popups/ContextMenuPopup.h"

#include "engine/DisplayConstants.h"
#include "engine/FormatCompat.h"
#include "engine/gfx/Draw.h"
#include "engine/gfx/PaletteHandle.h"
#include "engine/input/Action.h"
#include "engine/input/InputManager.h"
#include "engine/ui/InteractionEvent.h"
#include "resources/FontManager.h"

#include <algorithm>
#include <utility>

namespace {
constexpr int row_padding_x = 8;
constexpr int row_padding_y = 3;
constexpr int min_row_width = 120;

Surface frame_surface(int width, int height) {
  gfx::Surface s{width, height, gfx::PaletteHandle{0}};
  const ViewRect outer{0, 0, width, height};
  s.fill_rect(outer, 14);
  gfx::draw::box(s, outer, 11);
  gfx::draw::box(s, outer.inset(1, 1), 13);
  gfx::draw::box(s, outer.inset(2, 2), 15);
  gfx::draw::box(s, outer.inset(3, 3), 14);
  gfx::draw::box(s, outer.inset(4, 4), 13);
  return s;
}
} // namespace

ContextMenuPopup::ContextMenuPopup(const GameContext& context,
                                   std::vector<ContextMenuItem> items,
                                   ContextMenuStyle style)
    : BasePopup(context), items_(std::move(items)), style_(style) {
  int text_width = 0;
  int text_height = 0;
  for (const auto& item : items_) {
    const auto metrics = font_->size_text(item.text, style_.font);
    text_width = std::max(text_width, metrics.width);
    text_height = std::max(text_height, metrics.height);
  }
  row_height_ = text_height + (2 * row_padding_y);
  const int row_width =
      std::max(min_row_width, text_width + (2 * row_padding_x));
  const int rows = static_cast<int>(items_.size());
  size_ = {row_width + (2 * frame_border),
           (rows * row_height_) + (2 * frame_border)};
}

ViewRect ContextMenuPopup::row_rect(int i) const {
  return {frame_border, frame_border + (i * row_height_),
          size_.w - (2 * frame_border), row_height_};
}

void ContextMenuPopup::add_interactions(views::ui::InteractionContext ictx,
                                        ViewPoint origin) {
  using Type = views::ui::InteractionEvent::Type;

  const ViewRect frame{origin.x, origin.y, size_.w, size_.h};
  ictx.layers.create_frame_item(
      ViewRect{0, 0, DisplayConstants::SCREEN_WIDTH,
               DisplayConstants::SCREEN_HEIGHT},
      "context_menu_dismiss",
      [this, frame](const views::ui::InteractionEvent& ev) {
        if (ev.type == Type::ButtonDown && !frame.contains(ev.pos)) {
          finished();
        }
      },
      {.layer = views::ui::InteractionLayer::Exclusive});

  for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
    ictx.layers.create_frame_item(
        add_origin(row_rect(i), origin),
        compat::format("context_menu_row_{}", i),
        [this, i,
         inv = ictx.invalidate](const views::ui::InteractionEvent& ev) {
          switch (ev.type) {
          case Type::Enter:
            hovered_ = i;
            inv();
            break;
          case Type::Leave:
            if (hovered_ == i) {
              hovered_ = -1;
              inv();
            }
            break;
          case Type::Click:
            if (ev.button == views::ui::MouseButton::Left) {
              items_[i].callback();
              finished();
            }
            break;
          default:
            break;
          }
        });
  }
}

void ContextMenuPopup::render(Surface& surface, ViewPoint origin) {
  auto s = frame_surface(size_.w, size_.h);
  for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
    const ViewRect row = row_rect(i);
    gfx::ColorIndex color = style_.text_color;
    if (i == hovered_) {
      s.fill_rect(row, style_.hover_fill);
      color = style_.hover_text_color;
    }
    font_->render_text_onto(
        s, items_[i].text,
        ViewPoint{row.x + row_padding_x, row.y + row_padding_y}, style_.font,
        TextStyle{.color = color});
  }
  surface.blit_raw(s, origin);
}

bool ContextMenuPopup::on_action(const input::ActionEvent& ev) {
  if (ev.action == input::Action::Exit) {
    if (ev.state.pressed) {
      finished();
    }
    return true;
  }
  return BasePopup::on_action(ev);
}
