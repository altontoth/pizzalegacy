/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include "engine/geom/Spaces.h"

class Renderable;

struct LayerInfo {
  Renderable* view;
  ViewRect area;
  std::string label;
  // Skip clearing - render over existing content:
  bool preserve_background = false;
};
