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

#include "engine/input/backend/SDLKeymap.h"

#include <algorithm>
#include <iostream>
#include <utility>
#include "engine/SDL_include.h"
#include "magic_enum/magic_enum.hpp"

namespace input {

static inline Mod mods_from_sdl(Uint16 sdl_mod) noexcept {
  Mod m = Mod::None;
  if (sdl_mod & KMOD_SHIFT) {
    m = m | Mod::Shift;
  }
  if (sdl_mod & KMOD_CTRL) {
    m = m | Mod::Ctrl;
  }
  if (sdl_mod & KMOD_ALT) {
    m = m | Mod::Alt;
  }
  if (sdl_mod & KMOD_GUI) {
    m = m | Mod::Meta;
  }
  return m;
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
  SDL_StartTextInput();
  return TextSession{this};
}

void InputManager::end_text_input() {
  if (!text_active_) {
    return;
  }
  SDL_StopTextInput();
  text_active_ = false;
  text_sink_ = nullptr;
}

bool InputManager::text_input_active() const noexcept { return text_active_; }

bool InputManager::is_pressed(Action a) const noexcept {
  return actions_down_.find(a) != actions_down_.end();
}

bool InputManager::just_pressed(Action a) const noexcept {
  return actions_just_pressed_.find(a) != actions_just_pressed_.end();
}

std::pair<float, float> InputManager::axis(Action a) const noexcept {
  if (auto it = axes_.find(a); it != axes_.end()) {
    return it->second;
  }
  return {0.f, 0.f};
}

void InputManager::tick() {
  rebuild_axes();
  actions_just_pressed_.clear();
  suppress_next_textinput_ = false;
}

void InputManager::process(const SDL_Event& ev) {
  switch (ev.type) {
  case SDL_KEYDOWN:
  case SDL_KEYUP: {
    const bool down = (ev.type == SDL_KEYDOWN);
    const Key k = backend::key_from_scancode(ev.key.keysym.scancode);
    const Mod m = mods_from_sdl(ev.key.keysym.mod);

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

  case SDL_TEXTINPUT: {
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

  case SDL_CONTROLLERAXISMOTION:
  case SDL_CONTROLLERBUTTONDOWN:
  case SDL_CONTROLLERBUTTONUP:
    // TODO: when we wire gamepad support, update axes_ / actions_ here
    break;

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
  Binding b{mods, k};

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
  Binding b{mods, k};

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
    actions_just_pressed_.insert(a);

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
  // TODO: merge in gamepad analog sticks here (deadzone, normalize)
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

  os << "[actions_just_pressed]\n  ";
  first = true;
  for (const auto& a : actions_just_pressed_) {
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

void InputManager::clear_bindings() {
  button_bindings_.clear();
  axis_key_bindings_.clear();
}

} // namespace input
