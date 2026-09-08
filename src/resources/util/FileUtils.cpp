/*
 * SPDX-FileCopyrightText: 2019 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "resources/util/FileUtils.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <sys/stat.h>
#include <utility>
#include "engine/Fatal.h"
#include "engine/Logger.h"
#include "engine/PlatformPaths.h"
#if PLEGACY_EMULATE_CASE_INSENSITIVE_FS
#include <string.h>
#endif
#if defined(_WIN32)
#define strcasecmp _stricmp
#endif

namespace {
std::string game_data_path_;
}
namespace {
bool iequals(std::string_view a, std::string_view b) {
  return std::ranges::equal(a, b, [](char x, char y) {
    return std::tolower(static_cast<unsigned char>(x)) ==
           std::tolower(static_cast<unsigned char>(y));
  });
}
} // anonymous namespace

namespace fileutils {

std::optional<std::filesystem::path>
find_file_path(const std::string& file_path) {
  namespace fs = std::filesystem;
  fs::path input_path = file_path;
  fs::path resolved;

  if (!input_path.is_absolute()) {
    if (input_path.string().starts_with("saves/")) {
      const auto& saves_dir = platform_paths::saves_dir();
      auto relative = input_path.string().substr(6); // skip "saves/"
      resolved = fs::path(saves_dir) / relative;
    }
    else {
      resolved = fs::path(game_data_path_) / input_path;
    }
  }
  else {
    resolved = input_path;
  }

  // Try with the full path (with extension) first
  if (fs::exists(resolved)) {
    return resolved;
  }

  // If not found, try without extension
  std::string stem = resolved.stem().string();
  std::string parent = resolved.parent_path().string();
  fs::path without_extension;
  if (!parent.empty() && parent != ".") {
    without_extension = fs::path(parent) / stem;
  }
  else {
    without_extension = stem;
  }

  if (fs::exists(without_extension)) {
    return without_extension;
  }

#if PLEGACY_EMULATE_CASE_INSENSITIVE_FS
  // Absolute paths are searched from their filesystem root; relative paths
  // are searched from the game data directory.
  const auto [walk_base, relative] =
      input_path.is_absolute()
          ? std::pair{input_path.root_path(), input_path.relative_path()}
          : std::pair{fs::path(game_data_path_), input_path};
  auto fallback = case_insensitive_lookup(walk_base, relative);
  if (fallback && fs::exists(*fallback)) {
    return *fallback;
  }
#endif

  return std::nullopt;
}

std::ifstream open(const std::string& raw_file_path, std::ios::openmode mode) {
  auto file_path = find_file_path(raw_file_path);
  // std::cout << "File::Open " << file_path << std::endl;

  if (!file_path) {
    fatal("File not found: " + raw_file_path);
  }

  std::ifstream is(*file_path, mode);
  if (!is) {
    fatal("Failed to open: " + raw_file_path);
  }

  return is;
}

std::vector<uint8_t> read_bytes(const std::string& file_path) {
  std::ifstream is = open(file_path);
  is.seekg(0, std::ios::end);
  auto size = is.tellg();
  is.seekg(0, std::ios::beg);
  std::vector<uint8_t> bytes(size > 0 ? static_cast<std::size_t>(size) : 0);
  if (!bytes.empty()) {
    is.read(reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
  }
  return bytes;
}

bool write_bytes(const std::string& path, std::span<const uint8_t> bytes) {
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    return false;
  }
  out.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
  return out.good();
}

std::vector<std::string> load_text_lines(const std::string& path,
                                         bool skip_empty) {
  auto file = open(path);
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(file, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (skip_empty && line.empty()) {
      continue;
    }
    lines.push_back(std::move(line));
  }
  return lines;
}

bool fexists(const std::string& raw_file_path) {
  auto file_path = find_file_path(raw_file_path);
  if (!file_path) {
    return false;
  }

  // Check if either lowercase or uppercase variant exists
  return std::filesystem::exists(*file_path);
}

std::optional<std::filesystem::path>
case_insensitive_lookup(const std::filesystem::path& base,
                        const std::filesystem::path& relative) {
  namespace fs = std::filesystem;
  fs::path current = base;

  for (const auto& part : relative) {
    if (!fs::exists(current) || !fs::is_directory(current)) {
      return std::nullopt;
    }

    bool matched = false;
    for (const auto& entry : fs::directory_iterator(current)) {
      if (iequals(entry.path().filename().string(), part.string())) {
        current /= entry.path().filename();
        matched = true;
        break;
      }
    }

    if (!matched) {
      return std::nullopt;
    }
  }

  return current;
}

std::vector<FileEntry> list_files(const std::string& directory,
                                  const std::string& extension,
                                  FileSortOrder sort) {
  namespace fs = std::filesystem;

  fs::path search_path;

  if (directory == "saves" || directory.starts_with("saves/")) {
    search_path = platform_paths::saves_dir();
  }
  else {
    search_path = fs::path(game_data_path_) / directory;
  }

  if (!fs::exists(search_path) || !fs::is_directory(search_path)) {
    return {};
  }

  std::vector<fs::directory_entry> entries;
  for (const auto& entry : fs::directory_iterator(search_path)) {
    if (entry.is_regular_file() && entry.path().extension() == extension) {
      entries.push_back(entry);
    }
  }

  if (sort == FileSortOrder::Alphabetical) {
    std::sort(entries.begin(), entries.end(),
              [](const fs::directory_entry& a, const fs::directory_entry& b) {
                return a.path().filename() < b.path().filename();
              });
  }
  else {
    std::sort(entries.begin(), entries.end(),
              [](const fs::directory_entry& a, const fs::directory_entry& b) {
                return a.last_write_time() > b.last_write_time();
              });
  }

  std::vector<FileEntry> files;
  files.reserve(entries.size());
  for (const auto& entry : entries) {
    files.push_back(
        {entry.path().filename().string(), entry.last_write_time()});
  }
  return files;
}

void game_data_path(const std::string& file_path) {
  game_data_path_ = file_path;
}

const std::string& game_data_path() { return game_data_path_; }

bool fexist(const char* filename) {
  struct stat buffer;
  Logger::debug("Filename is {}", filename);
  if (stat(filename, &buffer) == 0) {
    Logger::debug("");
    return true;
  }
  Logger::debug(", but it does not exist... ");
  return false;
}

uint32_t sum_bytes(std::span<const std::byte> data) {
  uint32_t s = 0;
  for (auto b : data) {
    s += std::to_integer<uint8_t>(b);
  }
  return s; // modulo 2^32 because we're using an uint32_t here (as does the
            // original)
}

} // namespace fileutils
