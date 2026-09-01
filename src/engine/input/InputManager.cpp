/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/input/InputManager.h"

#include "engine/input/Action.h"
#include "engine/input/InputListener.h"
#include "engine/input/Key.h"
#include "engine/input/ListenerToken.h"
#include "engine/input/TextSession.h"

#include "engine/input/backend/SDLGamepad.h"
#include "engine/input/backend/SDLKeymap.h"

#include <algorithm>
#include <iostream>
#include <utility>
#include "engine/SDL_include.h"
#include "magic_enum/magic_enum.hpp"

namespace input {

static inline Mod mods_from_sdl(Uint16 sdl_mod) noexcept {
  Mod m = Mod::None;
  if (sdl_mod & SDL_KMOD_SHIFT) {
    m = m | Mod::Shift;
  }
  if (sdl_mod & SDL_KMOD_CTRL) {
    m = m | Mod::Ctrl;
  }
  if (sdl_mod & SDL_KMOD_ALT) {
    m = m | Mod::Alt;
  }
  if (sdl_mod & SDL_KMOD_GUI) {
    m = m | Mod::Meta;
  }
  return m;
}

// The modifier bit a key contributes by being held down. Pressing Left Ctrl
// sets SDL_KMOD_CTRL, so the keydown for the modifier key itself arrives with
// its own bit already in the mask.
static inline Mod contributed_mod_bit(Key k) noexcept {
  switch (k) {
  case Key::LShift:
  case Key::RShift:
    return Mod::Shift;
  case Key::LCtrl:
  case Key::RCtrl:
    return Mod::Ctrl;
  case Key::LAlt:
  case Key::RAlt:
    return Mod::Alt;
  case Key::LMeta:
  case Key::RMeta:
    return Mod::Meta;
  default:
    return Mod::None;
  }
}

static inline Mod without(Mod m, Mod bit) noexcept {
  return static_cast<Mod>(static_cast<uint8_t>(m) & ~static_cast<uint8_t>(bit));
}

// Build the binding key to look up for a physical key event. For a modifier
// key we drop its own contributed bit so a standalone binding (e.g. "LCtrl")
// matches.
static inline Binding lookup_binding(Key k, Mod mods) noexcept {
  return Binding{without(mods, contributed_mod_bit(k)), k};
}

// Each vec2 action is derived from the held state of four direction actions,
// so any input bound to a direction action also drives the vec2.
struct Vec2Components {
  Action vec2;
  Action up, down, left, right;
};
static constexpr Vec2Components vec2_components[] = {
    {Action::MoveVec2, Action::MoveUp, Action::MoveDown, Action::MoveLeft,
     Action::MoveRight},
    {Action::P2_MoveVec2, Action::P2_MoveUp, Action::P2_MoveDown,
     Action::P2_MoveLeft, Action::P2_MoveRight},
    {Action::ScrollVec2, Action::ScrollUp, Action::ScrollDown,
     Action::ScrollLeft, Action::ScrollRight},
};

// Analog stick directions dispatched as pseudo-button presses. Hysteresis:
// press past one threshold, release only once back under a lower one.
struct StickDir {
  GamepadButton button;
  GamepadAxis axis;
  float sign;
};
static constexpr StickDir stick_dirs[] = {
    {GamepadButton::LeftStickUp, GamepadAxis::LeftY, -1.f},
    {GamepadButton::LeftStickDown, GamepadAxis::LeftY, 1.f},
    {GamepadButton::LeftStickLeft, GamepadAxis::LeftX, -1.f},
    {GamepadButton::LeftStickRight, GamepadAxis::LeftX, 1.f},
    {GamepadButton::RightStickUp, GamepadAxis::RightY, -1.f},
    {GamepadButton::RightStickDown, GamepadAxis::RightY, 1.f},
    {GamepadButton::RightStickLeft, GamepadAxis::RightX, -1.f},
    {GamepadButton::RightStickRight, GamepadAxis::RightX, 1.f},
};
static constexpr float stick_press_threshold = 0.5f;
static constexpr float stick_release_threshold = 0.35f;

static constexpr bool repeats_while_held(Action a) noexcept {
  switch (a) {
  case Action::Backspace:
  case Action::MoveUp:
  case Action::MoveDown:
  case Action::MoveLeft:
  case Action::MoveRight:
  case Action::PageUp:
  case Action::PageDown:
    return true;
  default:
    return false;
  }
}

InputManager::InputManager() {}

InputManager::~InputManager() {
  if (text_active_) {
    end_text_input();
  }
  if (gamepad_) {
    SDL_CloseGamepad(static_cast<SDL_Gamepad*>(gamepad_));
    gamepad_ = nullptr;
  }
}

ListenerToken InputManager::register_listener(InputListener& l,
                                              ListenerPriority priority) {
  if (std::find(listeners_.begin(), listeners_.end(), &l) == listeners_.end()) {
    if (priority == ListenerPriority::High) {
      listeners_.insert(listeners_.begin(), &l);
    }
    else {
      listeners_.push_back(&l);
    }
  }
  return ListenerToken{*this, l};
}

void InputManager::unregister_listener(InputListener& l) {
  auto it = std::find(listeners_.begin(), listeners_.end(), &l);
  if (it != listeners_.end()) {
    listeners_.erase(it);
  }
}

[[nodiscard]] TextSession InputManager::start_text_input(InputListener& sink) {
  text_sink_ = &sink;
  text_active_ = true;
  SDL_StartTextInput(window_);
  return TextSession{this};
}

void InputManager::end_text_input() {
  if (!text_active_) {
    return;
  }
  SDL_StopTextInput(window_);
  text_active_ = false;
  text_sink_ = nullptr;
  repeat_ = {};
}

bool InputManager::text_input_active() const noexcept { return text_active_; }

bool InputManager::is_pressed(Action a) const noexcept {
  return actions_down_.find(a) != actions_down_.end();
}

std::pair<float, float> InputManager::axis(Action a) const noexcept {
  if (auto it = axes_.find(a); it != axes_.end()) {
    return it->second;
  }
  return {0.f, 0.f};
}

void InputManager::tick() {
  rebuild_axes();
  suppress_next_textinput_ = false;
}

void InputManager::process(const SDL_Event& ev) {
  switch (ev.type) {
  case SDL_EVENT_KEY_DOWN:
  case SDL_EVENT_KEY_UP: {
    const bool down = (ev.type == SDL_EVENT_KEY_DOWN);
    const Key k = backend::key_from_scancode(ev.key.scancode);
    const Mod m = mods_from_sdl(ev.key.mod);

    if (k == Key::Unknown) {
      return;
    }
    // advance_key_repeat repeats held keys.
    if (down && ev.key.repeat) {
      break;
    }

    if (down) {
      key_down(k, m);
    }
    else {
      key_up(k, m);
    }
    break;
  }

  case SDL_EVENT_TEXT_INPUT: {
    if (!text_active_ || !text_sink_) {
      break;
    }

    if (suppress_next_textinput_) {
      suppress_next_textinput_ = false;
      break;
    }
    // Only the first text event for a key is dispatched; advance_key_repeat
    // produces the rest.
    if (last_key_down_ == repeat_.key && !repeat_.text.empty()) {
      break;
    }
    dispatch_text(ev.text.text);
    arm_text_repeat(last_key_down_, ev.text.text);
    break;
  }

  case SDL_EVENT_WINDOW_FOCUS_LOST:
    repeat_ = {};
    break;

  case SDL_EVENT_GAMEPAD_ADDED: {
    // Open the first controller that connects; ignore the rest for now
    // TODO: add options for this?
    if (!gamepad_) {
      gamepad_ = SDL_OpenGamepad(ev.gdevice.which);
    }
    break;
  }

  case SDL_EVENT_GAMEPAD_REMOVED: {
    if (gamepad_) {
      SDL_CloseGamepad(static_cast<SDL_Gamepad*>(gamepad_));
      gamepad_ = nullptr;
      // Release held stick directions so movement stops on disconnect
      for (GamepadButton b : stick_dirs_down_) {
        gamepad_button_up(b);
      }
      stick_dirs_down_.clear();
    }
    break;
  }

  case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
    if (gamepad_) {
      update_stick_directions();
    }
    break;
  }

  case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
  case SDL_EVENT_GAMEPAD_BUTTON_UP: {
    const GamepadButton b = backend::button_from_sdl(
        static_cast<SDL_GamepadButton>(ev.gbutton.button));
    if (ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
      gamepad_button_down(b);
    }
    else {
      gamepad_button_up(b);
    }
    break;
  }

  default:
    break;
  }
}

