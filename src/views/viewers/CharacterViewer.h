/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include "engine/DebugLabel.h"
#include "engine/input/InputListener.h"
#include "engine/input/ListenerToken.h"
#include "views/base/View.h"

namespace views::screens::restaurant {
class SeatedCharacterRenderer;
}

class CharacterViewer : public View,
                        public input::InputListener,
                        public debug::AutoLabel<CharacterViewer> {
public:
  CharacterViewer(GameContext& context, GameState& state);
  void tick() override;
  void render(const RenderContext& ctx) override;
  bool on_action(const input::ActionEvent& ev) override;

private:
  void dump_offsets() const;

  uint8_t palette_{4};
  input::ListenerToken listener_;
  int customer_type_{0};
  int rotation_{0};
  int arm_state_{0};
  int max_character_index_{11};
  bool show_extra_info_{false};
  bool show_crosshairs_{true};
  int nudge_step_{2};
  int selected_part_{0}; // Which part we're currently adjusting

  // Per-part debug offsets and visibility
  std::array<PxOffset, 7> part_debug_offsets_{};
  std::array<bool, 7> part_visible_;

  std::unique_ptr<views::screens::restaurant::SeatedCharacterRenderer>
      guest_renderer_;
};
