/*
 * SPDX-FileCopyrightText: 2019 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace fileutils {
std::ifstream open(const std::string& file_path,
                   std::ios::openmode mode = std::ios::binary);
std::vector<std::string> list_files(const std::string& directory,
                                    const std::string& extension);
bool fexists(const std::string& file_path);

std::optional<std::filesystem::path>
find_file_path(const std::string& file_path);
std::optional<std::filesystem::path>
case_insensitive_lookup(const std::filesystem::path& base,
                        const std::filesystem::path& relative);
void game_data_path(const std::string&);
bool fexist(const char* filename);

} // namespace fileutils
