/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include <vector>

#include "engine/input/Action.h"

namespace input {

class InputManager;

void apply_bindings_file(InputManager& in, const std::string& path);

// The keys and gamepad buttons bound to an action, spelled as the file does.
std::vector<std::string> binding_tokens(const InputManager& in, Action a);

// Writes the bindings file if there isn't one yet.
void write_bindings_template(const InputManager& in, const std::string& path);

void save_bindings_file(const InputManager& in, const std::string& path);

} // namespace input
