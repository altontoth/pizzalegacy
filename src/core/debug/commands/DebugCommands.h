/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/debug/command/CommandDef.h"

namespace debug::commands {

// Build the complete 'debug' command tree
debug::command::CommandDef make_debug_commands();

// Top-level convenience commands (aliases for debug subcommands)
debug::command::CommandDef make_dump_command();
debug::command::CommandDef make_help_command();

} // namespace debug::commands
