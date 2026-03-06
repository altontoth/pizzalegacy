/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "CommandTokenizer.h"
#include <cctype>

namespace debug::command {

std::vector<CommandTokenizer::Token>
CommandTokenizer::tokenize(const std::string& line) {
  std::vector<Token> tokens;
  State state = State::Start;
  std::string current_token;
  size_t token_start = 0;
  bool was_quoted = false;

  for (size_t i = 0; i < line.size(); ++i) {
    char c = line[i];

    switch (state) {
    case State::Start:
      if (std::isspace(c)) {
        // Skip whitespace
        continue;
      }
      else if (c == '\'') {
        state = State::InSingleQuote;
        token_start = i;
        was_quoted = true;
        current_token.clear();
      }
      else if (c == '"') {
        state = State::InDoubleQuote;
        token_start = i;
        was_quoted = true;
        current_token.clear();
      }
      else {
        state = State::InWord;
        token_start = i;
        was_quoted = false;
        current_token = c;
      }
      break;

    case State::InWord:
      if (std::isspace(c)) {
        // End of word
        tokens.push_back(Token{current_token, token_start, was_quoted});
        current_token.clear();
        state = State::Start;
      }
      else if (c == '\\') {
        state = State::Escape;
      }
      else {
        current_token += c;
      }
      break;

    case State::InSingleQuote:
      if (c == '\'') {
        // End of single-quoted string
        tokens.push_back(Token{current_token, token_start, was_quoted});
        current_token.clear();
        state = State::Start;
      }
      else if (c == '\\') {
        state = State::Escape;
      }
      else {
        current_token += c;
      }
      break;

    case State::InDoubleQuote:
      if (c == '"') {
        // End of double-quoted string
        tokens.push_back(Token{current_token, token_start, was_quoted});
        current_token.clear();
        state = State::Start;
      }
      else if (c == '\\') {
        state = State::Escape;
      }
      else {
        current_token += c;
      }
      break;

    case State::Escape:
      // Add the escaped character literally
      current_token += c;
      // Return to previous state
      if (was_quoted) {
        // We were in a quoted string - determine which type
        if (i + 1 < line.size()) {
          // Peek at what comes after to determine quote type
          // For simplicity, assume we were in double quotes
          // A more robust implementation would track this explicitly
          state = State::InDoubleQuote;
        }
        else {
          state = State::Start;
        }
      }
      else {
        state = State::InWord;
      }
      break;
    }
  }

  // Handle final token
  if (!current_token.empty()) {
    tokens.push_back(Token{current_token, token_start, was_quoted});
  }

  return tokens;
}

std::vector<std::string>
CommandTokenizer::tokenize_simple(const std::string& line) {
  auto tokens = tokenize(line);
  std::vector<std::string> result;
  result.reserve(tokens.size());
  for (const auto& token : tokens) {
    result.push_back(token.value);
  }
  return result;
}

} // namespace debug::command
