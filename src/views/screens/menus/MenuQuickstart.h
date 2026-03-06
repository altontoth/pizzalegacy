/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "views/base/Menu.h"

class MenuQuickstart : public Menu {
public:
  MenuQuickstart(GameContext& context, GameState& state);

private:
  Surface background() const override;
};
