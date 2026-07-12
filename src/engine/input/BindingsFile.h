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
void write_bindings_template(const std::string& path);

} // namespace input
