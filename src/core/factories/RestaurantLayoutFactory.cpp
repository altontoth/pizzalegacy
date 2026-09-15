/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/factories/RestaurantLayoutFactory.h"
#include "core/factories/FloorPatternFactory.h"
#include "core/factories/FurnitureFactory.h"
#include "core/restaurant/FloorPattern.h"
#include "core/util/LegacyConstants.h"
#include "engine/FormatCompat.h"
#include "engine/Logger.h"
#include "engine/Size.h"
#include "engine/geom/Spaces.h"
#include "resources/util/ByteReader.h"
#include "resources/util/FileUtils.h"

#include <algorithm>
#include <vector>

namespace {
// Rotation is stored as 240 plus the rotation index. A few layout files carry
// bytes outside that range; those items are placed unrotated.
int decode_rotation(uint8_t raw) {
  const int rotation = raw - 240;
  return (rotation >= 0 && rotation < 4) ? rotation : 0;
}

// Layout-file byte layout: a fixed header ending in the two record counts, then
// the furniture records and the oven records, all one stride wide.
constexpr std::size_t header_size = 41;
constexpr std::size_t furniture_count_offset = 39;
constexpr std::size_t record_size = 9;
} // namespace

std::unique_ptr<RestaurantLayout>
RestaurantLayoutFactory::create_layout(const std::string& filename) {
  return create_layout(fileutils::read_bytes(filename));
}

std::unique_ptr<RestaurantLayout>
RestaurantLayoutFactory::create_ai_layout(int index, int furniture_count) {
  std::vector<uint8_t> file =
      fileutils::read_bytes(compat::format("f/f{}.pc", index));
  if (file.size() < header_size || furniture_count < 0 ||
      furniture_count >= file[furniture_count_offset]) {
    return create_layout(file);
  }
  // Keep the first furniture_count records, then all of the ovens.
  const auto begin = static_cast<std::ptrdiff_t>(header_size);
  const auto end = static_cast<std::ptrdiff_t>(file.size());
  const auto kept = std::min(
      begin + static_cast<std::ptrdiff_t>(furniture_count * record_size), end);
  const auto all =
      std::min(begin + static_cast<std::ptrdiff_t>(
                           file[furniture_count_offset] * record_size),
               end);

  std::vector<uint8_t> buf(file.begin(), file.begin() + begin);
  buf[furniture_count_offset] = static_cast<uint8_t>(furniture_count);
  buf.insert(buf.end(), file.begin() + begin, file.begin() + kept);
  buf.insert(buf.end(), file.begin() + all, file.end());
  return create_layout(buf);
}

std::unique_ptr<RestaurantLayout>
RestaurantLayoutFactory::create_layout(std::span<const uint8_t> bytes) {
  fileutils::ByteReader is{bytes};

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
  is.get();                                   // u3
  is.get();                                   // u4
  is.get();                                   // u5
  auto rl = std::make_unique<RestaurantLayout>(
      Size{width, height}, Size{k_width, k_height}, door_offset,
      kitchen_entrance_offset);
  uint8_t floor_vendor = is.get() - 1;
  std::array<uint8_t, 4> tile_pattern;
  std::array<int8_t, 6> popularity_raw;
  is.read(tile_pattern.data(), 4);
  is.read(popularity_raw.data(), 6);
  is.skip(19);
  FloorPattern fp;
  fp.vendor_id = floor_vendor;
  fp.tile_indices = tile_pattern;

  for (size_t i = 0; i < popularity_raw.size(); i++) {
    fp.popularity[i] = popularity_raw[i];
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

  const int tile_w_px = LegacyConstants::RESTAURANT_TILE_WIDTH;
  const int tile_h_px = LegacyConstants::RESTAURANT_TILE_HEIGHT;
  auto legacy_kitchen_coords_to_map = [k_width, k_height](int x,
                                                          int y) -> WorldPoint {
    // Kitchen view is right-aligned (X) and bottom-aligned (Y). The world
    // background has k_width+2 / k_height+2 tiles (interior + walls added by
    // calculate_background_dimensions). The viewport shows
    // RESTAURANT_VISIBLE_X/Y_TILES tiles. The offset is
    // (background_tiles - viewport_tiles) in each axis.
    const int scroll_x_px =
        (k_width + 2 - LegacyConstants::RESTAURANT_VISIBLE_X_TILES) * tile_w_px;
    const int scroll_y_px =
        (k_height - LegacyConstants::RESTAURANT_VISIBLE_Y_TILES) * tile_h_px;

    WorldPoint interior_relative{
        x - LegacyConstants::WOOD_BORDER_WIDTH_LEFT -
            LegacyConstants::RESTAURANT_TILE_WIDTH + scroll_x_px,

        y - LegacyConstants::RESTAURANT_LEGACY_Y_BIAS + scroll_y_px -
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
    is.skip(3);
    uint16_t x_coord = is.u16le() * 2;
    // Y needs only a byte because the original biases it by 88.
    uint16_t y_coord = is.get() * 2;
    // If this is 0xFF then it means not linked; PlacedFurniture will turn this
    // into an empty optional
    uint16_t linked_id = is.get();
    const Furniture* f =
        FurnitureFactory::create_furniture((FurnitureType)furniture_id);
    auto pf = std::make_unique<PlacedFurniture>(
        f, core::FurnitureId{i},
        restaurant_point_from_legacy(x_coord, y_coord, height),
        core::FurnitureId{linked_id}, decode_rotation(rotation));
    rl->place_furniture(std::move(pf));
  }
  // Iterate ovens:
  for (int i = 0; i < oven_count; i++) {
    uint8_t furniture_id = is.get();
    uint8_t rotation = is.get();
    is.skip(3);
    uint16_t x_coord = is.u16le() * 2;
    uint16_t y_coord = is.get() * 2;
    uint16_t linked_id = is.get();
    const Furniture* f =
        FurnitureFactory::create_furniture((FurnitureType)furniture_id);
    auto pf = std::make_unique<PlacedFurniture>(
        f, core::FurnitureId{i + furniture_count},
        legacy_kitchen_coords_to_map(x_coord, y_coord),
        core::FurnitureId{linked_id}, decode_rotation(rotation));
    // Logger::debug("Oven at raw {},{} is placed at {}", x_coord, y_coord,
    //              pf->origin());
    rl->place_kitchen_furniture(std::move(pf));
  }
  return rl;
}
