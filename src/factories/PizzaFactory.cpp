/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "factories/PizzaFactory.h"

#include <cstring>
#include <string>

#include "core/food/CanonicalPizza.h"
#include "core/food/Topping.h"
#include "core/util/LegacyConstants.h"
#include "engine/Endian.h"
#include "engine/FormatCompat.h"
#include "engine/Logger.h"
#include "engine/geom/Spaces.h"
#include "resources/util/ByteReader.h"
#include "resources/util/FileUtils.h"

std::unique_ptr<core::food::Pizza> PizzaFactory::create_pizza(
    const std::string& filename, const BobSizeFn& get_size,
    const core::food::CustomerTasteProfiles& taste_profiles) {
  // Extract recipe number from filename (e.g., "r/r13.pc" -> 13, "game/r3.pc"
  // -> 3)
  int recipe_id = -1;
  size_t r_pos = filename.rfind('r');
  if (r_pos != std::string::npos && r_pos + 1 < filename.size()) {
    size_t dot_pos = filename.find('.', r_pos);
    if (dot_pos != std::string::npos) {
      std::string num_str = filename.substr(r_pos + 1, dot_pos - r_pos - 1);
      try {
        recipe_id = std::stoi(num_str);
      }
      catch (...) {
        Logger::warn("Could not extract recipe number from filename: {}",
                     filename);
      }
    }
  }

  return create_pizza(fileutils::read_bytes(filename),
                      core::food::canonical_from_index(recipe_id), get_size,
                      taste_profiles);
}

std::unique_ptr<core::food::Pizza> PizzaFactory::create_pizza(
    std::span<const uint8_t> bytes,
    std::optional<core::food::CanonicalPizza> canonical,
    const BobSizeFn& get_size,
    const core::food::CustomerTasteProfiles& taste_profiles) {
  fileutils::ByteReader r{bytes};

  // Read the first 4 bytes: 'PREZ' for user-saved recipes, or empty for
  // canonical
  char header[4];
  r.read(header, 4);
  int offset = 302;
  int name_offset = 36;
  int stats_offset = 84; // Stats offset for PREZ files
  bool is_prez = (strncmp(header, "PREZ", 4) == 0);

  if (!is_prez) {
    offset = 294;
    name_offset = 28;
    stats_offset = 76; // Stats offset for non-PREZ files (28 + 48)
  }

  // Skip to offset 0x20 (32 bytes) to read the recipe name
  r.seek(name_offset);

  std::string pizza_name_str;
  if (!is_prez && canonical) {
    // Non-PREZ canonical files: use the canonical name
    pizza_name_str = to_string(*canonical);
  }
  else {
    // PREZ (user-saved) or non-canonical: read name from file
    constexpr std::size_t max_name_bytes = 64;

    std::size_t read_count = 0;
    while (read_count < max_name_bytes && r.has_more()) {
      char ch = static_cast<char>(r.get());
      ++read_count;
      if (ch == '\0') {
        break; // stop at terminator (cursor now at next field)
      }
      pizza_name_str.push_back(ch);
    }

    // Trim trailing spaces (0x20) that are used as padding in some records.
    while (!pizza_name_str.empty() && pizza_name_str.back() == ' ') {
      pizza_name_str.pop_back();
    }
  }

  // Read stats from disk at the stats offset
  r.seek(stats_offset);

  uint16_t fat_raw;
  uint16_t sweet_raw;
  uint16_t salty_raw;
  uint16_t hot_raw;

  uint8_t stat_bytes[8];
  r.read(stat_bytes, 8);
  fat_raw = endian::u16le(stat_bytes[0], stat_bytes[1]);
  sweet_raw = endian::u16le(stat_bytes[2], stat_bytes[3]);
  salty_raw = endian::u16le(stat_bytes[4], stat_bytes[5]);
  hot_raw = endian::u16le(stat_bytes[6], stat_bytes[7]);

  // Read 3 uint8_t values
  uint8_t appearance_raw = r.get();
  uint8_t refinement_raw = r.get();
  uint8_t normality_raw = r.get();

  // Seek to the ingredient count at offset (294 for non-PREZ)
  r.seek(offset);
  r.get(); // ingredient type count
  uint8_t ingredient_count = r.get();

  std::unique_ptr<core::food::Pizza> pizza =
      std::make_unique<core::food::Pizza>(pizza_name_str);

  constexpr int x_offset = -LegacyConstants::WOOD_BORDER_WIDTH_LEFT;
  constexpr int y_offset = -LegacyConstants::WOOD_BORDER_HEIGHT_TOP;

  // Read the position indices for each ingredient
  for (uint8_t i = 0; i < ingredient_count; ++i) {
    uint8_t ingredient_id = r.get();
    uint8_t state = r.get();
    uint8_t x_pos = r.get();
    uint8_t y_pos = r.get();

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
    termination_byte = r.get();
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
        "Unexpected termination bytes in recipe: {:02x} {:02x} {:02x} {:02x}",
        termination_bytes[0], termination_bytes[1], termination_bytes[2],
        termination_bytes[3]);
  }

  // Mark canonical pizzas with the correct origin
  if (canonical) {
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
    core::food::CanonicalPizza canonical, const BobSizeFn& get_size,
    const core::food::CustomerTasteProfiles& taste_profiles) {
  return create_pizza(
      fileutils::read_bytes(core::food::filename_for(canonical)), canonical,
      get_size, taste_profiles);
}

std::unique_ptr<core::food::Pizza> PizzaFactory::create_competition_pizza(
    int recipe_number, const BobSizeFn& get_size,
    const core::food::CustomerTasteProfiles& taste_profiles) {
  const auto filename = compat::format("r/r{}.pc", recipe_number);
  return create_pizza(fileutils::read_bytes(filename),
                      core::food::canonical_from_index(recipe_number), get_size,
                      taste_profiles);
}
