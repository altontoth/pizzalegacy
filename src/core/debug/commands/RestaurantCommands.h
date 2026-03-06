/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/debug/command/CommandDef.h"

namespace debug::commands {

// Build the complete 'restaurant' command tree
debug::command::CommandDef make_restaurant_commands();

} // namespace debug::commands
