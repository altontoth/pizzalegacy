/*
 * SPDX-FileCopyrightText: 2011 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>

class Tools {
public:
  static std::string to_uppercase(const std::string&);
  static int string_to_int(const std::string&);
};
