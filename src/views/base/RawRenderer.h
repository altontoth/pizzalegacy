/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "gfx/Surface.h"
#include "views/base/RawRenderable.h"

class RawRenderer {
public:
  gfx::Surface surface(const RawRenderable& renderable, int scale = 2);
};
