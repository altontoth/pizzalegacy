/*
 * SPDX-FileCopyrightText: 2012 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/base/PaneledView.h"

#include <cassert>
#include <memory>

#include "core/util/LegacyConstants.h"
#include "engine/DisplayConstants.h"
#include "engine/geom/Spaces.h"
#include "gfx/Blit.h"
#include "views/base/SharedPanel.h"
#include "views/base/ViewDefs.h"
#include "views/screens/InputPanel.h"
#include "views/screens/StatusDisplay.h"

using namespace views::base;
using DisplayConstants::SCREEN_HEIGHT;
using DisplayConstants::SCREEN_WIDTH;
using LegacyConstants::WOOD_BORDER_HEIGHT_TOP;
using LegacyConstants::WOOD_BORDER_WIDTH_LEFT;

PaneledView::PaneledView(GameContext& context, GameState& state,
                         std::unique_ptr<View> inner)
    : View(context, state),
      main_surface_(Surface(MAIN_PANEL_WIDTH, MAIN_PANEL_HEIGHT)),
      panel_container_view_(
          std::make_unique<PanelContainerView>(context, state)) {

  auto* mpv = dynamic_cast<MainPanelView*>(inner.get());
  if (!mpv) {
    fatal("PaneledView requires a MainPanelView");
  }

  ViewRect size_and_pos_main = {WOOD_BORDER_WIDTH_LEFT, WOOD_BORDER_HEIGHT_TOP,
                                MAIN_PANEL_WIDTH, MAIN_PANEL_HEIGHT};
  ViewRect size_and_pos_status = {STATUS_PANEL_OFFSET_X, STATUS_PANEL_OFFSET_Y,
                                  STATUS_PANEL_WIDTH, STATUS_PANEL_WIDTH};
  ViewRect size_and_pos_input = {INPUT_PANEL_OFFSET_X, INPUT_PANEL_OFFSET_Y,
                                 INPUT_PANEL_WIDTH, INPUT_PANEL_HEIGHT};

  status_panel_ = std::make_unique<SharedPanel>(state.status_display(context),
                                                size_and_pos_status);
  input_panel_ =
      std::make_unique<SharedPanel>(mpv->input_panel(), size_and_pos_input);
  main_panel_view_.reset(static_cast<MainPanelView*>(inner.release()));

  mpv->interaction_origin(size_and_pos_main.point());
  state.status_display(context)->interaction_origin(
      size_and_pos_status.point());
  mpv->input_panel()->interaction_origin(size_and_pos_input.point());
}

void PaneledView::render(const RenderContext& /* ctx */) {
  fatal("PaneledView::render should not be called.");
}

void PaneledView::tick() {
  main_panel_view_->tick();
  input_panel_->view->tick();
  status_panel_->view->tick();
}

// Returns multiple layers to the renderer
std::vector<LayerInfo> PaneledView::get_layers() {
  std::vector<LayerInfo> layers;

  // Background layer
  layers.push_back(LayerInfo{
      .view = panel_container_view_.get(),
      .area = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT},
      .label = "Wood border",
  });

  // Main panel layer
  layers.push_back(LayerInfo{
      .view = main_panel_view_.get(),
      .area = {WOOD_BORDER_WIDTH_LEFT, WOOD_BORDER_HEIGHT_TOP, MAIN_PANEL_WIDTH,
               MAIN_PANEL_HEIGHT},
      .label = "Main Panel",
      .preserve_background = main_panel_view_->preserve_background(),
  });

  // Input panel layer
  layers.push_back(LayerInfo{
      .view = input_panel_->view,
      .area = {input_panel_->location.x, input_panel_->location.y,
               INPUT_PANEL_WIDTH, INPUT_PANEL_HEIGHT},
      .label = "Input panel",
  });

  // Status panel layer
  layers.push_back(LayerInfo{
      .view = status_panel_->view,
      .area = {status_panel_->location.x, status_panel_->location.y,
               STATUS_PANEL_WIDTH, STATUS_PANEL_HEIGHT},
      .label = "Status panel",
  });

  return layers;
}
