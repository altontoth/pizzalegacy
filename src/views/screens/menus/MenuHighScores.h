/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "views/base/Menu.h"

class MenuHighScores : public Menu {
public:
  MenuHighScores(GameContext& context, GameState& state);

protected:
  Surface background() const override;
};
