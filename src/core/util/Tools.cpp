/*
 * SPDX-FileCopyrightText: 2011 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/util/Tools.h"

#include <algorithm>
#include <cstdlib>
#include <cwctype>
#include <iterator>

std::string Tools::to_uppercase(const std::string& input) {
  std::string output;
  std::ranges::transform(input, std::back_inserter(output),
                         [](char ch) { return std::towupper(ch); });
  return output;
}

int Tools::string_to_int(const std::string& number_str) {
  return atoi(number_str.c_str());
}
