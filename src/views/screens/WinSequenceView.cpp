/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/WinSequenceView.h"

#include "core/state/GameState.h"
#include "core/state/UIState.h"
#include "engine/DisplayConstants.h"
#include "engine/gfx/Blit.h"
#include "engine/input/InputManager.h"
#include "engine/ui/InteractionEvent.h"
#include "resources/util/FileUtils.h"

namespace {
constexpr uint8_t FG_COLOR = 7;
constexpr uint8_t SHADOW_COLOR = 2;
constexpr int WRAP_WIDTH = DisplayConstants::SCREEN_WIDTH - (2 * 30);

const std::vector<std::string>& ende_texts() {
  static std::vector<std::string> lines =
      fileutils::load_text_lines("t/ende.e", true);
  return lines;
}
} // namespace

WinSequenceView::WinSequenceView(GameContext& context, GameState& state,
                                 int line_index, core::PlayerId player_id)
    : View(context, state), typewriter_(context_.font), line_index_(line_index),
      player_id_(player_id),
      listener_(context_.input->register_listener(*this)),
      background_(context_.vga->get("gfx/ende")) {

  const auto& lines = ende_texts();
  total_lines_ = static_cast<int>(lines.size());

  TextStyle ts{.color = FG_COLOR,
               .align = TextAnchor::Start,
               .shadow = ShadowStyle{.offset = {-1, 1}, .color = SHADOW_COLOR}};
  typewriter_.set_text(lines[line_index_], ViewPoint{30, 320},
                       FontFace::ASTANDAR, ts, WRAP_WIDTH);
  typewriter_.set_char_delay(1);
  typewriter_.start();

  interactions_.create_persistent_item(
      ViewRect{0, 0, DisplayConstants::SCREEN_WIDTH,
               DisplayConstants::SCREEN_HEIGHT},
      "win_sequence_dismiss", [this](views::ui::InteractionEvent ie) {
        if (ie.type == views::ui::InteractionEvent::Type::Click &&
            ie.button == views::ui::MouseButton::Left) {
          if (typewriter_.is_complete()) {
            dismiss();
          }
          else {
            typewriter_.skip_to_end();
            mark_dirty();
          }
        }
      });
}

void WinSequenceView::tick() {
  if (typewriter_.tick()) {
    mark_dirty();
  }
}

void WinSequenceView::render(const RenderContext& ctx) {
  gfx::blit::blit_raw_with_palette(*ctx.surface, background_);
  typewriter_.render(*ctx.surface);
}

bool WinSequenceView::on_action(const input::ActionEvent& ev) {
  if (!ev.state.pressed) {
    return false;
  }

  if (typewriter_.is_complete()) {
    dismiss();
  }
  else {
    typewriter_.skip_to_end();
    mark_dirty();
  }
  return true;
}

void WinSequenceView::dismiss() {
  state_.ui().pop();
  if (line_index_ + 1 < total_lines_) {
    state_.ui().push<WinSequenceState>(WinSequenceState{
        .line_index = line_index_ + 1, .player_id = player_id_});
  }
  else {
    state_.eliminate_player(player_id_, GameOverCause::PizzaTycoon);
  }
}
