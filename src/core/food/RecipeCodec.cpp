/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/food/RecipeCodec.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <span>
#include "core/food/Pizza.h"
#include "core/util/LegacyConstants.h"
#include "engine/Math.h"
#include "resources/BobSizeFn.h"

namespace io::recipe {
// Writes a custom PREZ recipe to the specified file
bool save_prez(const core::food::Pizza& pizza,
               const std::filesystem::path& path,
               const BobSizeFn& get_bob_size) {
  // Gather toppings
  const auto& tops = pizza.toppings();
  const uint8_t ingredient_count =
      static_cast<uint8_t>(std::min<std::size_t>(tops.size(), 255));

  // Distinct ingredient types for the "type count" byte
  std::set<int> distinct_ids;
  for (std::size_t i = 0; i < ingredient_count; ++i) {
    distinct_ids.insert(static_cast<int>(tops[i]->ingredient_type()));
  }
  const uint8_t type_count =
      static_cast<uint8_t>(std::min<std::size_t>(distinct_ids.size(), 255));

  // Build blob: N = 0x128 + 4 * ingredient_count
  const uint32_t N = 0x128u + (static_cast<uint32_t>(ingredient_count) << 2);
  std::vector<uint8_t> blob(N, 0);

  // Offsets from reverse-engineering
  constexpr int TAG_OFF = 0x17;
  constexpr int NAME_OFF = 0x18;
  constexpr int TYPE_CNT_OFF = 0x122;   // type count
  constexpr int ING_CNT_OFF = 0x123;    // ingredient count (byte)
  constexpr int ITEMS_BASE_OFF = 0x124; // then 4 bytes per item
  // Position mapping used by our loader:
  constexpr int x_offset = -LegacyConstants::WOOD_BORDER_WIDTH_LEFT;
  constexpr int y_offset = -LegacyConstants::WOOD_BORDER_HEIGHT_TOP;

  // Pizza ID byte: must be >= 0x0E to allow saving as custom,
  // since the first 14 pizzas on the menu are always 'canonical'.
  // Ideally we'd make this the id of the pizza on the recipe list,
  // but it doesn't matter anyway, so let's just hardcode something > 14.
  blob[TAG_OFF] = 0x20;

  // Name (NUL-terminated)
  {
    const std::string& name = pizza.name();
    const std::size_t max_name = TYPE_CNT_OFF - NAME_OFF; // safety
    const std::size_t n = std::min(name.size(), max_name ? max_name - 1 : 0);
    std::memcpy(&blob[NAME_OFF], name.data(), n);
    blob[NAME_OFF + n] = 0x00;
  }

  // 2c) Counts
  blob[TYPE_CNT_OFF] = type_count;
  blob[ING_CNT_OFF] = ingredient_count;

  // 2d) Items: [id, state, x_pos, y_pos]
  for (uint8_t i = 0; i < ingredient_count; ++i) {
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

    const uint8_t x_pos = pixel_to_grid(tl_x, x_offset);
    const uint8_t y_pos = pixel_to_grid(tl_y, y_offset);

    const int base = ITEMS_BASE_OFF + (4 * i);
    blob[base + 0] = static_cast<uint8_t>(id & 0xFF);
    blob[base + 1] = static_cast<uint8_t>(state & 0xFF);
    blob[base + 2] = x_pos;
    blob[base + 3] = y_pos;
  }

  // 2e) The final 4 bytes after items are zero terminators (already zeroed).
  //      They live at ITEMS_BASE_OFF + 4*ingredient_count .. +3.

  // 3) Compute checksum over the entire blob
  const uint32_t checksum =
      math::sum_bytes(std::as_bytes(std::span{blob}).first(N));

  std::ofstream os(path, std::ios::binary);
  if (!os) {
    Logger::warn("Unable to create file");
    return false;
  }

  // PREZ header
  const uint32_t magic = 0x5A455250u; // 'PREZ' LE
  os.write(reinterpret_cast<const char*>(&magic), sizeof magic);
  os.write(reinterpret_cast<const char*>(&checksum), sizeof checksum);
  os.write(reinterpret_cast<const char*>(&N), sizeof N);

  // blob
  os.write(reinterpret_cast<const char*>(blob.data()), blob.size());
  os.close();
  return static_cast<bool>(os);
}
}; // namespace io::recipe
