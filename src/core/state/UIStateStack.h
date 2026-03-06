/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <cassert>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
#include "core/state/UIState.h"

class UIStateStack {
public:
  template <class S, class... Args>
    requires std::is_constructible_v<S, Args...>
  void push(Args&&... args) {
    // stack_.emplace_back(S{std::forward<Args>(args)...});
    stack_.emplace_back(std::in_place_type<S>, std::forward<Args>(args)...);
    changed_ = true;
  }
  void push_screen(ScreenTag::Kind k) { push<ScreenTag>(k); }
  // pop the top node
  void pop() {
    if (stack_.empty()) {
      assert("You popped while the stack was empty");
    }
    stack_.pop_back();
    if (stack_.empty()) {
      assert("You popped which made the stack empty");
    }
    changed_ = true;
  }

  size_t size() const { return stack_.size(); }

  // Replace the element that is currently on top of the stack.
  // If the stack is empty this behaves like push()
  template <class S, class... Args>
    requires std::is_constructible_v<S, Args...>
  void replace_top(Args&&... args) {
    if (stack_.empty()) {
      // stack_.emplace_back(S{std::forward<Args>(args)...});
      stack_.emplace_back(std::in_place_type<S>, std::forward<Args>(args)...);
    }
    else {
      // stack_.back() = S{std::forward<Args>(args)...};
      stack_.back().template emplace<S>(std::forward<Args>(args)...);
    }
    changed_ = true;
  }

  // variant access helpers
  template <class S> S* top_as() { return std::get_if<S>(&stack_.back()); }
  template <class S> const S* top_as() const {
    return std::get_if<S>(&stack_.back());
  }

  template <class S> bool is_top() const {
    return std::holds_alternative<S>(stack_.back());
  }

  // change-tracking
  bool ui_changed() { return std::exchange(changed_, false); }

  const UIState& top() const { return stack_.back(); }

private:
  // start with "no screen":
  std::vector<UIState> stack_{std::monostate{}};
  // first call builds initial view
  bool changed_{true};
};
