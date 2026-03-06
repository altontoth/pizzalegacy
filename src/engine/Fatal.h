/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdlib>
#include <iostream>
#include <string_view>

[[noreturn]] inline void fatal(std::string_view msg, int code = 1) {
  std::cerr << "Fatal error: " << msg << '\n';
  std::exit(code);
}
