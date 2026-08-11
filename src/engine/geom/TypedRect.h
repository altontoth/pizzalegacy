/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <optional>
#include "engine/Logger.h"
#include "engine/Size.h"
#include "engine/geom/PxOffset.h"

template <typename Tag> struct TypedPoint;

template <typename Tag> struct TypedRect {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;

  // Constructors
  constexpr TypedRect() = default;
  constexpr TypedRect(int new_x, int new_y, int new_w, int new_h)
      : x(new_x), y(new_y), w(new_w), h(new_h) {}
  constexpr TypedRect(TypedPoint<Tag> p, int new_w, int new_h)
      : x(p.x), y(p.y), w(new_w), h(new_h) {}
  constexpr TypedRect(TypedPoint<Tag> p, Size s)
      : x(p.x), y(p.y), w(s.w), h(s.h) {}

  static TypedRect centered_at(const Size& s,
                               const TypedPoint<Tag>& c) noexcept {
    return TypedRect{c.x - (s.w / 2), c.y - (s.h / 2), s.w, s.h};
  }

  // Utility functions
  [[nodiscard]] bool contains(TypedPoint<Tag> point) const noexcept {
    return point.x >= x && point.x <= (x + w) && point.y >= y &&
           point.y <= (y + h);
  }

  [[nodiscard]] bool contains(const TypedRect& other) const noexcept {
    return other.x >= x && other.y >= y && (other.x + other.w) <= (x + w) &&
           (other.y + other.h) <= (y + h);
  }

  [[nodiscard]] bool intersects(const TypedRect& other) const noexcept {
    return x <= other.x + other.w && x + w >= other.x &&
           y <= other.y + other.h && y + h >= other.y;
  }

  [[nodiscard]] TypedPoint<Tag> point() const noexcept {
    return TypedPoint<Tag>{x, y};
  }

  [[nodiscard]] constexpr TypedPoint<Tag> center() const noexcept {
    return TypedPoint<Tag>{x + (w / 2), y + (h / 2)};
  }

  // Edges are inclusive, matching contains().
  [[nodiscard]] constexpr int right() const noexcept { return x + w; }

  [[nodiscard]] constexpr int bottom() const noexcept { return y + h; }

  [[nodiscard]] constexpr Size size() const noexcept { return Size{w, h}; }

  // Operator Overloads
  [[nodiscard]] bool operator==(const TypedRect& other) const noexcept {
    return x == other.x && y == other.y && w == other.w && h == other.h;
  }

  [[nodiscard]] bool operator!=(const TypedRect& other) const noexcept {
    return !(*this == other);
  }

  [[nodiscard]] constexpr TypedRect inset(int dx, int dy) const noexcept {
    return TypedRect{x + dx, y + dy, std::max(0, w - (2 * dx)),
                     std::max(0, h - (2 * dy))};
  }

  void to_string() const {
    Logger::debug("x:{}, y:{}, w:{}, h:{}", x, y, w, h);
  }
};

template <typename Tag>
[[nodiscard]] constexpr TypedRect<Tag> translate(TypedRect<Tag> r,
                                                 PxOffset d) noexcept {
  return TypedRect<Tag>{r.x + d.dx, r.y + d.dy, r.w, r.h};
}

template <typename Tag>
[[nodiscard]] constexpr TypedRect<Tag> move_to(TypedRect<Tag> r,
                                               TypedPoint<Tag> p) noexcept {
  return TypedRect<Tag>{p.x, p.y, r.w, r.h};
}

template <typename Tag>
[[nodiscard]] static inline TypedRect<Tag> intersect(TypedRect<Tag> a,
                                                     TypedRect<Tag> b) {
  const int x1 = std::max(a.x, b.x);
  const int y1 = std::max(a.y, b.y);
  const int x2 = std::min(a.x + a.w, b.x + b.w);
  const int y2 = std::min(a.y + a.h, b.y + b.h);
  if (x2 <= x1 || y2 <= y1) {
    return TypedRect<Tag>{0, 0, 0, 0};
  }
  return TypedRect<Tag>{x1, y1, x2 - x1, y2 - y1};
}

template <typename Tag>
[[nodiscard]] inline std::optional<TypedRect<Tag>>
intersect_rect(const TypedRect<Tag>& a, const TypedRect<Tag>& b) {
  const int x1 = std::max(a.x, b.x);
  const int y1 = std::max(a.y, b.y);
  const int x2 = std::min(a.x + a.w, b.x + b.w);
  const int y2 = std::min(a.y + a.h, b.y + b.h);
  if (x2 <= x1 || y2 <= y1) {
    return std::nullopt;
  }
  return TypedRect<Tag>{x1, y1, x2 - x1, y2 - y1};
}
