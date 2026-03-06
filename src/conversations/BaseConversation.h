/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "core/finance/Money.h"
#include "core/state/GameState.h"
#include "resources/util/FileUtils.h"

class Actor;

struct ContractTermsDisplay {
  Money sum;
  int discount_pct;
  Money charge;
  Money penalty;
  int term_days;
};

enum class Emotion {
  NEUTRAL,
  ANGRY,
  HAPPY,
};

// Generic ostream operator for enum class
template <typename Enum, typename = std::enable_if_t<std::is_enum_v<Enum>>>
std::ostream& operator<<(std::ostream& os, const Enum& e) {
  os << static_cast<std::underlying_type_t<Enum>>(e);
  return os;
}

template <typename LineEnum> struct ConversationLine {
  std::string text;
  bool is_option;
  bool is_proposal;
  bool is_easy_accept;
  bool is_terminal;
  LineEnum line_id;
};

template <typename LineEnum>
inline std::ostream& operator<<(std::ostream& os,
                                const ConversationLine<LineEnum>& line) {
  // Cast line_id to its underlying integer type for printing:
  os << "{ text: \"" << line.text << "\", is_option: " << std::boolalpha
     << line.is_option << "\", is_proposal: " << std::boolalpha
     << line.is_proposal << "\", is_easy_accept: " << std::boolalpha
     << line.is_easy_accept << "\", is_terminal: " << std::boolalpha
     << line.is_terminal << ", line_id: " << static_cast<int>(line.line_id)
     << " }";
  return os;
}

struct LineOptions {
  bool is_option = false;
  bool is_proposal = false;
  bool easy_accept = true;
  bool is_terminal = false;
  std::string custom_text;
  bool prepend_text = false;
};

template <typename LineEnum> class BaseConversation {
public:
  using line_enum = LineEnum;
  // Constructs a conversation starting at a given line.
  BaseConversation(LineEnum start, GameState& state,
                   const std::string& file_path, const Actor* actor)
      : state_(state), current_line_(start), actor_(actor) {
    load_text_file(file_path);
  }

  virtual ~BaseConversation() = default;

  // Auto advance (no user input needed)
  virtual bool advance() = 0;
  // Advance given a choice
  virtual bool advance_line_choice(LineEnum choice) = 0;
  // Advance for boolean choice
  virtual bool advance_proposal(bool agree) { return false; }
  // Get lines for current state_
  virtual std::vector<ConversationLine<LineEnum>> lines() const = 0;

  // Set the current conversation state.
  // (this does not directly control what text is shown on screen; the
  // `lines()` method will use the `current_line_` enum to decide which
  // `ConversationLine` objects to return. This allows `lines()` to return
  // dynamically constructed lines (e.g., with formatted text), while the enum
  // still acts as a symbolic state key for the conversation logic.)
  void current_line(LineEnum line) {
    previous_line_ = current_line_;
    current_line_ = line;
  }

  virtual Emotion emotion() const = 0;
  virtual bool has_ended() const = 0;
  virtual std::optional<ContractTermsDisplay> contract_terms() const {
    return std::nullopt;
  }
  [[nodiscard]] bool is_proposal() const {
    return !lines().empty() && lines().at(0).is_proposal;
  }

  [[nodiscard]] bool is_option() const {
    return !lines().empty() && lines().at(0).is_option;
  }

  [[nodiscard]] bool is_terminal() const {
    return !lines().empty() && lines().at(0).is_terminal;
  }

protected:
  // Helper to create a ConversationLine from the loaded text vector.
  [[nodiscard]] ConversationLine<LineEnum>
  make_line(LineEnum line_id, const LineOptions& opts = {}) const {

    std::string file_line;
    int idx = static_cast<int>(line_id);
    // Some 'magic' lines are beyond the file length, so
    // check if we're within bounds and find the line,
    // and if we're not within bounds then just leave the
    // empty string. If there is no custom text then there
    // won't be anything to show so we throw an error.
    if (idx >= 0 && idx < static_cast<int>(lines_.size())) {
      file_line = lines_.at(idx);
    }
    else if (opts.custom_text.empty()) {
      // Safety check if the enum is out of range and no custom text:
      std::cerr << "Invalid line index " << idx << " in make_line()" << '\n';
      return ConversationLine<LineEnum>{
          "[INVALID LINE]", false, false, false, false, line_id};
    }

    // Retrieve base text and optionally append/prepend custom_text
    std::string text;
    if (opts.prepend_text) {
      if (!opts.custom_text.empty()) {
        text += opts.custom_text;
      }
      text += file_line;
    }
    else {
      text += file_line;
      if (!opts.custom_text.empty()) {
        text += opts.custom_text;
      }
    }

    return ConversationLine<LineEnum>{text,
                                      opts.is_option,
                                      opts.is_proposal,
                                      opts.easy_accept,
                                      opts.is_terminal,
                                      line_id};
  }

  [[nodiscard]] ConversationLine<LineEnum> make_option(LineEnum line_id) const {
    return make_line(line_id, {.is_option = true});
  }

  [[nodiscard]] ConversationLine<LineEnum>
  make_proposal(LineEnum line_id, std::string text = "") const {
    return make_line(line_id,
                     {.is_proposal = true, .custom_text = std::move(text)});
  }

  [[nodiscard]] ConversationLine<LineEnum>
  make_proposal(LineEnum line_id, bool easy_accept) const {
    return make_line(line_id,
                     {.is_proposal = true, .easy_accept = easy_accept});
  }

  [[nodiscard]] ConversationLine<LineEnum>
  make_text_line(LineEnum line_id, std::string text) const {
    return make_line(line_id, {.is_proposal = true,
                               .custom_text = std::move(text),
                               .prepend_text = true});
  }

  [[nodiscard]] ConversationLine<LineEnum>
  make_terminal_line(LineEnum line_id, std::string custom_text = "") const {
    return make_line(
        line_id, {.is_terminal = true, .custom_text = std::move(custom_text)});
  }

  [[nodiscard]] ConversationLine<LineEnum>
  make_concat_proposal(LineEnum first_line, const std::string& middle_text,
                       LineEnum second_line) const {
    // We only need second and third because those will be added to the first by
    // make_line
    std::string second_text = lines_.at(static_cast<int>(second_line));
    std::string full_text = middle_text + second_text;
    return make_line(first_line, {.is_option = false,
                                  .is_proposal = true,
                                  .easy_accept = false,
                                  .is_terminal = false,
                                  .custom_text = std::move(full_text),
                                  .prepend_text = false});
  }

  void load_text_file(const std::string& file_path) {
    std::ifstream file = fileutils::open(file_path);
    lines_.clear();
    std::string line;
    while (std::getline(file, line)) {
      lines_.push_back(line);
    }
  }

  GameState& state_;
  LineEnum current_line_;
  LineEnum previous_line_;
  std::vector<std::string> lines_;
  const Actor* actor_;
};
