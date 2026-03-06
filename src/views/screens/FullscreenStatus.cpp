/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/FullscreenStatus.h"
#include "core/character/Player.h"
#include "core/state/GameState.h"
#include "engine/DisplayConstants.h"
#include "engine/FormatCompat.h"
#include "engine/input/InputManager.h"
#include "engine/ui/InteractionEvent.h"
#include "gfx/Blit.h"
#include "resources/FontManager.h"

FullscreenStatus::FullscreenStatus(GameContext& context, GameState& state,
                                   core::events::LeventId msg_id)
    : View(context, state), typewriter_(context_.font), message_id_(msg_id),
      listener_(context_.input->register_listener(*this)),
      background_([&]() -> const Surface& {
        const auto& msg_data = state_.message_manager().get_data(msg_id);

        assert(msg_data.background_index != 0);

        std::string bg_path =
            compat::format("gfx/zw{}.vga", msg_data.background_index);
        return context_.vga->get(bg_path);
      }()) {

  text_ = state_.message_manager().get_message_text(msg_id);

  // Set up typewriter renderer with wrap width
  // Position is {50, 50}, so available width is SCREEN_WIDTH - 2*50 = 540
  int wrap_width = DisplayConstants::SCREEN_WIDTH - (2 * 30);

  TextStyle ts{.color = 3,
               .align = AlignX::Left,
               .shadow = ShadowStyle{
                   .offset = {-1, 1},
                   .color = 20,
               }};
  typewriter_.set_text(text_, ViewPoint{30, 320}, FontFace::ASTANDAR, ts,
                       wrap_width);
  // 1 tick per character (30 FPS = ~33ms per char)
  typewriter_.set_char_delay(1);
  typewriter_.start();

  // Set up mouse click to dismiss (when typewriter is complete)
  interactions_.create_persistent_item(
      ViewRect{0, 0, DisplayConstants::SCREEN_WIDTH,
               DisplayConstants::SCREEN_HEIGHT},
      "fullscreen_status_dismiss", [this](views::ui::InteractionEvent ie) {
        if (ie.type == views::ui::InteractionEvent::Type::Click &&
            ie.button == views::ui::MouseButton::Left) {
          if (typewriter_.is_complete()) {
            dismiss();
          }
        }
      });
}

void FullscreenStatus::tick() {
  if (typewriter_.is_complete()) {
  }
  if (typewriter_.tick()) {
    mark_dirty();
  }
}

void FullscreenStatus::render(const RenderContext& ctx) {
  gfx::blit::blit_raw_with_palette(*ctx.surface, background_);

  typewriter_.render(*ctx.surface);
}

bool FullscreenStatus::on_action(const input::ActionEvent& ev) {
  if (!ev.state.pressed) {
    return false;
  }

  if (typewriter_.is_complete()) {
    dismiss();
    return true;
  }

  return false;
}

void FullscreenStatus::dismiss() {
  // Award score points and mark message as shown
  if (message_id_.has_value()) {
    auto* player = state_.active_player();
    if (player) {
      const auto& msg_data = state_.message_manager().get_data(*message_id_);

      if (msg_data.score_points > 0) {
        player->add_score(msg_data.score_points);
      }

      state_.message_manager().mark_as_shown(*message_id_, player->id());
    }
  }

  state_.ui().pop();
}
