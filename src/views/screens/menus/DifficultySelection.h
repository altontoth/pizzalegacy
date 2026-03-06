/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "views/base/Menu.h"

class DifficultySelection : public Menu {
public:
  DifficultySelection(GameContext& context, GameState& state);

private:
  Surface background() const override;
};
