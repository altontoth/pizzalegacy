/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/item/ActionText.h"
#include "resources/util/FileUtils.h"

namespace core::item {

const std::vector<std::string>& ActionText::lines() {
  static std::vector<std::string> cache =
      fileutils::load_text_lines("t/action.e");
  return cache;
}

std::string ActionText::get(ActionLine line) {
  const auto& all = lines();
  int idx = static_cast<int>(line);
  if (idx < 0 || idx >= static_cast<int>(all.size())) {
    return "Invalid action.e file";
  }
  return all[idx];
}

} // namespace core::item
