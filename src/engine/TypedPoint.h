/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

template <typename Tag> struct TypedPoint {
  int x{0};
  int y{0};

  TypedPoint() = default;
  TypedPoint(int x, int y) : x(x), y(y) {}

  TypedPoint operator+(TypedPoint other) const {
    return {x + other.x, y + other.y};
  }

  TypedPoint operator-(TypedPoint other) const {
    return {x - other.x, y - other.y};
  }

  TypedPoint& operator+=(TypedPoint other) {
    x += other.x;
    y += other.y;
    return *this;
  }

  TypedPoint& operator-=(TypedPoint other) {
    x -= other.x;
    y -= other.y;
    return *this;
  }

  bool operator==(TypedPoint other) const {
    return x == other.x && y == other.y;
  }
};

// Tags to create distinct types
struct MapCoordsTag {};
struct BgCoordsTag {};
struct ViewportCoordsTag {};

// The actual types
using MapPoint = TypedPoint<MapCoordsTag>;
using BgPoint = TypedPoint<BgCoordsTag>;
using ViewportPoint = TypedPoint<ViewportCoordsTag>;
