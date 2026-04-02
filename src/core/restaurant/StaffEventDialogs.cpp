/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/restaurant/StaffEventDialogs.h"
#include "resources/util/FileUtils.h"

namespace core::restaurant {

namespace {
std::vector<std::string> load_e_file(const char* path) {
  std::vector<std::string> lines;
  auto file = fileutils::open(path);
  std::string line;
  while (std::getline(file, line)) {
    lines.push_back(std::move(line));
  }
  return lines;
}
} // namespace

const std::vector<std::string>& StaffEventDialogs::pers_lines() {
  static std::vector<std::string> cache = load_e_file("t/pers.e");
  return cache;
}

const std::vector<std::string>& StaffEventDialogs::persf_lines() {
  static std::vector<std::string> cache = load_e_file("t/persf.e");
  return cache;
}

std::string StaffEventDialogs::get_pers_line(int section_index,
                                             int offset_within_section) {
  const auto& lines = pers_lines();
  int idx = (section_index * 20) + (offset_within_section - 1);
  if (idx < 0 || idx >= static_cast<int>(lines.size())) {
    return {};
  }
  return lines[idx];
}

std::string StaffEventDialogs::get_persf_line(int section_index,
                                              int offset_within_section) {
  const auto& lines = persf_lines();
  int idx = (section_index * 20) + (offset_within_section - 1);
  if (idx < 0 || idx >= static_cast<int>(lines.size())) {
    return {};
  }
  return lines[idx];
}

} // namespace core::restaurant
