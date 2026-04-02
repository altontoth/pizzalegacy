/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include <vector>

namespace core::restaurant {

// Reads dialog lines from the PERS.E and PERSF.E event text files.
//
// File layout: each section is 20 lines.  Within a section (1-based offsets):
//   1-3  : staff excuse variants
//   4-6  : boss option-1 responses
//   7-9  : boss option-2 responses
//   10-12: boss option-3 responses
//   13-15: staff reply lines (indexed by outcome 0/1/2)
//
// For Quit (PERS.E section 4) the reply slots are special:
//   13: "Sorry, it wasn't enough. Goodbye, you heartless beast!" (staff left)
//   14: "OK, I'll stay!"                                         (staff stayed)
class StaffEventDialogs {
public:
  // PERS.E section indices
  static constexpr int SECTION_LATE = 1;
  static constexpr int SECTION_SICK = 2;
  static constexpr int SECTION_RAISE = 3;
  static constexpr int SECTION_QUIT = 4;
  static constexpr int SECTION_MATERNITY = 5;

  // Get a line from PERS.E.
  // section_index: 0-5; offset_within_section: 1-based (1 = first excuse line).
  static std::string get_pers_line(int section_index,
                                   int offset_within_section);

  // Get a line from PERSF.E.
  static std::string get_persf_line(int section_index,
                                    int offset_within_section);

private:
  static const std::vector<std::string>& pers_lines();
  static const std::vector<std::string>& persf_lines();
};

} // namespace core::restaurant
