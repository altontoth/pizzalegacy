/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <cstddef>
#include <exception>
#include <magic_enum/magic_enum.hpp>
#include <map>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <string_view>

#include "engine/Fatal.h"
#include "engine/FormatCompat.h"

// The game's data tables, shipped as JSON in the data directory.
namespace data_file {

using json = nlohmann::json;

// Parses the named file from the data directory and returns its object, or
// calls fatal if there are any issues with the file.
json parse_file(std::string_view file_name);

// Reads every entry of the named file through convert. Each key names a value
// of E, and convert is handed that value along with the entry. The file has to
// name every value of E, and the entries come back keyed by it, in enum order.
template <typename T, typename E, typename Convert>
std::map<E, T> load(std::string_view file_name, Convert convert) {
  const json entries = parse_file(file_name);
  std::map<E, T> out;
  for (const auto& [key, entry] : entries.items()) {
    const auto type = magic_enum::enum_cast<E>(key);
    if (!type) {
      fatal(compat::format("{}: unknown entry '{}'", file_name, key));
    }
    try {
      out.emplace(*type, convert(entry, *type));
    }
    catch (const std::exception& e) {
      fatal(compat::format("{} entry '{}': {}", file_name, key, e.what()));
    }
  }
  if (out.size() != magic_enum::enum_count<E>()) {
    fatal(compat::format("{}: expected {} entries, found {}", file_name,
                         magic_enum::enum_count<E>(), out.size()));
  }
  return out;
}

template <typename E> E enum_field(const json& entry, const char* key) {
  const auto& value = entry.at(key).get_ref<const std::string&>();
  if (auto parsed = magic_enum::enum_cast<E>(value)) {
    return *parsed;
  }
  throw std::runtime_error(
      compat::format("field '{}': unknown value '{}'", key, value));
}

template <typename E>
E enum_field(const json& entry, const char* key, E fallback) {
  if (!entry.contains(key)) {
    return fallback;
  }
  return enum_field<E>(entry, key);
}

template <typename T, std::size_t N>
std::array<T, N> array_field(const json& entry, const char* key) {
  const json& list = entry.at(key);
  if (!list.is_array() || list.size() != N) {
    throw std::runtime_error(
        compat::format("field '{}': expected {} values", key, N));
  }
  std::array<T, N> out{};
  for (std::size_t i = 0; i < N; ++i) {
    out[i] = list[i].get<T>();
  }
  return out;
}

} // namespace data_file