void InputManager::key_down(Key k, Mod mods) {
  keys_down_.insert(k);
  mods_down_ = mods;
  last_key_down_ = k;
  evaluate_bindings_for_key_down(k, mods);
}

void InputManager::key_up(Key k, Mod mods) {
  keys_down_.erase(k);
  mods_down_ = mods;
  // Releasing any key ends the repeat, modifiers included.
  repeat_ = {};

  evaluate_bindings_for_key_up(k);
}

void InputManager::evaluate_bindings_for_key_down(Key k, Mod mods) {
  Binding b = lookup_binding(k, mods);

  std::vector<Action> actions;
  for (const auto& [bind, bound_action] : button_bindings_) {
    if (bind == b) {
      actions.push_back(bound_action);
    }
  }
  // A combination that's not bound to any chord falls back to the key alone,
  // so holding Shift for speed still scrolls with the arrows. No fallback for
  // the Meta key, so Cmd+K does not come through as a plain K.
  if (actions.empty() && b.mods != Mod::None && !has(b.mods, Mod::Meta)) {
    const Binding plain{Mod::None, k};
    for (const auto& [bind, bound_action] : button_bindings_) {
      if (bind == plain) {
        actions.push_back(bound_action);
      }
    }
  }
  press_actions(actions, k);
}

