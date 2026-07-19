/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/input/Action.h"
#include "engine/input/Gamepad.h"
#include "engine/input/InputListener.h"
#include "engine/input/Key.h"
#include "engine/input/ListenerToken.h"
#include "engine/input/TextSession.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

union SDL_Event;
struct SDL_Window;

namespace input {

struct ActionState {
  bool pressed = false;  // edge this frame
  bool released = false; // edge this frame
  bool held = false;     // level
};

struct ActionEvent {
  Action action;
  ActionState state;
};

struct Vec2 {
  float x{0.f}, y{0.f};
  constexpr bool is_zero() const noexcept { return x == 0.f && y == 0.f; }
};

struct TextEvent {
  std::string text;
};

void configure_default_bindings(InputManager& in);

class InputManager {
public:
  InputManager();
  ~InputManager();

  InputManager(const InputManager&) = delete;
  InputManager& operator=(const InputManager&) = delete;
  InputManager(InputManager&&) = delete;
  InputManager& operator=(InputManager&&) = delete;

  // Event ingestion & per-frame maintenance
  void process(const SDL_Event& ev);
  void tick();

  void window(SDL_Window* window) noexcept { window_ = window; }

  // Listener registration (delivered in order until consumed)
  [[nodiscard]] ListenerToken register_listener(InputListener& l);
  void unregister_listener(InputListener& l);

  // Text input session (IME-aware). Returns RAII token.
  [[nodiscard]] TextSession start_text_input(InputListener& sink);
  void end_text_input();
  bool text_input_active() const noexcept;

  // Polling
  bool is_pressed(Action a) const noexcept;
  Mod modifiers() const noexcept { return mods_down_; }
  bool modifier_down(Mod m) const noexcept { return has(mods_down_, m); }
  std::pair<float, float> axis(Action a) const noexcept;

  // Debug
  void debug_dump_bindings(std::ostream& os) const;
  void debug_dump_state(std::ostream& os) const;

  // Bindings
  void bind_button(Binding chord, Action a);
  void bind_gamepad_button(GamepadButton b, Action a);
  void remove_bindings(Action a);
  void clear_bindings();

  // Bind an action that players cannot reassign. Bindings-file overrides for
  // it are refused, and it is left out of the generated template.
  void bind_fixed(Binding chord, Action a);
  bool is_fixed(Action a) const;

  // Record an action that has no default binding. It is written to the
  // bindings template with an empty value, so players can discover and
  // assign it.
  void declare_unbound(Action a);

  // Open a labelled group in the generated bindings template. Actions
  // bound after this call are listed under that label.
  void begin_section(std::string label);

  // A label and the position in documented_actions() where it starts.
  struct BindingDocSection {
    std::size_t first_action;
    std::string label;
  };

  const std::vector<std::pair<Binding, Action>>& button_bindings() const {
    return button_bindings_;
  }
  const std::vector<std::pair<GamepadButton, Action>>&
  gamepad_button_bindings() const {
    return gamepad_button_bindings_;
  }
  const std::vector<Action>& documented_actions() const {
    return documented_actions_;
  }
  const std::vector<BindingDocSection>& doc_sections() const {
    return doc_sections_;
  }

private:
  // Listeners (delivered in registration order until one returns true)
  std::vector<InputListener*> listeners_{};

  bool suppress_next_textinput_ = false;

  // Text input session
  InputListener* text_sink_{nullptr};
  bool text_active_{false};

  // Physical key state
  std::unordered_set<Key> keys_down_{};
  Mod mods_down_{Mod::None};

  // Action state (held)
  std::unordered_set<Action> actions_down_{};

  // Axis state: aggregated per action, normalized to [-1, +1]
  std::unordered_map<Action, std::pair<float, float>> axes_{};

  // Stick-direction pseudo-buttons currently past the press threshold
  std::unordered_set<GamepadButton> stick_dirs_down_;
  // Connected gamepad (SDL_Gamepad*), or nullptr if none.
  // Stored as void* to keep SDL out of this header.
  void* gamepad_{nullptr};

  // The SDL_Window that owns text input focus
  SDL_Window* window_{nullptr};

  // Binding tables. A key may carry several actions; they dispatch in the
  // order they were bound.
  std::vector<std::pair<Binding, Action>> button_bindings_;

  std::vector<std::pair<GamepadButton, Action>> gamepad_button_bindings_;

  // Every action the defaults declare, in declaration order, and the section
  // labels that group them.
  std::vector<Action> documented_actions_;
  std::vector<BindingDocSection> doc_sections_;

  std::unordered_set<Action> fixed_actions_;

  void document_action(Action a);

  // Key state updates from backend
  void key_down(Key k, Mod mods);
  void key_up(Key k, Mod mods);

  // Action evaluation & dispatch
  void evaluate_bindings_for_key_down(Key k, Mod mods);
  void evaluate_bindings_for_key_up(Key k);
  void gamepad_button_down(GamepadButton b);
  void gamepad_button_up(GamepadButton b);
  void update_stick_directions();
  void press_actions(const std::vector<Action>& actions);
  void dispatch_action_release(Action a);
  // True if the action was delivered to a listener or to the text sink.
  bool dispatch_action(const ActionEvent& ev);
  bool dispatch_text(const std::string& utf8);

  // Axis aggregation
  static float axis_component(bool positive_down, bool negative_down);
  void rebuild_axes();

  enum class TextRoute { Block, ToSinkExlusive, Normal };
  TextRoute route_in_text_mode(Action a) const noexcept {
    if (!text_active_) {
      return TextRoute::Normal;
    }
    switch (a) {
    case Action::ToggleConsole:
    case Action::Exit:
      return TextRoute::Normal;
    case Action::SubmitInput:
    case Action::CancelInput:
    case Action::Backspace:
    case Action::EraseLine:
    case Action::PageUp:
    case Action::PageDown:
    case Action::MoveUp:
    case Action::MoveDown:
      return TextRoute::ToSinkExlusive;
    default:
      return TextRoute::Block;
    }
  }
};
} // namespace input
