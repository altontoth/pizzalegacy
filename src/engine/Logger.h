/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <vector>
#include "engine/FormatCompat.h"

enum class LogLevel { Debug = 0, Info = 1, Warning = 2 };

class Logger {
public:
  using SinkFunc = std::function<void(LogLevel, const std::string&)>;
  static void add_sink(SinkFunc);
  static void clear_sinks();

  static void global_min_level(LogLevel);
  static LogLevel global_min_level();

  static void log(LogLevel, const std::string& msg);

  template <typename... Args>
  static void debug(compat::format_string<Args...> fmt, Args&&... args) {
    log(LogLevel::Debug, compat::format(fmt, std::forward<Args>(args)...));
  }

  template <typename... Args>
  static void info(compat::format_string<Args...> fmt, Args&&... args) {
    log(LogLevel::Info, compat::format(fmt, std::forward<Args>(args)...));
  }

  template <typename... Args>
  static void warn(compat::format_string<Args...> fmt, Args&&... args) {
    log(LogLevel::Warning, compat::format(fmt, std::forward<Args>(args)...));
  }

  static std::string level_prefix(LogLevel);

private:
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;

  static std::vector<SinkFunc>& sinks_();
  static LogLevel& min_lvl_();
  static TimePoint& start_time_();
  static TimePoint& last_time_();
};
