/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/finance/MoneyFormatter.h"
#include "engine/FormatCompat.h"

struct Money {
  int64_t cents{0};

  Money() = default;
  constexpr Money(int64_t c) : cents(c) {}

  constexpr Money operator+(Money other) const {
    return Money{cents + other.cents};
  }
  constexpr Money operator-(Money other) const {
    return Money{cents - other.cents};
  }
  constexpr Money operator-() const { return Money{-cents}; }

  constexpr operator int64_t() const { return cents; }

  constexpr auto operator<=>(const Money&) const = default;

  constexpr Money& operator-=(Money other) {
    cents -= other.cents;
    return *this;
  }

  constexpr Money& operator+=(Money other) {
    cents += other.cents;
    return *this;
  }

  template <std::integral I> constexpr Money& operator*=(I n) {
    cents = static_cast<int64_t>(cents * static_cast<long long>(n));
    return *this;
  }

  template <std::integral I> constexpr Money& operator/=(I n) {
    cents = static_cast<int64_t>(cents / static_cast<long long>(n));
    return *this;
  }

  [[nodiscard]] constexpr int64_t whole() const noexcept { return cents / 100; }

  [[nodiscard]] constexpr int64_t fractional() const noexcept {
    auto r = cents % 100;
    return r < 0 ? -r : r;
  }
};

constexpr Money operator""_usd(unsigned long long dollars) {
  return Money{static_cast<int64_t>(dollars * 100)};
}

constexpr Money operator""_c(unsigned long long cents) {
  return Money{static_cast<int64_t>(cents)};
}

inline const MoneyFormatter& get_money_formatter() {
  // We can later make this return a per-locale or per-setting value.
  static const MoneyFormatter formatter{"$", false}; // e.g., "$ 123.45"
  return formatter;
}

template <std::integral I> constexpr Money operator*(Money m, I n) {
  m *= n;
  return m;
}
template <std::integral I> constexpr Money operator*(I n, Money m) {
  m *= n;
  return m;
}

template <std::integral I> constexpr Money operator/(Money m, I n) {
  m /= n;
  return m;
}

// {:s} - with currency symbol (default) "$123"
// {:n} - number only "123.45"
// {:c} - raw minor units (cents) "12345"
// {:w} - whole dollars with symbol "$123" (no cents)
// {:i} - integer only "123"

template <> struct FORMAT_NAMESPACE::formatter<Money> {
  enum class style { symbol, number, cents, whole, integer };
  style how{style::whole}; // default to whole

  constexpr auto parse(FORMAT_NAMESPACE::format_parse_context& ctx) {
    auto it = ctx.begin();
    if (it != ctx.end() && *it != '}') {
      // spec present
      switch (*it) {
      case 's':
        how = style::symbol;
        break;
      case 'n':
        how = style::number;
        break;
      case 'c':
        how = style::cents;
        break;
      case 'w':
        how = style::whole;
        break;
      case 'i':
        how = style::integer;
        break;
      default:
        throw FORMAT_NAMESPACE::format_error("invalid Money format");
      }
      // consume the letter
      ++it;
    }
    if (it != ctx.end() && *it != '}') {
      throw FORMAT_NAMESPACE::format_error("expected '}'");
    }
    return it;
  }

  template <typename Ctx> auto format(const Money& m, Ctx& ctx) const {
    switch (how) {
    case style::integer:
      return FORMAT_NAMESPACE::format_to(ctx.out(), "{:.0f}", m.cents / 100.0);
    case style::symbol:
      return FORMAT_NAMESPACE::format_to(ctx.out(), "{}",
                                         get_money_formatter().format(m.cents));
    case style::cents:
      return FORMAT_NAMESPACE::format_to(ctx.out(), "{}", m.cents);
    case style::whole:
      // Round half away from zero (works for both positive and negative)
      return FORMAT_NAMESPACE::format_to(ctx.out(), "${}",
                                         m.cents >= 0 ? (m.cents + 50) / 100
                                                      : (m.cents - 50) / 100);
    case style::number:
    default:
      return FORMAT_NAMESPACE::format_to(ctx.out(), "{:.2f}", m.cents / 100.0);
    }
  }
};
