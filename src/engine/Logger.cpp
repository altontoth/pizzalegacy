/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/Logger.h"
#include "engine/FormatCompat.h"

std::vector<Logger::SinkFunc>& Logger::sinks_() {
  static std::vector<SinkFunc> v;
  return v;
}

LogLevel& Logger::min_lvl_() {
#ifdef NDEBUG
  static LogLevel lvl = LogLevel::Warning;
#else
  static LogLevel lvl = LogLevel::Debug;
#endif
  return lvl;
}

Logger::TimePoint& Logger::start_time_() {
  static TimePoint t = Clock::now();
  return t;
}

Logger::TimePoint& Logger::last_time_() {
  static TimePoint t = Clock::now();
  return t;
}

void Logger::add_sink(SinkFunc sink) { sinks_().push_back(std::move(sink)); }

void Logger::clear_sinks() { sinks_().clear(); }

void Logger::global_min_level(LogLevel lvl) { min_lvl_() = lvl; }
LogLevel Logger::global_min_level() { return min_lvl_(); }

void Logger::log(LogLevel lvl, const std::string& msg) {
  if (lvl < min_lvl_()) {
    return;
  }

  auto now = Clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - start_time_());
  auto delta =
      std::chrono::duration_cast<std::chrono::microseconds>(now - last_time_());
  last_time_() = now;

  auto elapsed_s = elapsed.count() / 1000;
  auto elapsed_ms = elapsed.count() % 1000;
  auto delta_ms = delta.count() / 1000;
  auto delta_us = delta.count() % 1000;

  auto stamped = compat::format("[{}.{:03d} +{}.{:03d}ms] {}", elapsed_s,
                                elapsed_ms, delta_ms, delta_us, msg);

  for (auto& s : sinks_()) {
    s(lvl, stamped);
  }
}

std::string Logger::level_prefix(LogLevel lvl) {
  switch (lvl) {
  case LogLevel::Debug:
    return "(DEBUG) ";
  case LogLevel::Info:
    return "(INFO) ";
  case LogLevel::Warning:
    return "(WARN) ";
  }
  return {};
}
