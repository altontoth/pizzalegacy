/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <vector>
#include "engine/geom/Spaces.h"

enum class Anchor { TopLeft, Center };

class BobRenderable {
public:
  virtual ~BobRenderable() = default;
  virtual const std::vector<int>& bobs() const = 0;
  virtual int bob_state() const = 0;
  virtual WorldPoint origin() const = 0;
  virtual Anchor anchor() const { return Anchor::TopLeft; }
};
