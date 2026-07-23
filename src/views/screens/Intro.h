/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/DebugLabel.h"
#include "engine/input/InputListener.h"
#include "engine/input/ListenerToken.h"
#include "views/base/View.h"
#include "views/ui/BusyCursor.h"

enum class Logo {
  NONE = 0,
  MPLOGO,
  PRESENTS,
  TITEL1,
  TITEL2,
  CYBER,
  CREDITS,
};

class Intro : public View,
              public input::InputListener,
              public debug::AutoLabel<Intro> {
public:
  Intro(GameContext& context, GameState& state);
  ~Intro();
  void tick() override;
  void render(const RenderContext& ctx) override;
  bool on_action(const input::ActionEvent& ev) override;

private:
  views::ui::BusyCursor busy_cursor_;
  uint32_t passed_ticks_{0};
  bool audio_initialized_{false};
  int current_credit_line_{0};
  std::vector<std::string> credit_lines_;
  std::vector<std::pair<int, std::vector<int>>> credit_mapping_;
  std::array<Logo, 6> logo_sequence_ = {Logo::MPLOGO,   Logo::CYBER,
                                        Logo::PRESENTS, Logo::TITEL1,
                                        Logo::TITEL2,   Logo::CREDITS};
  size_t current_logo_index_{0};
  input::ListenerToken listener_;
};
