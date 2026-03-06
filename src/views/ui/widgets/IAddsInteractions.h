/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/ui/InteractionLayers.h"

struct IAddsInteractions {
  virtual void add_interactions(views::ui::InteractionContext&) = 0;
};
