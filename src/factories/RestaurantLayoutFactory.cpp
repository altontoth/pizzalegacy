/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "factories/RestaurantLayoutFactory.h"
#include "core/restaurant/FloorPattern.h"
#include "core/util/LegacyConstants.h"
#include "engine/Logger.h"
#include "engine/Math.h"
#include "engine/Size.h"
#include "engine/geom/Spaces.h"
#include "factories/FloorPatternFactory.h"
#include "factories/FurnitureFactory.h"
#include "resources/util/FileUtils.h"

#include <algorithm>
#include <fstream>
#include <iostream>

std::unique_ptr<RestaurantLayout>
RestaurantLayoutFactory::create_layout(const std::string& filename) {

  std::ifstream is = fileutils::open(filename);
  if (!is) {
    std::cerr << "Failed to open file: " << filename << "\n";
    return nullptr;
  }

  uint8_t width = is.get();
  uint8_t height = is.get();
  uint8_t k_width = is.get();
  uint8_t k_height = is.get();
  uint8_t door_offset = is.get(); // entrance door offset; 0 = only 2 tiles
                                  // visible after vertical wall part at the
                                  // bottom, 1 = all three tiles visible, 2  =
                                  // one tile offset from vertical wall, etc.
  uint8_t kitchen_entrance_offset = is.get(); // 0 is immediately after
                                              // horizontal wall bit, 1 is one
                                              // down, etc.
  uint8_t u3 = is.get();
  uint8_t u4 = is.get();
  uint8_t u5 = is.get();
  auto rl = std::make_unique<RestaurantLayout>(
      Size{width, height}, Size{k_width, k_height}, door_offset,
      kitchen_entrance_offset);
  uint8_t floor_vendor = is.get() - 1;
  std::array<uint8_t, 4> tile_pattern;
  std::array<uint8_t, 6> popularity_raw;
  is.read((char*)tile_pattern.data(), 4);
  is.read((char*)popularity_raw.data(), 6);
  is.ignore(19);
  FloorPattern fp;
  fp.vendor_id = floor_vendor;
  fp.tile_indices = tile_pattern;

  for (size_t i = 0; i < 6; i++) {
    fp.popularity[i] = static_cast<int>(popularity_raw[i]);
  }
  const auto sid = static_cast<SupplierID>(floor_vendor);
  const auto& patterns = FloorPatternFactory::create(sid);
  auto it = std::ranges::find_if(patterns, [&fp](const FloorPattern& p) {
    return p.tile_indices == fp.tile_indices;
  });
  if (it != patterns.end()) {
    fp.popularity_factor = it->popularity_factor;
    fp.quality = it->quality;
    fp.price_per_tile = it->price_per_tile;
  }
  else {
    Logger::warn("RLF: could not find vendor in kachinfo for vendor {}",
                 floor_vendor);
  }
  rl->floor_pattern(fp);

  uint8_t furniture_count = is.get();
  uint8_t oven_count = is.get();

  // Original just did -88 at some point:
  constexpr int y_offset_from_original = 88 * 2;
  const int tile_w_px = LegacyConstants::RESTAURANT_TILE_WIDTH;
  const int tile_h_px = LegacyConstants::RESTAURANT_TILE_HEIGHT;
  constexpr int VISIBLE_X_TILES = 16;
  constexpr int VISIBLE_Y_TILES = 14;
  // Legacy coordinates are based on what's in view when we are scrolled
  // at the bottom left of the restaurant, and then from the top left
  // coordinate, ignoring the border and any wall tiles. Because of the
  // -88 they did you could have restaurants that were a bit bigger than
  // the screen and still put furniture there.

  // In our coordinate system, WorldPoint (0,0) is the TOP-LEFT WALL.
  // Interior starts at (tile_w, tile_h). So we convert legacy coords to
  // interior-relative, then add tile offsets to make them wall-relative.
  auto legacy_coords_to_map = [height](int x, int y) -> WorldPoint {
    WorldPoint interior_relative{x - LegacyConstants::WOOD_BORDER_WIDTH_LEFT -
                                     LegacyConstants::RESTAURANT_TILE_WIDTH,
                                 y - y_offset_from_original +
                                     ((height - VISIBLE_Y_TILES) * tile_h_px) -
                                     LegacyConstants::WOOD_BORDER_HEIGHT_TOP -
                                     LegacyConstants::RESTAURANT_TILE_HEIGHT};
    // Add wall offset to convert to world coordinates
    return WorldPoint{interior_relative.x + tile_w_px,
                      interior_relative.y + tile_h_px};
  };

  auto legacy_kitchen_coords_to_map = [k_width, k_height](int x,
                                                          int y) -> WorldPoint {
    // Kitchen view is right-aligned (X) and bottom-aligned (Y). The world
    // background has k_width+2 / k_height+2 tiles (interior + walls added by
    // calculate_background_dimensions). The viewport shows VISIBLE_X/Y tiles.
    // The offset is (background_tiles - viewport_tiles) in each axis.
    const int scroll_x_px = (k_width + 2 - VISIBLE_X_TILES) * tile_w_px;
    const int scroll_y_px = (k_height - VISIBLE_Y_TILES) * tile_h_px;

    WorldPoint interior_relative{x - LegacyConstants::WOOD_BORDER_WIDTH_LEFT -
                                     LegacyConstants::RESTAURANT_TILE_WIDTH +
                                     scroll_x_px,

                                 y - y_offset_from_original + scroll_y_px -
                                     LegacyConstants::WOOD_BORDER_HEIGHT_TOP -
                                     LegacyConstants::RESTAURANT_TILE_HEIGHT};

    // Add wall offset to convert to world coordinates
    return WorldPoint{interior_relative.x + tile_w_px,
                      interior_relative.y + tile_h_px};
  };

  // Iterate furniture
  for (int i = 0; i < furniture_count; i++) {
    uint8_t furniture_id = is.get();
    uint8_t rotation = is.get();
    is.seekg(2, std::ios::cur);
    char high = is.get();
    char low = is.get();
    uint16_t x_coord = math::two_bytes_to_uint16(low, high) * 2;
    high = is.get();
    low = is.get();
    uint16_t y_coord = math::two_bytes_to_uint16(low, high) * 2;
    // If this is 0xFF then it means not linked; PlacedFurniture will turn this
    // into an empty optional
    uint16_t linked_id = is.get();
    const Furniture* f =
        FurnitureFactory::create_furniture((FurnitureType)furniture_id);
    auto pf = std::make_unique<PlacedFurniture>(
        f, core::FurnitureId{i}, legacy_coords_to_map(x_coord, y_coord),
        core::FurnitureId{linked_id}, rotation - 240);
    rl->place_furniture(std::move(pf));
  }
  // Iterate ovens:
  for (int i = 0; i < oven_count; i++) {
    uint8_t furniture_id = is.get();
    uint8_t rotation = is.get();
    is.seekg(2, std::ios::cur);
    char high = is.get();
    char low = is.get();
    uint16_t x_coord = math::two_bytes_to_uint16(low, high) * 2;
    high = is.get();
    low = is.get();
    uint16_t y_coord = math::two_bytes_to_uint16(low, high) * 2;
    uint16_t linked_id = is.get();
    const Furniture* f =
        FurnitureFactory::create_furniture((FurnitureType)furniture_id);
    auto pf = std::make_unique<PlacedFurniture>(
        f, core::FurnitureId{i + furniture_count},
        legacy_kitchen_coords_to_map(x_coord, y_coord),
        core::FurnitureId{linked_id}, rotation > 0 ? rotation - 240 : 0);
    // Logger::debug("Oven at raw {},{} is placed at {}", x_coord, y_coord,
    //              pf->origin());
    rl->place_kitchen_furniture(std::move(pf));
  }
  return rl;
}
