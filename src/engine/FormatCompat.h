/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if HAS_STD_FORMAT
#include <format>
namespace compat {
using std::format;
template <typename... Args> using format_string = std::format_string<Args...>;
} // namespace compat
#define FORMAT_NAMESPACE std
#else
#include <fmt/format.h>
namespace compat {
using fmt::format;
template <typename... Args> using format_string = fmt::format_string<Args...>;
} // namespace compat
#define FORMAT_NAMESPACE fmt
#endif
