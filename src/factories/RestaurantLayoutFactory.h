/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/restaurant/RestaurantLayout.h"

class RestaurantLayoutFactory {
public:
  static std::unique_ptr<RestaurantLayout> create_layout(const std::string&);
};
