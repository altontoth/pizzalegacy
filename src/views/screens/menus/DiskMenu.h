/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "views/base/Menu.h"

class DiskMenu : public Menu {
public:
  DiskMenu(GameContext& context, GameState& state);

protected:
  Surface background() const override;
};
