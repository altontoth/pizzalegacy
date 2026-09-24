/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include "resources/BobSizeFn.h"
#include "resources/util/FileUtils.h"

namespace core::food {
class Pizza;
}

namespace io::recipe {
inline constexpr const char* recipe_extension = ".pc";

// The files in the player's recipes directory.
std::vector<fileutils::FileEntry> list_recipe_files();
std::filesystem::path recipe_path(const std::string& filename);
// A filename stem for the name that none of the files use yet: the name
// itself, or the name with a number after it.
std::string unused_recipe_stem(std::string_view name,
                               const std::vector<fileutils::FileEntry>& files);

// Writes the pizza as a PREZ recipe file with the given name.
bool save_prez(const core::food::Pizza& pizza,
               const std::filesystem::path& path, const BobSizeFn& get_bob_size,
               std::string_view name);
}; // namespace io::recipe
