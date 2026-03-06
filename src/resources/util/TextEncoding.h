/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include <string_view>

namespace text {

std::string iso8859_1_to_utf8(std::string_view input);
std::string convert_platform_text(std::string_view input);
std::string read_iso8859_1_string(std::istream& is, std::size_t field_size);

} // namespace text
