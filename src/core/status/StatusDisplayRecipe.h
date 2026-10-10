/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
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
  uint8_t color = 0;                             // 0 = default color
  // The name of an interaction item covering this line.
  std::optional<std::string> interaction_item;
};

class StatusDisplayRecipe {
public:
  void add_header(std::string_view text,
                  StatusAlignment a = StatusAlignment::Left,
                  uint8_t color = 0) {
    items_.push_back({.kind = StatusItemKind::Header,
                      .col1 = {std::string(text), 0, a},
                      .align = a,
                      .color = color});
  }
  void add_two_column(std::string_view label, std::string_view value) {
    items_.push_back({.kind = StatusItemKind::TwoColumn,
                      .col1 = {std::string(label)},
                      .col2 = {std::string(value), 110}});
  }
  void add_two_column(StatusColumn col1, StatusColumn col2) {
    items_.push_back({.kind = StatusItemKind::TwoColumn,
                      .col1 = std::move(col1),
                      .col2 = std::move(col2)});
  }
  void add_single(std::string_view text,
                  StatusAlignment a = StatusAlignment::Left,
                  uint8_t color = 0) {
    items_.push_back({.kind = StatusItemKind::Single,
                      .col1 = {std::string(text), 0, a},
                      .align = a,
                      .color = color});
  }
  void add_spacer(int lines = 1) {
    items_.push_back({.kind = StatusItemKind::Spacer, .lines = lines});
  }
  void last_line_interaction_item(std::string name) {
    items_.back().interaction_item = std::move(name);
  }
  void clear() { items_.clear(); }

  [[nodiscard]] const std::vector<StatusItem>& items() const noexcept {
    return items_;
  }

private:
  std::vector<StatusItem> items_;
};
