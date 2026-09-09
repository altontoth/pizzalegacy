/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/factories/DataFile.h"

#include <exception>
#include <fstream>
#include <string>
#include <string_view>

#include "engine/Fatal.h"
#include "engine/FormatCompat.h"
#include "engine/PlatformPaths.h"

namespace data_file {

json parse_file(std::string_view file_name) {
  const std::string path = platform_paths::data_dir() + std::string(file_name);
  std::ifstream in(path);
  if (!in) {
    fatal("Missing data file: " + path);
  }
  try {
    json parsed = json::parse(in);
    if (!parsed.is_object()) {
      fatal(path + ": expected an object");
    }
    return parsed;
  }
  catch (const std::exception& e) {
    fatal(compat::format("{}: {}", path, e.what()));
  }
}

} // namespace data_file
