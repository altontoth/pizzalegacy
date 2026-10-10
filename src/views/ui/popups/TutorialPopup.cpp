/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/popups/TutorialPopup.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

#include "engine/gfx/Draw.h"
#include "engine/gfx/PaletteHandle.h"
#include "engine/input/InputManager.h"
#include "engine/ui/InteractionEvent.h"
#include "resources/FontManager.h"
#include "resources/FontTypes.h"

namespace {
constexpr std::string_view title_label = "Tutorial";
constexpr FontFace text_face = FontFace::PIZ_NUMB;
constexpr int chevron_half_width = 6;
constexpr int chevron_half_height = 3;
} // namespace

TutorialPopup::TutorialPopup(const GameContext& context, std::string text,
                             std::string ok_label, Callbacks callbacks)
    : BasePopup(context), text_(std::move(text)), has_ok_(!ok_label.empty()),
      callbacks_(std::move(callbacks)), ok_({}, ok_label, [this] {
        if (callbacks_.on_ok) {
          callbacks_.on_ok();
        }
      }) {
  ok_.id(std::string{ok_item});
  ok_.face(text_face);

  const int ok_width = has_ok_ ? button_width(ok_label) : 0;
  width_ = fit_width(ok_width + 2 * button_margin);

  const int text_inset = border + text_margin;
  const auto metrics =
      font_->size_text(text_, text_face, width_ - 2 * text_inset,
                       TextStyle{.color = text_color});
  const int text_bottom = title_height + text_margin + metrics.height;
  if (has_ok_) {
    const int button_y = text_bottom + text_to_button_gap;
    height_ = button_y + button_height + button_margin;
    ok_.set_rect(
        {width_ - button_margin - ok_width, button_y, ok_width, button_height});
    add_shortcut(input::Action::SubmitInput, &ok_);
  }
  else {
    height_ = text_bottom + text_margin + border;
  }
}

int TutorialPopup::fit_width(int buttons_width) const {
  const TextStyle style{.color = text_color};
  for (int width = min_width; width < max_width; width += width_step) {
    if (width >= buttons_width &&
        font_->count_wrapped_lines(text_, text_face,
                                   width - 2 * (border + text_margin),
                                   style) <= max_lines) {
      return width;
    }
  }
  return max_width;
}

int TutorialPopup::button_width(const std::string& label) const {
  const auto metrics = font_->size_text(label, text_face);
  return std::max(min_button_width, metrics.width + button_padding);
}

ViewRect TutorialPopup::chevron_rect(int width) {
  return {width - title_height, 0, title_height, title_height};
}

Surface TutorialPopup::chrome_surface(Size size, Chevron chevron) const {
  Surface s{size.w, size.h, gfx::PaletteHandle{0}};
  s.fill_rect({0, 0, size.w, size.h}, fill_color);
  s.fill_rect({0, 0, size.w, title_height}, border_color);
  gfx::draw::box(s, ViewRect{0, 0, size.w, size.h}, border_color, border);

  const auto metrics = font_->size_text(title_label, FontFace::CSTANDAR);
  font_->render_text_onto(s, title_label,
                          {title_margin, (title_height - metrics.height) / 2},
                          FontFace::CSTANDAR, title_color);

  const ViewRect rect = chevron_rect(size.w);
  const ViewPoint center{rect.x + rect.w / 2, rect.y + rect.h / 2};
  const int tip =
      chevron == Chevron::Down ? chevron_half_height : -chevron_half_height;
  const ViewPoint left{center.x - chevron_half_width, center.y - tip};
  const ViewPoint point{center.x, center.y + tip};
  const ViewPoint right{center.x + chevron_half_width, center.y - tip};
  const auto color = chevron_hovered_ ? chevron_hover_color : title_color;
  gfx::draw::line(s, left, point, color, 2);
  gfx::draw::line(s, point, right, color, 2);
  return s;
}

void TutorialPopup::render(Surface& surface, ViewPoint origin) {
  auto s = chrome_surface({width_, height_}, Chevron::Down);
  const int text_inset = border + text_margin;
  font_->render_into_rect(s, text_,
                          {text_inset, title_height + text_margin,
                           width_ - 2 * text_inset, height_},
                          text_face, TextStyle{.color = text_color});
  if (has_ok_) {
    ok_.render_with(*this, s);
  }
  surface.blit_raw(s, origin);
}

void TutorialPopup::add_interactions(views::ui::InteractionContext ictx,
                                     ViewPoint origin) {
  if (has_ok_) {
    ok_.add_interactions(ictx, origin);
  }
  ictx.layers.create_frame_item(
      add_origin(chevron_rect(width_), origin), hide_item,
      [this, inv = ictx.invalidate](const views::ui::InteractionEvent& ev) {
        using Type = views::ui::InteractionEvent::Type;
        if (ev.type == Type::Enter || ev.type == Type::Leave) {
          chevron_hovered_ = ev.type == Type::Enter;
          inv();
        }
        else if (ev.type == Type::Click &&
                 ev.button == views::ui::MouseButton::Left) {
          chevron_hovered_ = false;
          if (callbacks_.on_hide) {
            callbacks_.on_hide();
          }
        }
      });
}

Size TutorialPopup::tab_size() const {
  const auto metrics = font_->size_text(title_label, FontFace::CSTANDAR);
  return {title_margin + metrics.width + title_height, title_height};
}

void TutorialPopup::render_tab(Surface& surface, ViewPoint origin) {
  const Size size = tab_size();
  auto s = chrome_surface(size, Chevron::Up);
  gfx::draw::box(s, ViewRect{0, 0, size.w, size.h}, fill_color);
  surface.blit_raw(s, origin);
}

bool TutorialPopup::reaches_screen_while_filtered(input::Action action) {
  switch (action) {
  case input::Action::ToggleConsole:
  case input::Action::ToggleDebug:
  case input::Action::TogglePause:
  case input::Action::ToggleFullscreen:
  case input::Action::TakeScreenshot:
  case input::Action::TimeScale1:
  case input::Action::TimeScale2:
  case input::Action::TimeScale3:
  case input::Action::TimeScale5:
  case input::Action::TimeScale10:
  // The console's line editing.
  case input::Action::SubmitInput:
  case input::Action::Backspace:
  case input::Action::EraseLine:
  case input::Action::CancelInput:
    return true;
  default:
    return false;
  }
}

bool TutorialPopup::on_action(const input::ActionEvent& ev) {
  if (!callbacks_.takes_input()) {
    return false;
  }
  if (!callbacks_.minimized() && BasePopup::on_action(ev)) {
    return true;
  }
  if (!callbacks_.filtered()) {
    return false;
  }
  if (ev.action == input::Action::Exit) {
    if (ev.state.pressed && callbacks_.on_skip) {
      callbacks_.on_skip();
    }
    return true;
  }
  return !reaches_screen_while_filtered(ev.action);
}
