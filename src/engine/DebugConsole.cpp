/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/DebugConsole.h"
#include "core/debug/command/CommandRegistry.h"
#include "core/state/GameState.h"
#include "core/util/GameContext.h"

void DebugConsole::init(debug::command::CommandRegistry& registry,
                        GameState& state, GameContext& context) {
  registry_ = &registry;
  state_ = &state;
  context_ = &context;
}

void DebugConsole::execute_command(const std::string& line) {
  add_line(ConsoleLineType::Command, "> " + line);

  if (line.empty() || line.find_first_not_of(' ') == std::string::npos) {
    add_line(ConsoleLineType::Result, "");
    return;
  }

  if (!registry_ || !state_ || !context_) {
    add_line(ConsoleLineType::Result, "Console not initialized");
    return;
  }

  report_result(registry_->execute(line, *state_, *context_));
}

void DebugConsole::execute_command(const std::vector<std::string>& args) {
  std::string display;
  for (const auto& arg : args) {
    if (!display.empty()) {
      display += ' ';
    }
    display += arg;
  }
  add_line(ConsoleLineType::Command, "> " + display);

  if (args.empty()) {
    add_line(ConsoleLineType::Result, "");
    return;
  }

  if (!registry_ || !state_ || !context_) {
    add_line(ConsoleLineType::Result, "Console not initialized");
    return;
  }

  report_result(registry_->execute(args, *state_, *context_));
}

void DebugConsole::report_result(const debug::command::CommandResult& result) {
  if (result.error_message) {
    add_line(ConsoleLineType::Result, *result.error_message);
    Logger::warn("console: {}", *result.error_message);
    if (result.error_suggestion) {
      add_line(ConsoleLineType::Result, *result.error_suggestion);
      Logger::warn("console: {}", *result.error_suggestion);
    }
  }
  else {
    for (const auto& msg : result.messages) {
      add_line(ConsoleLineType::Result, msg);
    }
  }
}

void DebugConsole::add_line(ConsoleLineType t, std::string text) {
  if (lines_.size() >= max_lines) {
    lines_.pop_front();
  }
  lines_.push_back(ConsoleLine{.type = t, .text = std::move(text)});
}

void DebugConsole::log(LogLevel lvl, const std::string& msg) {
  if (lvl < min_level_) {
    return;
  }
  add_line(ConsoleLineType::Output, Logger::level_prefix(lvl) + msg);
}

void DebugConsole::set_min_level(LogLevel lvl) { min_level_ = lvl; }
LogLevel DebugConsole::min_level() const { return min_level_; }

std::function<void(LogLevel, const std::string&)> DebugConsole::make_sink() {
  return [this](LogLevel lvl, const std::string& msg) { this->log(lvl, msg); };
}
