/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>

namespace platform {

enum class AssetFormat { ENGLISH, GERMAN };
void init_asset_format();
AssetFormat asset_format();
const char* asset_format_string();
bool can_toggle_format();
AssetFormat alternate_format();
std::string format_display_name(AssetFormat format);

} // namespace platform
