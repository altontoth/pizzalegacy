/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include <vector>

enum class StatusAlignment { Left, Center, Right };
enum class StatusItemKind { Header, TwoColumn, Single, Spacer };

struct StatusColumn {
  std::string text;
  int x = 0;
  StatusAlignment align = StatusAlignment::Left;
};

struct StatusItem {
  StatusItemKind kind;
  StatusColumn col1; // Header/Single: text; TwoColumn: left column
  StatusColumn col2; // TwoColumn: right column
  StatusAlignment align = StatusAlignment::Left; // for Header/Single (legacy)
  int lines = 1;                                 // for Spacer
  int color = 0;                                 // 0 = default color
};

class StatusDisplayRecipe {
public:
  void add_header(std::string_view text,
                  StatusAlignment a = StatusAlignment::Left, int color = 0) {
    items_.push_back(
        {StatusItemKind::Header, {std::string(text), 0, a}, {}, a, 1, color});
  }
  void add_two_column(std::string_view label, std::string_view value) {
    items_.push_back({StatusItemKind::TwoColumn,
                      {std::string(label)},
                      {std::string(value), 110}});
  }
  void add_two_column(StatusColumn col1, StatusColumn col2) {
    items_.push_back(
        {StatusItemKind::TwoColumn, std::move(col1), std::move(col2)});
  }
  void add_single(std::string_view text,
                  StatusAlignment a = StatusAlignment::Left, int color = 0) {
    items_.push_back(
        {StatusItemKind::Single, {std::string(text), 0, a}, {}, a, 1, color});
  }
  void add_spacer(int lines = 1) {
    items_.push_back(
        {StatusItemKind::Spacer, {}, {}, StatusAlignment::Left, lines});
  }
  void append(const StatusDisplayRecipe& other) {
    items_.insert(items_.end(), other.items().begin(), other.items().end());
  }

  void clear() { items_.clear(); }

  [[nodiscard]] const std::vector<StatusItem>& items() const noexcept {
    return items_;
  }

private:
  std::vector<StatusItem> items_;
};
