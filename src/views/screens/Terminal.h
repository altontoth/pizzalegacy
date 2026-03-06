/*
 * SPDX-FileCopyrightText: 2014 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>
#include <string>
#include "core/character/Actor.h"
#include "views/base/MainPanelView.h"
#include "views/helpers/TypewriterTextRenderer.h"

// Map enum values to strings
extern const std::map<TerminalCategory, std::string> category_to_string;
extern const std::map<TerminalCategory, std::vector<ActorType>>
    category_to_actor_types;

#define NUMBER_OF_TERMINAL_CATEGORIES 5
#define PHONE_NUMBER_LENGTH 7

enum class DialingPhase {
  SUCCESSFUL_CONNECTION,
  IDLE,
  AUTO_DIALING,
  WAITING_FOR_CONNECTION,
  RINGING,
  PHONE_PICKUP,
  CONNECTING,
  CONNECTED,
  ERROR,
  ERROR_RINGING,
};

class Terminal : public MainPanelView {
public:
  Terminal(GameContext& context, GameState& state, TerminalState ts);
  ~Terminal();
  void tick() override;
  void render(const RenderContext& ctx) override;

private:
  void standard_inputmenu();
  void keypad_menu();
  void draw_category(const RenderContext& ctx, TerminalCategory);
  void draw_main_menu(const RenderContext& ctx);
  void draw_dialing_prompt(const RenderContext& ctx);
  void reset_dialed_number();
  void dial_number(char);
  void clear_current_screen();
  std::vector<const Actor*> actors_by_category(TerminalCategory) const;

  void start_dialing_sequence(const std::string& phone_number);
  void transition_to_phase(DialingPhase phase);
  void on_dialing_phase_finished();
  void setup_waiting_animation();
  void setup_connected_animation();
  void setup_error_animation();

  const Surface& category_background_;
  TerminalCategory current_category_{};
  bool keypad_mode_{false};
  char dialed_number_[PHONE_NUMBER_LENGTH]{};
  bool fully_dialed_number_{false};
  bool set_dial_submenu_{false};

  DialingPhase dialing_phase_{DialingPhase::IDLE};
  int phase_timer_{0};
  std::string pending_phone_number_;
  ActorType pending_actor_type_{ActorType::NONE};
  std::optional<int> pending_actor_id_;

  size_t auto_dial_index_{0}; // Current digit being auto-dialed
  int auto_dial_timer_{0};    // Delay between auto-dialed digits

  // Text animation for status messages
  std::unique_ptr<TypewriterTextRenderer> prompt_animator_;

  // Force specific error message (-1 = use phone number algorithm)
  int forced_error_index_{-1};
};
