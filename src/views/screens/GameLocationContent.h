/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string_view>
#include "engine/geom/Spaces.h"

namespace game_location {

// The screen is rendered at 640x400 and halved to make the VGA at the original
// game's size, so everything sits on an even coordinate. A box draws its far
// edge at x + w - 1, which is why the sizes are odd.
inline constexpr ViewRect browse_button{174, 336, 141, 33};
inline constexpr ViewRect exit_button{354, 336, 111, 33};

inline constexpr int button_border = 2;

inline constexpr std::string_view browse_label = "Browse...";
inline constexpr std::string_view exit_label = "Exit";

inline constexpr std::string_view headline =
    "Find your Pizza Tycoon/Connection files";

inline constexpr std::string_view hint =
    "Pizza Legacy needs the original English-language game files to "
    "play. On Steam and GOG, Pizza Tycoon is sold as Pizza Connection.\n\n"
    "Select your Steam, GOG, or game folder. Pizza Legacy will "
    "automatically find the game files inside it.";

} // namespace game_location
