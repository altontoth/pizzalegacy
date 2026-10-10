/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <optional>

class GameState;
class GameContext;

namespace core::tutorial {

void start_tutorial_game(GameState& state, GameContext& context,
                         std::optional<uint32_t> seed = std::nullopt);

} // namespace core::tutorial
