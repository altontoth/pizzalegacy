/*
 * SPDX-FileCopyrightText: 2014 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/MafiaMissionView.h"

#include "conversations/MafiaLine.h"
#include "core/character/Player.h"
#include "core/state/GameState.h"
#include "engine/ui/InteractionEvent.h"
#include "gfx/Blit.h"
#include "resources/FontManager.h"
#include "resources/FontTypes.h"
#include "resources/util/FileUtils.h"
#include "views/base/ViewDefs.h"

MafiaMissionView::MafiaMissionView(GameContext& context, GameState& state,
                                   MafiaLine line)
    : MainPanelView(context, state), line_(line) {
  auto& mission = state_.active_player()->mafia_state().active_mission;
  if (mission) {
    if (line_ == MafiaLine::SSSHHH_HERE_TAKE_THIS_SUITCASE) {
      mission->pickup_done = true;
    }
    else if (line_ == MafiaLine::COME_ON_HAND_IT_OVER) {
      mission->completed = true;
    }
  }

  auto file = fileutils::open("t/mafia.e");
  std::string l;
  int idx = 0;
  int target = static_cast<int>(line_);
  while (std::getline(file, l)) {
    if (idx == target) {
      text_ = l;
      break;
    }
    idx++;
  }

  input_panel_->add_button(Icon::ARROW_BACK, ButtonType::Square,
                           ButtonPosition::BottomRight, "Back",
                           [this] { state_.ui().pop(); });
}

void MafiaMissionView::tick() {}

void MafiaMissionView::render(const RenderContext& ctx) {
  interactions_.begin_frame();
  ctx.surface->fill(240);
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get("gfx/back38"));

  context_.font->render_into_rect(
      *ctx.surface, text_, ViewRect{20, 270, 192, 84}, FontFace::PIZ_NUMB);
  interactions_.create_frame_item(
      ViewRect{0, 0, views::base::MAIN_PANEL_WIDTH,
               views::base::MAIN_PANEL_HEIGHT},
      "mafia_catchall", [this](views::ui::InteractionEvent ie) {
        if (ie.type == views::ui::InteractionEvent::Type::Click) {
          state_.ui().pop();
        }
      });
}
