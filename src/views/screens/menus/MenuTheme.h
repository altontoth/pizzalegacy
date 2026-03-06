/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "resources/FontTypes.h"

namespace views::ui::menu {

inline constexpr ShadowStyle shadow{.offset = {5, -5}, .color = 26};
inline constexpr TextStyle style{
    .color = 1, .align = AlignX::Center, .shadow = shadow};
inline constexpr TextStyle sel_style{
    .color = 29, .align = AlignX::Center, .shadow = shadow};
inline constexpr FontFace face = FontFace::MENU;

} // namespace views::ui::menu
