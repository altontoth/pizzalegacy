/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/factories/PizzaFactory.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <string>

#include "core/food/IngredientType.h"
#include "core/food/PlayerRecipes.h"
#include "core/food/Recipe.h"
#include "core/food/RecipeNode.h"
#include "core/food/StandardPizza.h"
#include "core/food/Topping.h"
#include "core/util/LegacyConstants.h"
#include "engine/Endian.h"
#include "engine/FormatCompat.h"
#include "engine/Logger.h"
#include "engine/geom/Spaces.h"
#include "resources/util/ByteReader.h"
#include "resources/util/FileUtils.h"

namespace {

// Place one ingredient: the file stores its top-left in 320x200 space with the
// wooden border; convert to a wall-relative center point.
std::unique_ptr<core::food::Topping> make_topping(uint8_t ingredient_id,
                                                  uint8_t state, uint8_t x_pos,
                                                  uint8_t y_pos,
                                                  const BobSizeFn& get_size) {
  constexpr int x_offset = -LegacyConstants::WOOD_BORDER_WIDTH_LEFT;
  constexpr int y_offset = -LegacyConstants::WOOD_BORDER_HEIGHT_TOP;
  const int tl_x = (x_pos * 2) + x_offset;
  const int tl_y = (y_pos * 2) + y_offset;
  auto size = get_size(ingredient_id, state);
  const WorldPoint center{tl_x + (size.w / 2), tl_y + (size.h / 2)};
  return std::make_unique<core::food::Topping>(ingredient_id, state, center);
}

// Field offsets in a .pc file. In a PREZ file all these are shifted by
// prez_shift bytes.
constexpr std::size_t pc_name = 28;
constexpr std::size_t pc_stats = 76;
constexpr std::size_t pc_counts = 294;
constexpr std::size_t prez_shift = 8;

namespace layout = core::food::recipe_node;

struct PcFile {
  std::unique_ptr<core::food::Pizza> pizza;
  std::string name;
  core::food::Flavors disk_flavors;
  uint8_t appearance{0};
  uint8_t refinement{0};
  uint8_t normality{0};
};

PcFile read_pc_file(std::span<const uint8_t> bytes,
                    std::optional<core::food::StandardPizza> reference,
                    const BobSizeFn& get_size) {
  fileutils::ByteReader r{bytes};

  // Read the first 4 bytes: 'PREZ' for user-saved recipes, or empty for a
  // reference
  char header[4];
  r.read(header, 4);
  bool is_prez = (strncmp(header, "PREZ", 4) == 0);
  const std::size_t shift = is_prez ? prez_shift : 0;
  const std::size_t offset = pc_counts + shift;
  const std::size_t name_offset = pc_name + shift;
  const std::size_t stats_offset = pc_stats + shift;

  // Skip to offset 0x20 (32 bytes) to read the recipe name
  r.seek(name_offset);

  std::string pizza_name_str;
  if (!is_prez && reference) {
    // Non-PREZ reference files: use the reference name
    pizza_name_str = to_string(*reference);
  }
  else {
    // PREZ (user-saved) or not a reference: read name from file
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

  // Read the position indices for each ingredient
  for (uint8_t i = 0; i < ingredient_count; ++i) {
    uint8_t ingredient_id = r.get();
    uint8_t state = r.get();
    uint8_t x_pos = r.get();
    uint8_t y_pos = r.get();
    pizza->add_topping(
        make_topping(ingredient_id, state, x_pos, y_pos, get_size));
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

  // Mark standard pizzas with the correct origin
  if (reference) {
    pizza->set_origin(core::food::Origin::Standard);
  }

  // Set stats from disk values
  // Disk values are in range 0-10, need to scale to 0-100 for internal
  // representation
  core::food::Flavors disk_flavors;
  disk_flavors.fat = static_cast<int>(fat_raw) * 10;
  disk_flavors.sweet = static_cast<int>(sweet_raw) * 10;
  disk_flavors.salty = static_cast<int>(salty_raw) * 10;
  disk_flavors.hot = static_cast<int>(hot_raw) * 10;

  return PcFile{.pizza = std::move(pizza),
                .name = pizza_name_str,
                .disk_flavors = disk_flavors,
                .appearance = appearance_raw,
                .refinement = refinement_raw,
                .normality = normality_raw};
}

} // namespace

std::unique_ptr<core::food::Pizza> PizzaFactory::create_pizza(
    const std::string& filename, const BobSizeFn& get_size,
    const core::food::CustomerTasteProfiles& taste_profiles) {
  // Extract recipe number from filename (e.g., "r/r13.pc" -> 13, "game/r3.pc"
  // -> 3)
  int recipe_id = -1;
  const std::string stem = std::filesystem::path(filename).stem().string();
  if (stem.size() > 1 && stem[0] == 'r' &&
      std::all_of(stem.begin() + 1, stem.end(),
                  [](unsigned char c) { return std::isdigit(c) != 0; })) {
    recipe_id = std::stoi(stem.substr(1));
  }

  return create_pizza(fileutils::read_bytes(filename),
                      core::food::standard_from_index(recipe_id), get_size,
                      taste_profiles);
}

std::optional<PizzaFactory::RecipeSummary>
PizzaFactory::read_summary(const std::string& filename) {
  return read_summary(fileutils::read_bytes(filename));
}

std::optional<PizzaFactory::RecipeSummary>
PizzaFactory::read_summary(std::span<const uint8_t> bytes) {
  if (bytes.size() < 4) {
    return std::nullopt;
  }
  const bool is_prez = std::memcmp(bytes.data(), "PREZ", 4) == 0;
  const std::size_t shift = is_prez ? prez_shift : 0;
  const std::size_t name_offset = pc_name + shift;
  const std::size_t count_offset = pc_counts + shift;
  if (bytes.size() < count_offset + 2) {
    return std::nullopt;
  }

  RecipeSummary summary;
  constexpr std::size_t max_name_bytes = 64;
  for (std::size_t i = name_offset;
       i < name_offset + max_name_bytes && i < bytes.size() && bytes[i] != 0;
       ++i) {
    summary.name.push_back(static_cast<char>(bytes[i]));
  }
  while (!summary.name.empty() && summary.name.back() == ' ') {
    summary.name.pop_back();
  }
  if (summary.name.empty()) {
    return std::nullopt;
  }

  const int count = bytes[count_offset + 1];
  const std::size_t items = count_offset + 2;
  if (bytes.size() < items + (4 * static_cast<std::size_t>(count))) {
    return std::nullopt;
  }
  for (int i = 0; i < count; ++i) {
    const int id = bytes[items + (4 * static_cast<std::size_t>(i))];
    const std::string name{
        core::food::to_string(static_cast<core::food::IngredientType>(id))};
    if (!name.empty() && std::ranges::find(summary.ingredients, name) ==
                             summary.ingredients.end()) {
      summary.ingredients.push_back(name);
    }
  }
  return summary;
}

PizzaFactory::ImportResult PizzaFactory::import_recipe(
    const std::string& filename, core::food::PlayerRecipes& recipes,
    const BobSizeFn& get_size,
    const core::food::CustomerTasteProfiles& taste_profiles) {
  const auto bytes = fileutils::read_bytes(filename);
  if (!read_summary(bytes)) {
    return ImportResult::Unreadable;
  }
  auto pizza = create_pizza(bytes, std::nullopt, get_size, taste_profiles);
  if (!pizza) {
    return ImportResult::Unreadable;
  }
  std::string name = pizza->name();
  const bool listed =
      std::ranges::any_of(recipes.all(), [&](const core::food::Recipe* r) {
        return r->display_name() == name;
      });
  if (listed) {
    return ImportResult::AlreadyListed;
  }
  const auto id = recipes.create_user(name, std::move(*pizza));
  Logger::info("Imported recipe {} from {} into user slot {}", name, filename,
               id.value);
  return ImportResult::Imported;
}

std::unique_ptr<core::food::Pizza> PizzaFactory::create_pizza(
    std::span<const uint8_t> bytes,
    std::optional<core::food::StandardPizza> reference,
    const BobSizeFn& get_size,
    const core::food::CustomerTasteProfiles& taste_profiles) {
  auto pc = read_pc_file(bytes, reference, get_size);
  pc.pizza->finalize(taste_profiles, pc.name);
  pc.pizza->set_disk_stats(pc.disk_flavors, pc.appearance, pc.refinement,
                           pc.normality, core::food::Pizza::full_imitation,
                           taste_profiles);
  return std::move(pc.pizza);
}

std::unique_ptr<core::food::Pizza>
PizzaFactory::read_pizza(std::span<const uint8_t> bytes,
                         std::optional<core::food::StandardPizza> reference,
                         const BobSizeFn& get_size) {
  return read_pc_file(bytes, reference, get_size).pizza;
}

std::unique_ptr<core::food::Pizza>
PizzaFactory::read_pizza(core::food::StandardPizza reference,
                         const BobSizeFn& get_size) {
  return read_pizza(fileutils::read_bytes(core::food::filename_for(reference)),
                    reference, get_size);
}

std::unique_ptr<core::food::Pizza> PizzaFactory::create_pizza(
    core::food::StandardPizza reference, const BobSizeFn& get_size,
    const core::food::CustomerTasteProfiles& taste_profiles) {
  return create_pizza(
      fileutils::read_bytes(core::food::filename_for(reference)), reference,
      get_size, taste_profiles);
}

std::unique_ptr<core::food::Pizza> PizzaFactory::create_competition_pizza(
    int recipe_number, const BobSizeFn& get_size,
    const core::food::CustomerTasteProfiles& taste_profiles) {
  const auto filename = compat::format("r/r{}.pc", recipe_number);
  return create_pizza(fileutils::read_bytes(filename),
                      core::food::standard_from_index(recipe_number), get_size,
                      taste_profiles);
}

int PizzaFactory::gam_node_recipe_uid(std::span<const uint8_t> node) {
  return node.size() > layout::uid ? node[layout::uid]
                                   : core::food::STANDARD_PIZZA_COUNT;
}

core::food::Recipe::DailySales
PizzaFactory::gam_node_daily_sales(std::span<const uint8_t> node) {
  core::food::Recipe::DailySales sales{};
  fileutils::ByteReader r{node};
  r.seek(layout::daily_sales);
  for (auto& day : sales) {
    day = r.u16le();
  }
  return sales;
}

std::unique_ptr<core::food::Pizza> PizzaFactory::create_pizza_from_gam_node(
    std::span<const uint8_t> node, const BobSizeFn& get_size,
    const core::food::CustomerTasteProfiles& taste_profiles) {
  fileutils::ByteReader r{node};

  r.seek(layout::name);
  std::string name;
  constexpr std::size_t max_name_bytes = 32;
  for (std::size_t i = 0; i < max_name_bytes && r.has_more(); ++i) {
    char ch = static_cast<char>(r.get());
    if (ch == '\0') {
      break;
    }
    name.push_back(ch);
  }

  const int uid = gam_node_recipe_uid(node);

  r.seek(layout::imitation);
  const uint8_t imitation = r.get();

  r.seek(layout::appearance);
  const uint8_t appearance = r.get();
  const uint8_t refinement = r.get();

  r.seek(layout::placed_count);
  uint8_t placed_count = r.get();

  r.seek(layout::placements);
  auto pizza = std::make_unique<core::food::Pizza>(name);
  for (uint8_t i = 0; i < placed_count; ++i) {
    uint8_t ingredient_id = r.get();
    uint8_t state = r.get();
    uint8_t x_pos = r.get();
    uint8_t y_pos = r.get();
    pizza->add_topping(
        make_topping(ingredient_id, state, x_pos, y_pos, get_size));
  }

  if (uid < core::food::STANDARD_PIZZA_COUNT) {
    pizza->set_origin(core::food::Origin::Standard);
  }

  // Origin gates the appearance term in the demographic scores, so it must be
  // set before the stats recompute below.
  pizza->finalize(taste_profiles, name);
  pizza->set_saved_stats(appearance, refinement, imitation, taste_profiles);
  return pizza;
}
