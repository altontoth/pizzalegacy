/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include <string_view>

namespace debug {

// Compile-time type name extraction (no allocation)
template <class T> constexpr std::string_view type_name() {
#if defined(__clang__) || defined(__GNUC__)
  std::string_view p = __PRETTY_FUNCTION__;
  auto s = p.find("T = ");
  auto e = p.find(']', s);
  return p.substr(s + 4, e - (s + 4));
#elif defined(_MSC_VER)
  std::string_view p = __FUNCSIG__;
  auto s = p.find("type_name<") + 10;
  auto e = p.find(">(void)", s);
  return p.substr(s, e - s);
#else
  return "unknown";
#endif
}

constexpr std::string_view short_name(std::string_view fq) {
  // strip namespaces: last "::"
  auto pos = fq.rfind("::");
  return pos == std::string_view::npos ? fq : fq.substr(pos + 2);
}

// Minimal interface to query at runtime.
struct debug_named {
  virtual ~debug_named() = default;
  virtual std::string_view debug_name() const noexcept = 0;
};

// CRTP mixin: provides debug_name(); optional per-instance label.
template <class Derived> class AutoLabel : public debug_named {
public:
  AutoLabel() = default;
  explicit AutoLabel(std::string label) : label_(std::move(label)) {}

  std::string_view debug_name() const noexcept override {
    return label_.empty() ? short_name(type_name<Derived>())
                          : std::string_view(label_);
  }
  void set_label(std::string s) { label_ = std::move(s); }

private:
  std::string label_; // empty = use type name
};

// Helper: get a label from anything (falls back if object isn't debug_named)
template <class T>
  requires(!std::is_pointer_v<std::remove_reference_t<T>>)
std::string_view label_of(const T& x) {
  if (const auto* n = dynamic_cast<const debug_named*>(&x)) {
    return n->debug_name();
  }
  return "(unnamed)";
}

// Raw pointer overload
template <class T> std::string_view label_of(const T* p) {
  if (!p) {
    return "(null)";
  }
  if (const auto* n = dynamic_cast<const debug_named*>(p)) {
    return n->debug_name();
  }
  return "(unnamed)";
}

} // namespace debug
