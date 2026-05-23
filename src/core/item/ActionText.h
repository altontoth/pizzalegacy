/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include <vector>
#include "core/item/ActionLine.h"

namespace core::item {

// Reads text lines from ACTION.E
class ActionText {
public:
  static std::string get(ActionLine line);

private:
  static const std::vector<std::string>& lines();
};

} // namespace core::item
