/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/geom/GeomFormat.h"
#include "engine/geom/TypedDelta.h"
#include "engine/geom/TypedPoint.h"
#include "engine/geom/TypedRect.h"

// Tags to create distinct types
struct WorldGeomTag {};
struct ViewGeomTag {};
struct ScreenGeomTag {};

// The actual types
using WorldPoint = TypedPoint<WorldGeomTag>;
using ViewPoint = TypedPoint<ViewGeomTag>;
using ScreenPoint = TypedPoint<ScreenGeomTag>;

using WorldRect = TypedRect<WorldGeomTag>;
using ViewRect = TypedRect<ViewGeomTag>;
using ScreenRect = TypedRect<ScreenGeomTag>;

using WorldDelta = TypedDelta<WorldGeomTag>;

template <typename Tag>
constexpr TypedDelta<Tag> operator-(TypedPoint<Tag> a,
                                    TypedPoint<Tag> b) noexcept {
  return {a.x - b.x, a.y - b.y};
}
template <typename Tag>
constexpr TypedPoint<Tag> operator+(TypedPoint<Tag> p,
                                    TypedDelta<Tag> d) noexcept {
  return {p.x + d.dx, p.y + d.dy};
}
template <typename Tag>
constexpr TypedPoint<Tag> operator-(TypedPoint<Tag> p,
                                    TypedDelta<Tag> d) noexcept {
  return {p.x - d.dx, p.y - d.dy};
}

// Add a panel/view origin (all in View space)
constexpr ViewPoint add_origin(ViewPoint p, ViewPoint origin) noexcept {
  return {origin.x + p.x, origin.y + p.y};
}
constexpr ViewRect add_origin(ViewRect r, ViewPoint origin) noexcept {
  return {origin.x + r.x, origin.y + r.y, r.w, r.h};
}
