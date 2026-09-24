/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstddef>

// Byte offsets in the original's recipe record, which a PREZ recipe file
// stores whole.
namespace core::food::recipe_node {
inline constexpr std::size_t daily_sales = 8; // 7 u16, oldest first
// User creations start from a counter at 50.
inline constexpr std::size_t uid = 23;
inline constexpr std::size_t name = 24;
inline constexpr std::size_t judged = 54;
inline constexpr std::size_t imitation = 65;
// Up to four ingredient ids, strongest first, 255 when unused.
inline constexpr std::size_t primary_ingredients = 68;
inline constexpr std::size_t primary_ingredient_count = 4;
inline constexpr std::size_t flavors = 72; // fat, sweet, salty, hot: u16 0-10
inline constexpr std::size_t appearance = 80;
inline constexpr std::size_t refinement = 81;
inline constexpr std::size_t normality = 82;
inline constexpr std::size_t herbs = 83;
inline constexpr std::size_t popularity = 84; // one byte per customer group
inline constexpr std::size_t calories = 90;   // u16
inline constexpr std::size_t grams = 98;      // u16 per ingredient id
inline constexpr std::size_t grams_entries = 96;
inline constexpr std::size_t type_count = 290;
inline constexpr std::size_t placed_count = 291;
inline constexpr std::size_t placements = 292; // [id, state, x, y] each
// The record ends one empty placement past the last one.
inline constexpr std::size_t size_without_placements = 296;
} // namespace core::food::recipe_node
