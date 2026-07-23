/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "resources/util/Platform.h"
#include "core/util/ConfigManager.h"

namespace {
inline platform::AssetFormat g_asset_format = platform::AssetFormat::ENGLISH;
};
namespace platform {

void init_asset_format() {
  auto* config = ConfigManager::instance();
  if (!config) {
    g_asset_format = AssetFormat::ENGLISH;
    return;
  }

  g_asset_format = config->options().asset_format;
}

AssetFormat asset_format() { return g_asset_format; }

const char* asset_format_string() {
  return (asset_format() == AssetFormat::GERMAN) ? "german" : "english";
}

bool can_toggle_format() {
  auto* config = ConfigManager::instance();
  if (!config) {
    return false;
  }
  return config->both_formats_available();
}

AssetFormat alternate_format() {
  return (asset_format() == AssetFormat::ENGLISH) ? AssetFormat::GERMAN
                                                  : AssetFormat::ENGLISH;
}

std::string format_display_name(AssetFormat format) {
  return (format == AssetFormat::ENGLISH) ? "English (DOS)" : "German (DOS)";
}

} // namespace platform
