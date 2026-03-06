/*
 * SPDX-FileCopyrightText: 2012 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/state/GameState.h"
#include "core/util/GameContext.h"
#include "engine/DebugConsole.h"

void register_debug_commands(DebugConsole& console, GameState& state,
                             GameContext& context);
