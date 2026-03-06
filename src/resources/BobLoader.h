/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>
#include <string>

#include "resources/Bob.h"

class BobLoader {
public:
  static std::unique_ptr<Bob> load(const std::string& filename);
};
