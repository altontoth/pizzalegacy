/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/food/RecipeCodec.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "core/Demographics.h"
#include "core/food/Ingredient.h"
#include "core/food/Pizza.h"
#include "core/food/RecipeNode.h"
#include "core/food/Topping.h"
#include "core/util/LegacyConstants.h"
#include "engine/Logger.h"
#include "engine/Math.h"
#include "engine/PlatformPaths.h"
#include "resources/BobSizeFn.h"
#include "resources/util/FileUtils.h"
#include "resources/util/TextEncoding.h"

namespace io::recipe {

std::vector<fileutils::FileEntry> list_recipe_files() {
  return fileutils::list_files_in(platform_paths::recipes_dir(),
                                  recipe_extension);
}

std::filesystem::path recipe_path(const std::string& filename) {
  return std::filesystem::path(platform_paths::recipes_dir()) / filename;
}

std::string unused_recipe_stem(std::string_view name,
                               const std::vector<fileutils::FileEntry>& files) {
  std::string stem = fileutils::sanitize_filename(std::string(name));
  if (stem.empty()) {
    stem = "recipe";
  }
  auto taken = [&](const std::string& candidate) {
    const std::string filename = candidate + recipe_extension;
    return std::ranges::any_of(files, [&](const fileutils::FileEntry& f) {
      return f.filename == filename;
    });
  };
  if (!taken(stem)) {
    return stem;
  }
  for (int n = 2;; ++n) {
    const std::string candidate = stem + " " + std::to_string(n);
    if (!taken(candidate)) {
      return candidate;
    }
  }
}

namespace {

void put_u16(std::vector<uint8_t>& blob, std::size_t offset, int value) {
  const auto v = static_cast<uint16_t>(std::clamp(value, 0, 65535));
  blob[offset] = static_cast<uint8_t>(v & 255);
  blob[offset + 1] = static_cast<uint8_t>(v >> 8);
}

uint8_t to_byte(int value) {
  return static_cast<uint8_t>(std::clamp(value, 0, 255));
}

} // namespace

bool save_prez(const core::food::Pizza& pizza,
               const std::filesystem::path& path, const BobSizeFn& get_bob_size,
               std::string_view name) {
  namespace node = core::food::recipe_node;

  const auto& tops = pizza.toppings();
  const std::size_t placed_count = std::min<std::size_t>(tops.size(), 255);

  std::set<int> distinct_ids;
  std::array<int, node::grams_entries> grams{};
  std::array<int, node::grams_entries> intensity{};
  for (std::size_t i = 0; i < placed_count; ++i) {
    const auto& t = *tops[i];
    const int id = static_cast<int>(t.ingredient_type());
    distinct_ids.insert(id);
    if (id >= 0 && id < static_cast<int>(node::grams_entries)) {
      grams[id] += t.weight();
      intensity[id] = t.ingredient()->intensity();
    }
  }

  const std::size_t n = node::size_without_placements + (4 * placed_count);
  std::vector<uint8_t> blob(n, 0);

  // Pizza ID byte: must be >= 0x0E to allow saving as custom,
  // since the first 14 pizzas on the menu are always 'standard'.
  // Ideally we'd make this the id of the pizza on the recipe list,
  // but it doesn't matter anyway, so let's just hardcode something > 14.
  blob[node::uid] = 0x20;

  // Name (NUL-terminated)
  {
    std::string stored(name);
    text::truncate_utf8(stored, node::judged - node::name - 1);
    std::memcpy(&blob[node::name], stored.data(), stored.size());
  }

  blob[node::judged] = 1;
  blob[node::imitation] = to_byte(pizza.imitation());

  // The ingredients weighing most on the taste, by grams times intensity.
  std::vector<int> by_intensity;
  for (int id = 0; id < static_cast<int>(node::grams_entries); ++id) {
    if (grams[id] > 0) {
      by_intensity.push_back(id);
    }
  }
  std::ranges::stable_sort(by_intensity, std::greater{},
                           [&](int id) { return grams[id] * intensity[id]; });
  for (std::size_t i = 0; i < node::primary_ingredient_count; ++i) {
    blob[node::primary_ingredients + i] =
        i < by_intensity.size() ? static_cast<uint8_t>(by_intensity[i]) : 255;
  }

  const auto flavors = pizza.flavors();
  put_u16(blob, node::flavors, flavors.fat / 10);
  put_u16(blob, node::flavors + 2, flavors.sweet / 10);
  put_u16(blob, node::flavors + 4, flavors.salty / 10);
  put_u16(blob, node::flavors + 6, flavors.hot / 10);
  blob[node::appearance] = to_byte(pizza.appearance());
  blob[node::refinement] = to_byte(pizza.refinement());
  blob[node::normality] = to_byte(pizza.normality());
  blob[node::herbs] = to_byte(pizza.herb_score());
  const auto popularity = pizza.popularity();
  for (std::size_t i = 0; i < ALL_CONSUMER_TYPES.size(); ++i) {
    blob[node::popularity + i] = to_byte(popularity[ALL_CONSUMER_TYPES[i]]);
  }
  put_u16(blob, node::calories, pizza.calories());
  for (std::size_t id = 0; id < node::grams_entries; ++id) {
    put_u16(blob, node::grams + (2 * id), grams[id]);
  }

  blob[node::type_count] = to_byte(static_cast<int>(distinct_ids.size()));
  blob[node::placed_count] = static_cast<uint8_t>(placed_count);

  // Position mapping used by our loader:
  constexpr int x_offset = -LegacyConstants::WOOD_BORDER_WIDTH_LEFT;
  constexpr int y_offset = -LegacyConstants::WOOD_BORDER_HEIGHT_TOP;

  // Items: [id, state, x_pos, y_pos]
  for (std::size_t i = 0; i < placed_count; ++i) {
    const auto& t = *tops[i];
    const int id = static_cast<int>(t.ingredient_type());
    const int state = t.state_raw();

    // We stored CENTER in pizza-space.
    const WorldPoint c = t.origin();

    // Look up bob size for this ingredient/state
    const Size sz = get_bob_size(id, state);
    const int w = sz.w;
    const int h = sz.h;

    // Convert to legacy TOP-LEFT pixel coordinates
    const int tl_x = c.x - (w / 2);
    const int tl_y = c.y - (h / 2);

    // Pixel -> legacy grid (grid step = 2 px, with per-file offsets)
    auto pixel_to_grid = [](int pixel, int offset) -> uint8_t {
      const int v = pixel - offset; // pixels relative to file origin

      // ROUNDING POLICY:
      // nearest: (v + sign(v)) / 2
      const int grid = (v >= 0) ? ((v + 1) / 2) : ((v - 1) / 2);

      return static_cast<uint8_t>(std::clamp(grid, 0, 255));
    };

    const std::size_t base = node::placements + (4 * i);
    blob[base + 0] = static_cast<uint8_t>(id & 0xFF);
    blob[base + 1] = static_cast<uint8_t>(state & 0xFF);
    blob[base + 2] = pixel_to_grid(tl_x, x_offset);
    blob[base + 3] = pixel_to_grid(tl_y, y_offset);
  }

  const uint32_t checksum = math::sum_bytes(std::as_bytes(std::span{blob}));
  const auto length = static_cast<uint32_t>(n);

  std::ofstream os(path, std::ios::binary);
  if (!os) {
    Logger::warn("Unable to create file");
    return false;
  }

  // PREZ header
  const uint32_t magic = 0x5A455250u; // 'PREZ' LE
  os.write(reinterpret_cast<const char*>(&magic), sizeof magic);
  os.write(reinterpret_cast<const char*>(&checksum), sizeof checksum);
  os.write(reinterpret_cast<const char*>(&length), sizeof length);

  // blob
  os.write(reinterpret_cast<const char*>(blob.data()), blob.size());
  os.close();
  return static_cast<bool>(os);
}
}; // namespace io::recipe
