/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <string_view>
#include "core/finance/Money.h"

namespace core::finance::parse {
std::optional<Money> from_input(std::string_view);
}