void InputManager::evaluate_bindings_for_key_up(Key k) {
  // Match on the key alone: the held modifiers may have changed since the
  // press, and a stricter match would leave the action stuck down.
  for (const auto& [bind, bound_action] : button_bindings_) {
    if (bind.key != k) {
      continue;
    }
    Action a = bound_action;
    if (a == Action::Exit && text_active_) {
      a = Action::CancelInput;
    }

    dispatch_action_release(a);
  }
}

void InputManager::gamepad_button_down(GamepadButton b) {
  std::vector<Action> actions;
  for (const auto& [button, bound_action] : gamepad_button_bindings_) {
    if (button == b) {
      actions.push_back(bound_action);
    }
  }
  press_actions(actions, Key::Unknown);
}

void InputManager::gamepad_button_up(GamepadButton b) {
  for (const auto& [button, bound_action] : gamepad_button_bindings_) {
    if (button == b) {
      dispatch_action_release(bound_action);
    }
  }
}

void InputManager::update_stick_directions() {
  auto* pad = static_cast<SDL_Gamepad*>(gamepad_);
  constexpr float AXIS_MAX = 32767.f;
  for (const auto& dir : stick_dirs) {
    const SDL_GamepadAxis sdl_axis = backend::axis_to_sdl(dir.axis);
    if (sdl_axis == SDL_GAMEPAD_AXIS_INVALID) {
      continue;
    }
    const float v = dir.sign *
                    static_cast<float>(SDL_GetGamepadAxis(pad, sdl_axis)) /
                    AXIS_MAX;
    const bool was_down = stick_dirs_down_.contains(dir.button);
    if (!was_down && v >= stick_press_threshold) {
      stick_dirs_down_.insert(dir.button);
      gamepad_button_down(dir.button);
    }
    else if (was_down && v <= stick_release_threshold) {
      stick_dirs_down_.erase(dir.button);
      gamepad_button_up(dir.button);
    }
  }
}

void InputManager::press_actions(const std::vector<Action>& actions,
                                 Key trigger) {
  // Record held state for every action up front so polled state (is_pressed,
  // vec2 axes) does not depend on which listener consumed the press event.
  std::vector<Action> newly_down;
  newly_down.reserve(actions.size());
  for (Action a : actions) {
    if (actions_down_.insert(a).second) {
      newly_down.push_back(a);
    }
  }

  for (Action a : newly_down) {
    const bool was_text_active = text_active_;
    ActionEvent ev{
        .action = a,
        .state = ActionState{.pressed = true, .released = false, .held = true}};
    const bool delivered = dispatch_action(ev);

    // Suppress the backtick TEXTINPUT that follows a ToggleConsole keydown
    if (a == Action::ToggleConsole && (was_text_active || text_active_)) {
      suppress_next_textinput_ = true;
    }
    if (delivered) {
      if (repeats_while_held(a)) {
        arm_action_repeat(trigger, a);
      }
      break;
    }
  }
}

void InputManager::dispatch_action_release(Action a) {
  const bool was_down = actions_down_.erase(a) > 0;

  if (was_down) {
    ActionEvent ev{.action = a,
                   /*state=*/.state = ActionState{
                       .pressed = false, .released = true, .held = false}};
    dispatch_action(ev);
  }
}

bool InputManager::dispatch_action(const ActionEvent& ev) {
  switch (route_in_text_mode(ev.action)) {
  case TextRoute::Block:
    return false;
  case TextRoute::ToSinkExlusive:
    if (text_sink_) {
      text_sink_->on_action(ev);
    }
    return true;
  case TextRoute::Normal:
    break;
  }
  for (auto* l : listeners_) {
    if (l && l->on_action(ev)) {
      return true;
    }
  }
  return false;
}

void InputManager::arm_action_repeat(Key k, Action a) {
  if (k == Key::Unknown) {
    return;
  }
  repeat_ = KeyRepeat{.key = k, .action = a, .ms_left = repeat_delay_ms_};
}

void InputManager::arm_text_repeat(Key k, std::string text) {
  if (k == Key::Unknown) {
    return;
  }
  repeat_ =
      KeyRepeat{.key = k, .text = std::move(text), .ms_left = repeat_delay_ms_};
}

