/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>

#include "engine/gfx/Surface.h"

using gfx::Surface;

class VgaLoader {
public:
  static Surface load(const std::string& filename);
};
