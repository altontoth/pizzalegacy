/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <vector>

#include "core/debug/command/CommandDef.h"

class Game;
class GameContext;
class GameState;

namespace engine {

// The console commands that write what is on screen to a file.
std::vector<debug::command::CommandDef>
make_screen_commands(Game& game, GameContext& context, GameState& state);

} // namespace engine
