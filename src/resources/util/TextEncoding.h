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

// Removes the last codepoint, keeping the string valid UTF-8.
void pop_back_utf8(std::string& s);

// Shortens to at most max_bytes without cutting a codepoint in half.
void truncate_utf8(std::string& s, std::size_t max_bytes);

} // namespace text
