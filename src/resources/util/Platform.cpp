/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "resources/util/Platform.h"
#include "core/util/ConfigManager.h"

namespace {
inline platform::AssetFormat g_asset_format = platform::AssetFormat::DOS;
};
namespace platform {

void init_asset_format() {
  auto* config = ConfigManager::instance();
  if (!config) {
    g_asset_format = AssetFormat::DOS;
    return;
  }

  g_asset_format = config->options().asset_format;
}

AssetFormat asset_format() { return g_asset_format; }

bool is_amiga() { return asset_format() == AssetFormat::AMIGA; }

bool is_dos() { return asset_format() == AssetFormat::DOS; }

Endian endianness() {
  return asset_format() == AssetFormat::AMIGA ? Endian::Big : Endian::Little;
}

const char* asset_format_string() { return is_amiga() ? "amiga" : "dos"; }

bool can_toggle_format() {
  auto* config = ConfigManager::instance();
  if (!config) {
    return false;
  }
  return config->both_formats_available();
}

AssetFormat alternate_format() {
  return (asset_format() == AssetFormat::DOS) ? AssetFormat::AMIGA
                                              : AssetFormat::DOS;
}

std::string format_display_name(AssetFormat format) {
  return (format == AssetFormat::DOS) ? "DOS" : "AMIGA";
}

} // namespace platform
