/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

// What are we doing?
#include <optional>
#include "core/common/Ids.h"
#include "core/food/CanonicalPizza.h"
#include "core/food/Pizza.h"

namespace core::food {

enum class BakeKind {
  NewUser,              // <NEW>
  InstantiateCanonical, // canonical row that doesn't exist yet
  RecreateExisting,     // user or already-instantiated canonical
};

struct BakeSession {
  BakeKind kind;
  std::optional<core::food::CanonicalPizza> canonical_id; // if any
  std::optional<core::RecipeId> existing_id;              // if any
  std::string suggested_name; // e.g. canonical name or empty for <NEW>
  core::food::Pizza working;
};

} // namespace core::food
