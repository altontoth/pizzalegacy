/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include "engine/FormatCompat.h"

struct PxOffset {
  int dx = 0;
  int dy = 0;

  constexpr PxOffset() : dx(0), dy(0) {}
  constexpr PxOffset(int new_dx, int new_dy) : dx(new_dx), dy(new_dy) {}
};

namespace FORMAT_NAMESPACE {
template <>
struct formatter<PxOffset> : FORMAT_NAMESPACE::formatter<std::string> {
  auto format(const PxOffset& s, auto& ctx) const {
    return FORMAT_NAMESPACE::formatter<std::string>::format(
        FORMAT_NAMESPACE::format("{},{}", s.dx, s.dy), ctx);
  }
};
} // namespace FORMAT_NAMESPACE
