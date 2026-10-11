/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "resources/BobManager.h"
#include "resources/BobSizeFn.h"

namespace core::food {

BobSizeFn topping_size_fn(BobManager& bobs);

} // namespace core::food
