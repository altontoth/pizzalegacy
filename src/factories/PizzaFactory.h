/*
 * SPDX-FileCopyrightText: 2012 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>
#include "core/food/Pizza.h"
#include "engine/Size.h"

using BobSizeFn = std::function<Size(int ingredient_id, int state)>;

class PizzaFactory {
public:
  static std::unique_ptr<core::food::Pizza>
  create_pizza(const std::string& filename, const BobSizeFn& get_size);

  static std::unique_ptr<core::food::Pizza>
  create_pizza(int recipe_id, const BobSizeFn& get_size);
};
