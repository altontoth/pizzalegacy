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
#include <sys/stat.h>
#include "core/save/SaveGameManager.h"
#include "engine/Fatal.h"
#include "engine/Logger.h"
#include "engine/Random.h"
#include "resources/ImploderDecompressor.h"
#if PLEGACY_CASE_INSENSITIVE_FS
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
    if (input_path.string().starts_with("assets/")) {
      fs::path candidate1 = fs::path("..") / input_path;
      fs::path candidate2 = fs::path(".") / input_path;
      if (fs::exists(candidate1)) {
        return candidate1;
      }
      if (fs::exists(candidate2)) {
        return candidate2;
      }
    }
    else if (input_path.string().starts_with("saves/")) {
      const auto& saves_dir = save::SaveGameManager::get_saves_directory();
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

#if PLEGACY_CASE_INSENSITIVE_FS
  auto fallback = case_insensitive_lookup(
      fs::path(game_data_path_), input_path.is_absolute()
                                     ? input_path.lexically_relative("/")
                                     : input_path);
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

  // Open file in binary mode to check for IMP! magic
  std::ifstream is(*file_path, std::ios::binary);
  if (!is.is_open()) {
    fatal("Failed to open: " + raw_file_path);
  }

  // Check for IMP! compressed magic bytes
  uint8_t magic[4] = {0};
  is.read(reinterpret_cast<char*>(magic), 4);
  is.seekg(0);

  if (ImploderDecompressor::is_imploder_magic(
          std::vector<uint8_t>(magic, magic + 4))) {
    // File is IMP! compressed - decompress it
    try {
      Logger::debug("Decompressing IMP! file: {}", file_path->string());

      // Read entire compressed file
      is.seekg(0, std::ios::end);
      std::streamsize file_size = is.tellg();
      is.seekg(0, std::ios::beg);

      std::vector<uint8_t> compressed(file_size);
      is.read(reinterpret_cast<char*>(compressed.data()), file_size);
      is.close();

      // Decompress
      auto decompressed = ImploderDecompressor::decompress(compressed);

      // Create temporary file in system temp directory
      namespace fs = std::filesystem;
      fs::path temp_dir = fs::temp_directory_path();
      std::string temp_filename =
          "plegacy_" + std::to_string(std::hash<std::string>{}(raw_file_path)) +
          ".tmp";
      fs::path temp_path = temp_dir / temp_filename;

      // Write decompressed data to temp file
      std::ofstream temp_file(temp_path, std::ios::binary);
      if (!temp_file) {
        fatal("Failed to create temporary decompressed file: " +
              temp_path.string());
      }
      temp_file.write(reinterpret_cast<char*>(decompressed.data()),
                      decompressed.size());
      temp_file.close();

      Logger::debug("Wrote decompressed data to: {}", temp_path.string());

      // Open and return the temporary file
      std::ifstream temp_read(temp_path, mode);
      if (!temp_read.is_open()) {
        fatal("Failed to open decompressed temporary file: " +
              temp_path.string());
      }

      return temp_read;
    }
    catch (const std::exception& e) {
      fatal(std::string("Failed to decompress file: ") + raw_file_path + " - " +
            e.what());
    }
  }

  // Not compressed - return file as-is with requested mode
  // Close and reopen with actual requested mode
  is.close();
  std::ifstream final_is(*file_path, mode);
  if (!final_is.is_open()) {
    fatal("Failed to open: " + raw_file_path);
  }

  return final_is;
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
    search_path = save::SaveGameManager::get_saves_directory();
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

// int rand_max_n(int max) { return max > 0 ? std::rand() % max : 0; }
int rand_max_n(int max) {
  int r = engine::random::ansi_rand15(); // 0..32767
                                         // same shift logic:
  long long product = static_cast<long long>(r) * max;
  // product is up to 32767 * param
  // shr 15 is floor division by 32768:
  return static_cast<int>(product >> 15);
}

int rand_between(int min, int max) {
  auto rand = rand_max_n(max - min);
  return rand + min;
}

int ansi_rand15() { return engine::random::ansi_rand15(); }

bool rand_percentage_chance(int percentage) {
  if (percentage <= 0) {
    return 0;
  }
  if (percentage >= 100) {
    return 1;
  }
  // param is between 1..99
  int r = ansi_rand15();          // 0..32767
  int r_scaled = (r * 100) >> 15; // uniform in [0..99]
  if (r_scaled < percentage) {
    return 1;
  }
  return 0;
}

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

bool coin_flip() { return rand_percentage_chance(50); }

uint32_t sum_bytes(std::span<const std::byte> data) {
  uint32_t s = 0;
  for (auto b : data) {
    s += std::to_integer<uint8_t>(b);
  }
  return s; // modulo 2^32 because we're using an uint32_t here (as does the
            // original)
}

} // namespace fileutils
