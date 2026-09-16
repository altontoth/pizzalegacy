/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/widgets/SliderMenuItem.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>

#include "engine/FormatCompat.h"
#include "engine/Size.h"
#include "engine/gfx/Draw.h"
#include "resources/FontManager.h"

namespace {
constexpr uint8_t COLOR_TRACK_FILLED = 240;
constexpr uint8_t COLOR_TRACK_EMPTY = 198;
constexpr uint8_t COLOR_HANDLE = 14;
constexpr uint8_t COLOR_HANDLE_HOVER = 29;
constexpr uint8_t COLOR_HANDLE_GRAB = 15;
constexpr uint8_t COLOR_HANDLE_RING = 240;

// Track start as an offset from the row origin. This way the track lines up
// with ToggleMenuItem's value column (x coord 400) at the options menu
// origin (x coord 120).
constexpr int TRACK_LOCAL_X = 280;
constexpr int TRACK_WIDTH = 132;
constexpr int TRACK_THICKNESS = 6;
constexpr int HANDLE_RADIUS = 8;
constexpr int ROW_HEIGHT = (2 * HANDLE_RADIUS) + 2;
// Gap between the track's right end and the value text.
constexpr int VALUE_GAP = 10;
// Row-local x where the value text starts.
constexpr int VALUE_LOCAL_X =
    TRACK_LOCAL_X + TRACK_WIDTH + HANDLE_RADIUS + VALUE_GAP;
} // anonymous namespace

SliderMenuItemBase::SliderMenuItemBase(GameContext& context,
                                       const std::string& text,
                                       FontFace font_face)
    : MenuItem(context, font_face, text) {}

void SliderMenuItemBase::draw(Surface& screen, ViewPoint p, bool is_selected) {
  context_.font->render_text_onto(screen, text_, p, font_face_, idle_color(),
                                  TextAnchor::Start);

  const int track_x = p.x + TRACK_LOCAL_X;
  const int cy = p.y + (ROW_HEIGHT / 2);
  const int track_y = cy - (TRACK_THICKNESS / 2);
  const float frac = std::clamp(fraction(), 0.0f, 1.0f);
  const int handle_x =
      track_x + static_cast<int>(std::lround(frac * TRACK_WIDTH));

  if (handle_x > track_x) {
    screen.fill_rect({track_x, track_y, handle_x - track_x, TRACK_THICKNESS},
                     COLOR_TRACK_FILLED);
  }
  if (handle_x < track_x + TRACK_WIDTH) {
    screen.fill_rect({handle_x, track_y, (track_x + TRACK_WIDTH) - handle_x,
                      TRACK_THICKNESS},
                     COLOR_TRACK_EMPTY);
  }

  uint8_t handle_color = COLOR_HANDLE;
  if (track_drag_.dragging()) {
    handle_color = COLOR_HANDLE_GRAB;
  }
  else if (is_selected) {
    handle_color = COLOR_HANDLE_HOVER;
  }
  const ViewPoint center{handle_x, cy};
  gfx::draw::fill_circle(screen, center, HANDLE_RADIUS, COLOR_HANDLE_RING);
  gfx::draw::fill_circle(screen, center, HANDLE_RADIUS - 2, handle_color);

  context_.font->render_text_onto(screen, value_text(),
                                  ViewPoint{p.x + VALUE_LOCAL_X, p.y},
                                  font_face_, idle_color(), TextAnchor::Start);
}

Size SliderMenuItemBase::measure() const {
  RenderedTextMetrics label = context_.font->size_text(text_, font_face_);
  RenderedTextMetrics value =
      context_.font->size_text(widest_value_text(), font_face_);
  return Size{VALUE_LOCAL_X + value.width, std::max(label.height, ROW_HEIGHT)};
}

void SliderMenuItemBase::handle_action() {}

bool SliderMenuItemBase::on_adjust(AdjustAxis axis, int delta) {
  if (axis != AdjustAxis::Horizontal) {
    return false;
  }
  step(delta);
  return true;
}

bool SliderMenuItemBase::on_interaction(views::ui::InteractionEvent ie) {
  // Menu::update_interactions rewrites ie.pos to be local to the row rect,
  // whose origin is the row origin the track is drawn relative to. The
  // padding keeps the disc grabbable even when at min or max position.
  const TrackDrag::Track track{
      .start = TRACK_LOCAL_X, .length = TRACK_WIDTH, .padding = HANDLE_RADIUS};

  if (ie.type == views::ui::InteractionEvent::Type::Scroll) {
    if (ie.scroll_delta == 0 || !track.contains(ie.pos.x)) {
      return false;
    }
    step(ie.scroll_delta > 0 ? 1 : -1);
    return true;
  }

  const auto result = track_drag_.on_interaction(ie, ie.pos.x, track);
  if (result.fraction) {
    set_fraction(*result.fraction);
  }
  return result.handled;
}

SliderMenuItem::SliderMenuItem(GameContext& context, const std::string& text,
                               float& value,
                               std::function<void(float)> on_change,
                               FontFace font_face)
    : SliderMenuItemBase(context, text, font_face), value_(value),
      on_change_(std::move(on_change)) {}

float SliderMenuItem::fraction() const {
  return std::clamp(value_, 0.0f, 1.0f);
}

void SliderMenuItem::set_fraction(float frac) {
  value_ = frac;
  if (on_change_) {
    on_change_(frac);
  }
}

void SliderMenuItem::step(int delta) {
  constexpr float KEY_STEP = 0.05f;
  set_fraction(
      std::clamp(value_ + (static_cast<float>(delta) * KEY_STEP), 0.0f, 1.0f));
}

std::string SliderMenuItem::value_text() const {
  const int pct =
      static_cast<int>(std::lround(std::clamp(value_, 0.0f, 1.0f) * 100));
  return compat::format("({}%)", pct);
}

std::string SliderMenuItem::widest_value_text() const { return "(100%)"; }
