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
#include <cmath>
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

static inline bool is_vec2_action(Action a) noexcept {
  switch (a) {
  case Action::MoveVec2:
  case Action::P2_MoveVec2:
  case Action::ScrollVec2:
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

ListenerToken InputManager::register_listener(InputListener& l) {
  if (std::find(listeners_.begin(), listeners_.end(), &l) == listeners_.end()) {
    listeners_.push_back(&l);
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
    dispatch_text(ev.text.text);
    break;
  }

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
      // Release any stick-driven axes so movement stops on disconnect
      for (auto& [action, vec] : gamepad_axes_) {
        vec = {0.f, 0.f};
      }
    }
    break;
  }

  case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
    // SDL fires one event per axis, but a stick is two axes. Re-sample every
    // bound stick from the controller so each entry holds a consistent {x, y}.
    if (gamepad_) {
      auto* pad = static_cast<SDL_Gamepad*>(gamepad_);
      constexpr float AXIS_MAX = 32767.f;
      // TODO: Maybe have an option for configuring the deadzone?
      constexpr float DEAD_ZONE = 4000.f / AXIS_MAX;
      const auto sample = [&](GamepadAxis a) -> float {
        const SDL_GamepadAxis sdl_axis = backend::axis_to_sdl(a);
        if (sdl_axis == SDL_GAMEPAD_AXIS_INVALID) {
          return 0.f;
        }
        const float v =
            static_cast<float>(SDL_GetGamepadAxis(pad, sdl_axis)) / AXIS_MAX;
        return (std::abs(v) < DEAD_ZONE) ? 0.f : v;
      };
      for (const auto& [action, stick] : gamepad_stick_bindings_) {
        gamepad_axes_[action] = {sample(stick.x), sample(stick.y)};
      }
    }
    break;
  }

  case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
  case SDL_EVENT_GAMEPAD_BUTTON_UP: {
    const GamepadButton b = backend::button_from_sdl(
        static_cast<SDL_GamepadButton>(ev.gbutton.button));
    if (auto it = gamepad_button_bindings_.find(b);
        it != gamepad_button_bindings_.end()) {
      if (ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        dispatch_action_press(it->second);
      }
      else {
        dispatch_action_release(it->second);
      }
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
  evaluate_bindings_for_key_down(k, mods);
}

void InputManager::key_up(Key k, Mod mods) {
  keys_down_.erase(k);
  mods_down_ = mods;

  evaluate_bindings_for_key_up(k, mods);
}

void InputManager::evaluate_bindings_for_key_down(Key k, Mod mods) {
  Binding b = lookup_binding(k, mods);

  auto range = button_bindings_.equal_range(b);
  for (auto it = range.first; it != range.second; ++it) {
    Action a = it->second;
    const bool was_text_active = text_active_;
    const bool consumed = dispatch_action_press(a);

    // Suppress the backtick TEXTINPUT that follows a ToggleConsole keydown
    if (a == Action::ToggleConsole && (was_text_active || text_active_)) {
      suppress_next_textinput_ = true;
    }
    if (consumed) {
      break;
    }
  }
}

void InputManager::evaluate_bindings_for_key_up(Key k, Mod mods) {
  Binding b = lookup_binding(k, mods);

  auto range = button_bindings_.equal_range(b);
  for (auto it = range.first; it != range.second; ++it) {
    Action a = it->second;

    if (a == Action::Exit && text_active_) {
      a = Action::CancelInput;
    }

    dispatch_action_release(a);
  }
}

bool InputManager::dispatch_action_press(Action a) {
  const bool was_down = actions_down_.find(a) != actions_down_.end();
  actions_down_.insert(a);

  if (!was_down) {
    ActionEvent ev{
        .action = a,
        .state = ActionState{.pressed = true, .released = false, .held = true}};
    return dispatch_action(ev);
  }
  return false;
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
    return true;
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

std::pair<float, float>
InputManager::compute_axis_from_keys(const KeyVec2& kv) const {
  const bool l = keys_down_.contains(kv.left);
  const bool r = keys_down_.contains(kv.right);
  const bool u = keys_down_.contains(kv.up);
  const bool d = keys_down_.contains(kv.down);
  std::pair<float, float> v{axis_component(r, l), axis_component(d, u)};
  return v;
}

void InputManager::rebuild_axes() {
  for (const auto& [act, kv] : axis_key_bindings_) {
    // Zero out gameplay axes while typing
    if (text_active_ && !is_vec2_action(act)) {
      axes_[act] = {0.f, 0.f};
      continue;
    }
    axes_[act] = compute_axis_from_keys(kv);
  }
  // Keyboard and gamepad can both contribute to the same action. For each
  // component, keep whichever input is farther from zero.
  for (const auto& [act, gv] : gamepad_axes_) {
    if (text_active_ && !is_vec2_action(act)) {
      continue;
    }
    auto& dst = axes_[act];
    if (std::abs(gv.first) > std::abs(dst.first)) {
      dst.first = gv.first;
    }
    if (std::abs(gv.second) > std::abs(dst.second)) {
      dst.second = gv.second;
    }
  }
}

void InputManager::debug_dump_bindings(std::ostream& os) const {
  os << "[buttons]\n";
  // Invert multimap so we print grouped by action
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

  os << "[axes]\n";
  for (const auto& [act, kv] : axis_key_bindings_) {
    os << "  " << std::string(magic_enum::enum_name(act)) << " = Vec2Keys("
       << key_to_string(kv.up) << "," << key_to_string(kv.down) << ","
       << key_to_string(kv.left) << "," << key_to_string(kv.right) << ")\n";
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
  button_bindings_.emplace(chord, a);
}

void InputManager::bind_axis_keys(Action a, Key up, Key down, Key left,
                                  Key right) {
  axis_key_bindings_.emplace(
      a, KeyVec2{.up = up, .down = down, .left = left, .right = right});
}

void InputManager::bind_gamepad_button(GamepadButton b, Action a) {
  gamepad_button_bindings_[b] = a;
}

void InputManager::bind_gamepad_stick(Action a, GamepadAxis x_axis,
                                      GamepadAxis y_axis) {
  gamepad_stick_bindings_[a] = AxisVec2{.x = x_axis, .y = y_axis};
}

void InputManager::clear_bindings() {
  button_bindings_.clear();
  axis_key_bindings_.clear();
  gamepad_button_bindings_.clear();
  gamepad_stick_bindings_.clear();
}

} // namespace input