void InputManager::advance_key_repeat(uint32_t dt_ms) {
  if (repeat_.key == Key::Unknown) {
    return;
  }
  if (!keys_down_.contains(repeat_.key)) {
    repeat_ = {};
    return;
  }

  // A long frame yields a single repeat.
  repeat_.ms_left -= static_cast<int>(dt_ms);
  if (repeat_.ms_left > 0) {
    return;
  }
  repeat_.ms_left = repeat_interval_ms_;

  if (repeat_.action) {
    ActionEvent ev{
        .action = *repeat_.action,
        .state = ActionState{
            .pressed = true, .released = false, .held = true, .repeat = true}};
    dispatch_action(ev);
  }
  else {
    dispatch_text(repeat_.text);
  }
}

bool InputManager::dispatch_text(const std::string& utf8) {
  if (!text_sink_) {
    return false;
  }
  TextEvent tev{utf8};
  return text_sink_->on_text(tev);
}

float InputManager::axis_component(bool positive_down, bool negative_down) {
  return (positive_down ? 1.f : 0.f) - (negative_down ? 1.f : 0.f);
}

void InputManager::rebuild_axes() {
  for (const auto& c : vec2_components) {
    axes_[c.vec2] = {axis_component(is_pressed(c.right), is_pressed(c.left)),
                     axis_component(is_pressed(c.down), is_pressed(c.up))};
  }
}

void InputManager::debug_dump_bindings(std::ostream& os) const {
  os << "[buttons]\n";
  // Invert the table so we print grouped by action
  std::unordered_map<Action, std::vector<Binding>> by_action;
  by_action.reserve(button_bindings_.size());
  for (const auto& [bind, act] : button_bindings_) {
    by_action[act].push_back(bind);
  }
  for (const auto& [act, binds] : by_action) {
    os << "  " << std::string(magic_enum::enum_name(act)) << " = ";
    for (size_t i = 0; i < binds.size(); ++i) {
      os << chord_to_string(binds[i]);
      if (i + 1 < binds.size()) {
        os << ", ";
      }
    }
    os << "\n";
  }

  os << "[gamepad]\n";
  for (const auto& [btn, act] : gamepad_button_bindings_) {
    os << "  " << std::string(magic_enum::enum_name(act)) << " = Pad."
       << gamepad_button_to_string(btn) << "\n";
  }
}

void InputManager::debug_dump_state(std::ostream& os) const {
  os << "[text]\n";
  os << "  active = " << (text_active_ ? "true" : "false") << "\n";

  os << "[keys_down]\n  ";
  bool first = true;
  for (const auto& k : keys_down_) {
    if (!first) {
      os << ", ";
    }
    first = false;
    os << key_to_string(k);
  }
  if (first) {
    os << "(none)";
  }
  os << "\n";

  os << "[actions_down]\n  ";
  first = true;
  for (const auto& a : actions_down_) {
    if (!first) {
      os << ", ";
    }
    first = false;
    os << std::string(magic_enum::enum_name(a));
  }
  if (first) {
    os << "(none)";
  }
  os << "\n";

  os << "[axes]\n";
  for (const auto& [a, v] : axes_) {
    if (v.first == 0.f && v.second == 0.f) {
      continue; // only show active
    }
    os << "  " << std::string(magic_enum::enum_name(a)) << " = (" << v.first
       << ", " << v.second << ")\n";
  }
}

void InputManager::bind_button(Binding chord, Action a) {
  button_bindings_.emplace_back(chord, a);
  document_action(a);
}

void InputManager::bind_gamepad_button(GamepadButton b, Action a) {
  gamepad_button_bindings_.emplace_back(b, a);
  document_action(a);
}

void InputManager::bind_fixed(Binding chord, Action a) {
  button_bindings_.emplace_back(chord, a);
  fixed_actions_.insert(a);
}

bool InputManager::is_fixed(Action a) const {
  return fixed_actions_.contains(a);
}

void InputManager::declare_unbound(Action a) { document_action(a); }

void InputManager::begin_section(std::string label) {
  doc_sections_.push_back(
      BindingDocSection{documented_actions_.size(), std::move(label)});
}

void InputManager::document_action(Action a) {
  if (std::ranges::find(documented_actions_, a) == documented_actions_.end()) {
    documented_actions_.push_back(a);
  }
}

void InputManager::remove_bindings(Action a) {
  std::erase_if(button_bindings_,
                [a](const auto& entry) { return entry.second == a; });
  std::erase_if(gamepad_button_bindings_,
                [a](const auto& entry) { return entry.second == a; });
}

void InputManager::clear_bindings() {
  button_bindings_.clear();
  gamepad_button_bindings_.clear();
  documented_actions_.clear();
  doc_sections_.clear();
  fixed_actions_.clear();
}

} // namespace input
