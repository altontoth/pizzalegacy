/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "factories/PizzaFactory.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

#include "core/food/CanonicalPizza.h"
#include "core/food/Topping.h"
#include "core/util/LegacyConstants.h"
#include "engine/Logger.h"
#include "engine/geom/Spaces.h"
#include "resources/util/FileUtils.h"
#include "resources/util/Platform.h"

std::unique_ptr<core::food::Pizza> PizzaFactory::create_pizza(
    const std::string& filename, const BobSizeFn& get_size,
    const core::food::CustomerTasteProfiles& taste_profiles) {
  std::string path(filename);

  // Extract recipe number from filename (e.g., "r/r14.pc" -> 14, "game/r3.pc"
  // -> 3)
  int recipe_id = -1;
  size_t r_pos = path.rfind('r');
  if (r_pos != std::string::npos && r_pos + 1 < path.size()) {
    size_t dot_pos = path.find('.', r_pos);
    if (dot_pos != std::string::npos) {
      std::string num_str = path.substr(r_pos + 1, dot_pos - r_pos - 1);
      try {
        recipe_id = std::stoi(num_str);
      }
      catch (...) {
        Logger::warn("Could not extract recipe number from filename: {}",
                     filename);
      }
    }
  }

  std::ifstream is = fileutils::open(path);
  if (!is) {
    std::cerr << "Failed to open file: " << path << "\n";
    return nullptr;
  }

  // Read the first 4 bytes: 'PREZ' for user-saved recipes, or empty for
  // canonical
  char header[4];
  is.read(header, 4);
  int offset = 302;
  int name_offset = 36;
  int stats_offset = 84; // Stats offset for PREZ files
  bool is_prez = (strncmp(header, "PREZ", 4) == 0);

  if (!is_prez) {
    offset = 294;
    name_offset = 28;
    stats_offset = 76; // Stats offset for non-PREZ files (28 + 48)
  }

  // Skip 4 unknown bytes after PREZ/non-PREZ header
  is.get();
  is.get();
  is.get();
  is.get();

  // Skip to offset 0x20 (32 bytes) to read the recipe name
  is.seekg(name_offset, std::ios::beg);

  std::string pizza_name_str;
  if (!is_prez && recipe_id >= 0 &&
      recipe_id <=
          static_cast<int>(core::food::CanonicalPizza::VegetablePizza)) {
    // Non-PREZ canonical files: use the canonical name
    pizza_name_str =
        to_string(static_cast<core::food::CanonicalPizza>(recipe_id));
  }
  else {
    // PREZ (user-saved) or non-canonical: read name from file
    constexpr std::size_t max_name_bytes = 64;

    char ch = '\0';
    std::size_t read_count = 0;
    while (read_count < max_name_bytes && is.get(ch)) {
      ++read_count;
      if (ch == '\0') {
        break; // stop at terminator (stream now at next field)
      }
      pizza_name_str.push_back(ch);
    }

    // Trim trailing spaces (0x20) that are used as padding in some records.
    while (!pizza_name_str.empty() && pizza_name_str.back() == ' ') {
      pizza_name_str.pop_back();
    }
  }

  // std::cout << "Processing pizza recipe: " << pizza_name_str << "\n";

  // Read stats from disk at the stats offset
  is.seekg(stats_offset, std::ios::beg);

  uint16_t fat_raw;
  uint16_t sweet_raw;
  uint16_t salty_raw;
  uint16_t hot_raw;

  auto endianness = platform::endianness();

  uint8_t bytes[8];
  is.read(reinterpret_cast<char*>(bytes), 8);
  fat_raw = math::two_bytes_to_uint16(bytes[0], bytes[1], endianness);
  sweet_raw = math::two_bytes_to_uint16(bytes[2], bytes[3], endianness);
  salty_raw = math::two_bytes_to_uint16(bytes[4], bytes[5], endianness);
  hot_raw = math::two_bytes_to_uint16(bytes[6], bytes[7], endianness);

  // Read 3 uint8_t values
  uint8_t appearance_raw = is.get();
  uint8_t refinement_raw = is.get();
  uint8_t normality_raw = is.get();

  // Logger::debug("  Stats at offset {}: fat={}, sweet={}, salty={}, hot={}, "
  //               "appearance={}, refinement={}, normality={}",
  //               stats_offset, fat_raw, sweet_raw, salty_raw, hot_raw,
  //               appearance_raw, refinement_raw, normality_raw);

  // Seek to the ingredient count at offset (294 for non-PREZ)
  is.seekg(offset, std::ios::beg);
  uint8_t ingredient_type_count_maybe = is.get();
  uint8_t ingredient_count = is.get();
  // Logger::debug("  Ingredient data at offset {}: type_count={},
  // ingredient_count={}",
  //               offset, ingredient_type_count_maybe, ingredient_count);

  std::unique_ptr<core::food::Pizza> pizza =
      std::make_unique<core::food::Pizza>(pizza_name_str);
  // std::cout << "Ingredient count: " << static_cast<int>(ingredient_count)
  //           << "\n";

  constexpr int x_offset = -LegacyConstants::WOOD_BORDER_WIDTH_LEFT;
  constexpr int y_offset = -LegacyConstants::WOOD_BORDER_HEIGHT_TOP;

  // Read the position indices for each ingredient
  for (uint8_t i = 0; i < ingredient_count; ++i) {
    uint8_t ingredient_id = is.get();
    uint8_t state = is.get();
    uint8_t x_pos = is.get();
    uint8_t y_pos = is.get();

    // Logger::debug("  Ingredient {}: id={}, state={}, pos=({}, {})",
    //               i, ingredient_id, state, x_pos, y_pos);

    // File stores top-left in 320x200 and with wooden borders:
    const int tl_x = (x_pos * 2) + x_offset;
    const int tl_y = (y_pos * 2) + y_offset;

    // We need the size to get to the center point:
    auto size = get_size(ingredient_id, state);
    const WorldPoint center{tl_x + (size.w / 2), tl_y + (size.h / 2)};

    std::unique_ptr<core::food::Topping> t =
        std::make_unique<core::food::Topping>(ingredient_id, state, center);
    pizza->add_topping(std::move(t));
  }

  uint8_t termination_bytes[4];
  for (unsigned char& termination_byte : termination_bytes) {
    termination_byte = is.get();
  }

  bool is_terminated = true;
  for (unsigned char termination_byte : termination_bytes) {
    if (termination_byte != 0) {
      is_terminated = false;
      break;
    }
  }

  if (!is_terminated) {
    Logger::warn(
        "Unexpected termination bytes in {}: {:02x} {:02x} {:02x} {:02x}", path,
        termination_bytes[0], termination_bytes[1], termination_bytes[2],
        termination_bytes[3]);
  }

  // Close the file stream
  is.close();

  // Mark canonical pizzas with the correct origin
  if (recipe_id >= 0 &&
      recipe_id <=
          static_cast<int>(core::food::CanonicalPizza::VegetablePizza)) {
    pizza->set_origin(core::food::Origin::Canonical);
  }

  // Set stats from disk values
  // Disk values are in range 0-10, need to scale to 0-100 for internal
  // representation
  core::food::Flavors disk_flavors;
  disk_flavors.fat = static_cast<int>(fat_raw) * 10;
  disk_flavors.sweet = static_cast<int>(sweet_raw) * 10;
  disk_flavors.salty = static_cast<int>(salty_raw) * 10;
  disk_flavors.hot = static_cast<int>(hot_raw) * 10;

  pizza->finalize(taste_profiles, pizza_name_str);
  pizza->set_disk_stats(disk_flavors, appearance_raw, refinement_raw,
                        normality_raw, taste_profiles);

  return pizza;
}

std::unique_ptr<core::food::Pizza> PizzaFactory::create_pizza(
    int recipe_id, const BobSizeFn& get_size,
    const core::food::CustomerTasteProfiles& taste_profiles) {
  char filename[16];
  snprintf(filename, sizeof(filename), "r/r%d.pc", recipe_id);
  return create_pizza(std::string(filename), get_size, taste_profiles);
}
