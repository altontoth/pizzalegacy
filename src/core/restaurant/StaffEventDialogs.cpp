/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/restaurant/StaffEventDialogs.h"
#include "engine/Random.h"
#include "resources/util/FileUtils.h"

namespace core::restaurant {

namespace {

std::optional<int> section_for(StaffEventData::Type type) {
  using Type = StaffEventData::Type;
  if (StaffEventDialogs::is_persf(type)) {
    return static_cast<int>(type) - static_cast<int>(Type::DestroyCheeseStock);
  }
  switch (type) {
  case Type::Late:
    return StaffEventDialogs::SECTION_LATE;
  case Type::Sick:
    return StaffEventDialogs::SECTION_SICK;
  case Type::Raise:
    return StaffEventDialogs::SECTION_RAISE;
  case Type::Quit:
    return StaffEventDialogs::SECTION_QUIT;
  case Type::Maternity:
    return StaffEventDialogs::SECTION_MATERNITY;
  default:
    return std::nullopt;
  }
}

} // namespace

bool StaffEventDialogs::is_persf(StaffEventData::Type type) {
  return type >= StaffEventData::Type::DestroyCheeseStock;
}

std::string StaffEventDialogs::line(StaffEventData::Type type, int offset) {
  auto section = section_for(type);
  if (!section) {
    return {};
  }
  if (is_persf(type)) {
    return get_persf_line(*section, offset);
  }
  return get_pers_line(*section, offset);
}

std::optional<StaffEventDialogs::DialogTexts>
StaffEventDialogs::pick_dialog_texts(StaffEventData::Type type) {
  if (!section_for(type)) {
    return std::nullopt;
  }

  using engine::random::rand_between;
  // Pick random line variants (1-3) for each group
  int ev = rand_between(1, 4);
  int e1 = rand_between(1, 4);
  int e2 = rand_between(1, 4);
  int e3 = rand_between(1, 4);

  return DialogTexts{
      .excuse = line(type, ev),
      .options = {line(type, 3 + e1), line(type, 6 + e2), line(type, 9 + e3)},
  };
}

const std::vector<std::string>& StaffEventDialogs::pers_lines() {
  static std::vector<std::string> cache =
      fileutils::load_text_lines("t/pers.e");
  return cache;
}

const std::vector<std::string>& StaffEventDialogs::persf_lines() {
  static std::vector<std::string> cache =
      fileutils::load_text_lines("t/persf.e");
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
