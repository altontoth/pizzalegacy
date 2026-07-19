/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>

namespace input {

class InputManager;

void apply_bindings_file(InputManager& in, const std::string& path);

// Writes a commented template listing the current bindings as this build's
// defaults.
void write_bindings_template(const InputManager& in, const std::string& path);

} // namespace input
