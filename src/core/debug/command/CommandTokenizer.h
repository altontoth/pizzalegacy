/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include <vector>

namespace debug::command {

// Tokenizer for command-line input with quoted string support
class CommandTokenizer {
public:
  // Token with metadata
  struct Token {
    std::string value;
    size_t position; // Character position in original string
    bool was_quoted; // Was this token in quotes?
  };

  // Tokenize with full metadata
  static std::vector<Token> tokenize(const std::string& line);

  // Simple tokenization (just values)
  static std::vector<std::string> tokenize_simple(const std::string& line);

private:
  enum class State { Start, InWord, InSingleQuote, InDoubleQuote, Escape };
};

} // namespace debug::command
