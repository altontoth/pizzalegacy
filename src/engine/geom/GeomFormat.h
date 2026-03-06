/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/FormatCompat.h"
#include "engine/geom/TypedPoint.h"
#include "engine/geom/TypedRect.h"

namespace std {

template <typename Tag> struct hash<TypedRect<Tag>> {
  size_t operator()(const TypedRect<Tag>& r) const noexcept {
    size_t h = 0;
    auto mix = [&](size_t v) {
      h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    };
    mix(std::hash<int>{}(r.x));
    mix(std::hash<int>{}(r.y));
    mix(std::hash<int>{}(r.w));
    mix(std::hash<int>{}(r.h));
    return h;
  }
};

template <typename Tag> struct hash<TypedPoint<Tag>> {
  size_t operator()(const TypedPoint<Tag>& p) const noexcept {
    size_t h = 0;
    auto mix = [&](size_t v) {
      h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    };
    mix(std::hash<int>{}(p.x));
    mix(std::hash<int>{}(p.y));
    return h;
  }
};

} // namespace std

namespace FORMAT_NAMESPACE {
template <typename Tag>
struct formatter<TypedRect<Tag>> : FORMAT_NAMESPACE::formatter<std::string> {
  auto format(const TypedRect<Tag>& r, auto& ctx) const {
    return FORMAT_NAMESPACE::formatter<std::string>::format(
        FORMAT_NAMESPACE::format("([{}],[{}])", r.point(), r.size()), ctx);
  }
};
template <typename Tag>
struct formatter<TypedPoint<Tag>> : FORMAT_NAMESPACE::formatter<std::string> {
  auto format(const TypedPoint<Tag>& p, auto& ctx) const {
    return FORMAT_NAMESPACE::formatter<std::string>::format(
        FORMAT_NAMESPACE::format("({},{})", p.x, p.y), ctx);
  }
};

} // namespace FORMAT_NAMESPACE
