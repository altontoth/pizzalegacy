/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "gfx/Surface.h"

constexpr int NUMBER_OF_BARS = 10;
constexpr int LINE_HEIGHT = 2;

namespace views::ui::dropdowndecoration {

void draw_dropdown_decoration(gfx::Surface& s, int y_offset);

} // namespace views::ui::dropdowndecoration
