/*
 * SPDX-FileCopyrightText: 2013 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/widgets/Button.h"

Button::Button(VgaManager* vga, Icon icon, ButtonType type, ViewRect pos,
               std::string tooltip, std::function<void()> on_click,
               std::function<void()> mark_dirty)
    : pos_(pos), tooltip_(std::move(tooltip)), on_click_(std::move(on_click)),
      mark_dirty_(std::move(mark_dirty)),
      image_(make_button_surface(vga, icon, type, pos, false)),
      pressed_image_(make_button_surface(vga, icon, type, pos, true)) {}

void Button::draw(Surface& s, const ViewPoint& origin) {
  s.blit_raw(button_pressed_ ? pressed_image_ : image_,
             add_origin(pos_.point(), origin));
}

const std::string& Button::tooltip() const { return tooltip_; }

Surface Button::make_button_surface(VgaManager* vga, Icon icon, ButtonType type,
                                    ViewRect /* pos */, bool for_pressed) {
  Size s = ButtonGeometry::size(type);
  Surface out{s};

  const auto& button_blitmap = vga->get("gfx/permap");
  const auto& icons = vga->get("gfx/icons");

  // figure out which slice of the button sprite to draw
  ViewRect bg_loc{};
  bool pressed_offset_vertical = false;
  switch (type) {
  case ButtonType::Square:
    bg_loc = {108, 100, s.w, s.h};
    break;
  case ButtonType::Horizontal:
    bg_loc = {0, 0, s.w, s.h};
    break;
  case ButtonType::Vertical:
    bg_loc = {0, 100, s.w, s.h};
    pressed_offset_vertical = true;
    break;
  }

  // draw the non-pressed frame
  out.blit(button_blitmap, bg_loc);

  // if we're building the pressed image, offset and draw again
  if (for_pressed) {
    if (pressed_offset_vertical) {
      bg_loc.x += s.w;
    }
    else {
      bg_loc.y += s.h;
    }
    out.blit(button_blitmap, bg_loc);
  }

  ViewRect icon_loc{(static_cast<int>(icon) % ICONS_PER_ROW) * ICON_WIDTH,
                    (static_cast<int>(icon) / ICONS_PER_ROW) * ICON_HEIGHT,
                    ICON_WIDTH, ICON_HEIGHT};

  // figure out where inside the button to blit the icon
  ViewPoint dst{12, 14};
  if (type == ButtonType::Horizontal) {
    dst.x += 30;
  }
  if (type == ButtonType::Vertical) {
    dst.y += 28;
  }

  if (for_pressed) {
    dst.y -= 2;
    out.blit(icons, icon_loc, dst);
  }
  else {
    out.blit(icons, icon_loc, dst);
  }

  return out;
}

void Button::on_interaction(views::ui::InteractionEvent ie) {
  switch (ie.type) {
  case views::ui::InteractionEvent::Type::ButtonDown:
    button_pressed_ = true;
    mark_dirty_();
    break;
  case views::ui::InteractionEvent::Type::ButtonUp:
    button_pressed_ = false;
    mark_dirty_();
    break;
  case views::ui::InteractionEvent::Type::Click:
    mark_dirty_();
    on_click_();
    break;
  default:
    return;
  }
}
