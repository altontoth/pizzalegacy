/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/Demographics.h"

// Random guest selection shared by seated-guest seeding and walker spawning.
// See the comment in RestaurantView::rebuild_guest_cache for how guest_type
// and face_index map onto gaeste.bob frames and portrait files.
namespace views::screens::restaurant {

// Pick a demographic weighted by the restaurant's current customer counts;
// uniform when there are no customers yet.
[[nodiscard]] ConsumerType
pick_weighted_demographic(const CustomerCounts& counts);

// One of the two sprite sets for the demographic.
[[nodiscard]] int pick_guest_type(ConsumerType demo);

// One of the two portraits for the sprite set.
[[nodiscard]] int pick_face_index(int guest_type);

} // namespace views::screens::restaurant
