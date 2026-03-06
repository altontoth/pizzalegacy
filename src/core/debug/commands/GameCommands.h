/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/debug/command/CommandDef.h"

namespace debug::commands {

// Build the complete 'game' command tree
debug::command::CommandDef make_game_commands();

} // namespace debug::commands
