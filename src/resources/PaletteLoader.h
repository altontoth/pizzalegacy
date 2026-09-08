/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>

namespace resources {

// The color indices the fallback palette defines.
namespace fallback_palette {
inline constexpr uint8_t white = 255;
inline constexpr uint8_t black = 240;
// The red the mouse cursor is drawn in.
inline constexpr uint8_t cursor_red = 17;
} // namespace fallback_palette

// Reads gfx/palette.e from the game data and hands it to gfx::PaletteBank.
void load_palette_bank();

// The palette used before palette.e has been loaded.
void load_fallback_palette();

} // namespace resources
