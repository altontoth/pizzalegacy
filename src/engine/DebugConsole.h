/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <deque>
#include <functional>
#include <string>
#include <vector>
#include "engine/Logger.h"

// Forward declarations
class GameState;
class GameContext;
namespace debug::command {
class CommandRegistry;
struct CommandResult;
} // namespace debug::command

enum class ConsoleLineType { Command, Result, Output };

struct ConsoleLine {
  ConsoleLineType type;
  std::string text;
};

class DebugConsole {
public:
  // Initialize with registry and context references
  void init(debug::command::CommandRegistry& registry, GameState& state,
            GameContext& context);

  // Execute a command line (delegates to registry)
  void execute_command(const std::string& line);

  // Execute a command from already separated arguments, e.g. argv
  void execute_command(const std::vector<std::string>& args);

  // history
  [[nodiscard]] const std::deque<ConsoleLine>& lines() const { return lines_; }

  // log sink
  void log(LogLevel lvl, const std::string& msg);
  void set_min_level(LogLevel);
  [[nodiscard]] LogLevel min_level() const;

  // utility to register with Logger:  Logger::add_sink(dc.make_sink());
  std::function<void(LogLevel, const std::string&)> make_sink();

private:
  void add_line(ConsoleLineType, std::string text);
  void report_result(const debug::command::CommandResult& result);

  debug::command::CommandRegistry* registry_{nullptr};
  GameState* state_{nullptr};
  GameContext* context_{nullptr};

  std::deque<ConsoleLine> lines_;
  LogLevel min_level_{LogLevel::Debug};

  static constexpr std::size_t max_lines = 2000;
};
