/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include <vector>

#include "core/tutorial/TutorialStep.h"

namespace core::tutorial {

// The premise that the player has to rent and the realtor for it.
constexpr MapTile tutorial_premise{.x = 119, .y = 91};
inline const std::string tutorial_realtor = "Quixote Dream Estate";

const std::vector<Step>& tutorial_steps();

} // namespace core::tutorial
