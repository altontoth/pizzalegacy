/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include <string_view>
#include "engine/DebugLabel.h"
#include "engine/input/InputListener.h"
#include "engine/input/ListenerToken.h"
#include "views/base/View.h"

class AmigaBobViewer : public View,
                       public input::InputListener,
                       public debug::AutoLabel<AmigaBobViewer> {
public:
  AmigaBobViewer(GameContext& context, GameState& state,
                 std::string_view filename);
  void tick() override;
  void render(const RenderContext& ctx) override;
  bool on_action(const input::ActionEvent& ev) override;

private:
  uint8_t palette_;
  std::string filename_;
  int num_bitplanes_;           // number of bitplanes to try
  int header_offset_;           // offset from start of file to image data
  int width_;                   // Image width
  int height_;                  // Image height
  int palette_offset_;          // Offset to add to all palette indices
  int bitplane_padding_;        // Extra bytes between each bitplane
  int bytes_per_line_override_; // Manual override for bytes per line (-1 =
                                // auto)
  int bitplane_size_extra_; // Extra bytes to add to each bitplane's calculated
                            // size
  input::ListenerToken listener_;
};
