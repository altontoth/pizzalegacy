/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

// What are we doing?
#include <optional>
#include "core/common/Ids.h"
#include "core/food/Pizza.h"
#include "core/food/StandardPizza.h"

namespace core::food {

enum class BakeKind {
  NewUser,             // <NEW>
  InstantiateStandard, // standard row that doesn't exist yet
  RecreateExisting,    // user or already-instantiated standard
};

struct BakeSession {
  BakeKind kind;
  std::optional<core::food::StandardPizza> reference_id; // if any
  std::optional<core::RecipeId> existing_id;              // if any
  std::string suggested_name; // the standard pizza's name, empty for <NEW>
  core::food::Pizza working;
};

} // namespace core::food
