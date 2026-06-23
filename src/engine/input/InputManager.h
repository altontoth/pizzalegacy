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
  void bind_axis_keys(Action a, Key up, Key down, Key left, Key right);
  void bind_gamepad_button(GamepadButton b, Action a);
  // For binding with a vec2 action:
  void bind_gamepad_stick(Action a, GamepadAxis x_axis, GamepadAxis y_axis);
  void clear_bindings();

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

  // Gamepad analog-stick state, merged into axes_ by rebuild_axes(). Keyed by
  // action, populated from gamepad_stick_bindings_ on controller axis events.
  std::unordered_map<Action, std::pair<float, float>> gamepad_axes_{};
  // Connected gamepad (SDL_Gamepad*), or nullptr if none.
  // Stored as void* to keep SDL out of this header.
  void* gamepad_{nullptr};

  // The SDL_Window that owns text input focus
  SDL_Window* window_{nullptr};

  // Binding tables
  struct BindingHash {
    std::size_t operator()(const Binding& b) const noexcept {
      return (static_cast<std::size_t>(static_cast<uint8_t>(b.mods)) << 16) ^
             static_cast<std::size_t>(static_cast<uint16_t>(b.key));
    }
  };
  struct BindingEq {
    bool operator()(const Binding& a, const Binding& b) const noexcept {
      return a.key == b.key && a.mods == b.mods;
    }
  };

  std::unordered_multimap<Binding, Action, BindingHash, BindingEq>
      button_bindings_;

  struct KeyVec2 {
    Key up{}, down{}, left{}, right{};
  };
  std::unordered_multimap<Action, KeyVec2> axis_key_bindings_;

  std::unordered_map<GamepadButton, Action> gamepad_button_bindings_;
  struct AxisVec2 {
    GamepadAxis x{GamepadAxis::Unknown}, y{GamepadAxis::Unknown};
  };
  std::unordered_map<Action, AxisVec2> gamepad_stick_bindings_;

  // Key state updates from backend
  void key_down(Key k, Mod mods);
  void key_up(Key k, Mod mods);

  // Action evaluation & dispatch
  void evaluate_bindings_for_key_down(Key k, Mod mods);
  void evaluate_bindings_for_key_up(Key k, Mod mods);
  bool dispatch_action_press(Action a);
  void dispatch_action_release(Action a);
  bool dispatch_action(const ActionEvent& ev);
  bool dispatch_text(const std::string& utf8);

  // Axis aggregation
  static float axis_component(bool positive_down, bool negative_down);
  std::pair<float, float> compute_axis_from_keys(const KeyVec2& kv) const;
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
