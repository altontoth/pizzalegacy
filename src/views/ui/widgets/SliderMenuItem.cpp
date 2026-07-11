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
#include "gfx/Draw.h"
#include "resources/FontManager.h"

namespace {
constexpr uint8_t COLOR_LABEL = 240;
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
// Gap between the track's right end and the percentage.
constexpr int PCT_GAP = 10;
// Row-local x where the percentage text starts.
constexpr int PCT_LOCAL_X =
    TRACK_LOCAL_X + TRACK_WIDTH + HANDLE_RADIUS + PCT_GAP;

int percent_of(float value) {
  return static_cast<int>(std::lround(std::clamp(value, 0.0f, 1.0f) * 100));
}
} // namespace

SliderMenuItem::SliderMenuItem(GameContext& context, const std::string& text,
                               float& value,
                               std::function<void(float)> on_change,
                               FontFace font_face)
    : MenuItem(context, font_face, text), value_(value),
      on_change_(std::move(on_change)) {}

void SliderMenuItem::draw(Surface& screen, ViewPoint p, bool is_selected) {
  context_.font->render_text_onto(screen, text_, p, font_face_, COLOR_LABEL,
                                  AlignX::Left);

  const int track_x = p.x + TRACK_LOCAL_X;
  const int cy = p.y + (ROW_HEIGHT / 2);
  const int track_y = cy - (TRACK_THICKNESS / 2);
  const float frac = std::clamp(value_, 0.0f, 1.0f);
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
  if (dragging_) {
    handle_color = COLOR_HANDLE_GRAB;
  }
  else if (is_selected) {
    handle_color = COLOR_HANDLE_HOVER;
  }
  const ViewPoint center{handle_x, cy};
  gfx::draw::fill_circle(screen, center, HANDLE_RADIUS, COLOR_HANDLE_RING);
  gfx::draw::fill_circle(screen, center, HANDLE_RADIUS - 2, handle_color);

  const std::string pct = compat::format("({}%)", percent_of(value_));
  context_.font->render_text_onto(screen, pct,
                                  ViewPoint{p.x + PCT_LOCAL_X, p.y}, font_face_,
                                  COLOR_LABEL, AlignX::Left);
}

Size SliderMenuItem::measure() const {
  RenderedTextMetrics label = context_.font->size_text(text_, font_face_);
  // Reserve room for the widest value so the row width stays stable.
  RenderedTextMetrics pct = context_.font->size_text("(100%)", font_face_);
  return Size{PCT_LOCAL_X + pct.width, std::max(label.height, ROW_HEIGHT)};
}

void SliderMenuItem::handle_action() {}

bool SliderMenuItem::on_adjust(AdjustAxis axis, int delta) {
  if (axis != AdjustAxis::Horizontal) {
    return false;
  }
  constexpr float KEY_STEP = 0.05f;
  value_ =
      std::clamp(value_ + (static_cast<float>(delta) * KEY_STEP), 0.0f, 1.0f);
  if (on_change_) {
    on_change_(value_);
  }
  return true;
}

bool SliderMenuItem::point_on_track(int local_x) {
  // Pad by the handle radius so the disc stays grabbable even when at min or
  // max position.
  return local_x >= TRACK_LOCAL_X - HANDLE_RADIUS &&
         local_x <= TRACK_LOCAL_X + TRACK_WIDTH + HANDLE_RADIUS;
}

void SliderMenuItem::seek_to(int local_x) {
  const float frac = std::clamp(
      static_cast<float>(local_x - TRACK_LOCAL_X) / TRACK_WIDTH, 0.0f, 1.0f);
  value_ = frac;
  if (on_change_) {
    on_change_(frac);
  }
}

bool SliderMenuItem::on_interaction(views::ui::InteractionEvent ie) {
  using Type = views::ui::InteractionEvent::Type;
  // Menu::update_interactions rewrites ie.pos to be local to the row rect,
  // whose origin is the row origin the track is drawn relative to.
  const int local_x = ie.pos.x;

  switch (ie.type) {
  case Type::ButtonDown:
    if (ie.button != views::ui::MouseButton::Left || !point_on_track(local_x)) {
      armed_ = false;
      return false;
    }
    // Click-to-seek: jump immediately, and arm a possible drag.
    armed_ = true;
    seek_to(local_x);
    return true;
  case Type::DragStart:
    if (!armed_) {
      return false;
    }
    dragging_ = true;
    seek_to(local_x);
    return true;
  case Type::DragMove:
    if (!dragging_) {
      return false;
    }
    seek_to(local_x);
    return true;
  case Type::DragEnd:
    if (!dragging_ && !armed_) {
      return false;
    }
    dragging_ = false;
    armed_ = false;
    return true;
  default:
    return false;
  }
}
