/*
 * SPDX-FileCopyrightText: 2013 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/base/MainPanelView.h"
#include "core/state/GameState.h"
#include "core/util/LegacyConstants.h"
#include "engine/geom/Spaces.h"
#include "views/base/ViewDefs.h"

MainPanelView::MainPanelView(GameContext& context, GameState& state)
    : PanelView(context, state,
                ViewPoint{LegacyConstants::WOOD_BORDER_WIDTH_LEFT,
                          LegacyConstants::WOOD_BORDER_HEIGHT_TOP}),
      input_surface_(Surface(views::base::INPUT_PANEL_WIDTH,
                             views::base::INPUT_PANEL_HEIGHT)) {

  input_panel_ = std::make_unique<InputPanel>(
      context, state,
      ViewPoint{views::base::INPUT_PANEL_OFFSET_X,
                views::base::INPUT_PANEL_OFFSET_Y});
}

InputPanel* MainPanelView::input_panel() {
  if (!input_panel_) {
    Logger::debug("I don't have a input_panel in MainPanel right now :(");
  }
  return input_panel_.get();
}
