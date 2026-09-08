/*
 * SPDX-FileCopyrightText: 2019 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace fileutils {

enum class FileSortOrder { Alphabetical, LastModified };

struct FileEntry {
  std::string filename;
  std::filesystem::file_time_type last_write_time;
};

std::ifstream open(const std::string& file_path,
                   std::ios::openmode mode = std::ios::binary);
std::vector<uint8_t> read_bytes(const std::string& file_path);
// Writes to the path as given. False when the file could not be written.
bool write_bytes(const std::string& path, std::span<const uint8_t> bytes);
std::vector<std::string> load_text_lines(const std::string& path,
                                         bool skip_empty = false);
std::vector<FileEntry>
list_files(const std::string& directory, const std::string& extension,
           FileSortOrder sort = FileSortOrder::Alphabetical);
bool fexists(const std::string& file_path);

std::optional<std::filesystem::path>
find_file_path(const std::string& file_path);
std::optional<std::filesystem::path>
case_insensitive_lookup(const std::filesystem::path& base,
                        const std::filesystem::path& relative);
void game_data_path(const std::string&);
const std::string& game_data_path();
bool fexist(const char* filename);

} // namespace fileutils
