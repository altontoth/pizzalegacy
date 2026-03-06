/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/base/PanelView.h"
#include "engine/geom/Spaces.h"

PanelView::PanelView(GameContext& context, GameState& state, ViewPoint origin)
    : View(context, state), panel_origin_(origin) {}
