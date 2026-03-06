/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/geom/Spaces.h"
#include "views/base/View.h"

class SharedPanel {
public:
  SharedPanel(View* v, ViewRect loc) : view(v), location(loc) {}
  virtual ~SharedPanel() = default;

  View* view;
  ViewRect location;
};
