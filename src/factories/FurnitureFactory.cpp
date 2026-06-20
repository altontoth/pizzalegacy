/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "factories/FurnitureFactory.h"

#include "core/supply/Supplier.h"

#include <ranges>

std::unordered_map<FurnitureType, std::unique_ptr<const Furniture>>
    FurnitureFactory::cache_ = {};

std::vector<const Furniture*> FurnitureFactory::all_furniture() {

  std::vector<const Furniture*> list;
  for (int i = static_cast<int>(FurnitureType::TRADITIO_ROMANO);
       i <= static_cast<int>(FurnitureType::ASHES_KITCHEN); i++) {
    list.push_back(create_furniture(static_cast<FurnitureType>(i)));
  }

  return list;
}

std::vector<const Furniture*>
FurnitureFactory::all_furniture_by_supplier(SupplierID supplier) {

  auto suppliers = all_furniture();
  auto filtered =
      suppliers | std::ranges::views::filter([supplier](const Furniture* f) {
        return f && f->supplier() == static_cast<int>(supplier);
      });
  return std::vector<const Furniture*>(filtered.begin(), filtered.end());
}

std::optional<FurnitureType>
FurnitureFactory::get_furniture_type(const Furniture* furniture) {
  if (!furniture) {
    return std::nullopt;
  }

  // Search through the cache to find the matching furniture pointer
  for (const auto& [type, cached_furniture] : cache_) {
    if (cached_furniture.get() == furniture) {
      return type;
    }
  }

  return std::nullopt;
}

const Furniture*
FurnitureFactory::create_furniture(FurnitureType furniture_type) {
  // Check if the Furniture instance already exists in the cache
  auto it = cache_.find(furniture_type);
  if (it != cache_.end()) {
    return it->second.get(); // Return the cached instance
  }
  std::unique_ptr<const Furniture> furniture;
  switch (furniture_type) {
  case FurnitureType::TRADITIO_ROMANO:
    furniture = std::make_unique<Furniture>(
        FurnitureType::TRADITIO_ROMANO,
        2,                                 // type
        30,                                // q
        12,                                // supplier
        "Traditio Romano",                 // name
        180,                               // price
        std::vector<int>{0, 1, 2, 3},      // bobs
        static_cast<FurnitureCategory>(2), // category
        4,                                 // u2
        1,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(1),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            2, 2, 2, 2, 2, 2} // popularity
    );
    break;

  case FurnitureType::NAPOLEON:
    furniture = std::make_unique<Furniture>(
        FurnitureType::NAPOLEON,
        2,                                 // type
        50,                                // q
        13,                                // supplier
        "Napoleon",                        // name
        300,                               // price
        std::vector<int>{4, 5, 6, 7},      // bobs
        static_cast<FurnitureCategory>(2), // category
        5,                                 // u2
        1,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(1),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            0, 0, 0, 2, 3, 4} // popularity
    );
    break;

  case FurnitureType::TSAR:
    furniture = std::make_unique<Furniture>(
        FurnitureType::TSAR,
        2,                                 // type
        50,                                // q
        13,                                // supplier
        "Tsar",                            // name
        300,                               // price
        std::vector<int>{8, 9, 10, 11},    // bobs
        static_cast<FurnitureCategory>(2), // category
        8,                                 // u2
        1,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(1),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            0, 0, 0, 2, 3, 4} // popularity
    );
    break;

  case FurnitureType::MISS_MARPLE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::MISS_MARPLE,
        2,                                 // type
        82,                                // q
        24,                                // supplier
        "Miss Marple",                     // name
        1800,                              // price
        std::vector<int>{12, 13, 14, 15},  // bobs
        static_cast<FurnitureCategory>(2), // category
        250,                               // u2
        1,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(1),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            -5, -4, -1, 7, 12, 16} // popularity
    );
    break;

  case FurnitureType::CLASSIC_LIGHT:
    furniture = std::make_unique<Furniture>(
        FurnitureType::CLASSIC_LIGHT,
        2,                                 // type
        36,                                // q
        12,                                // supplier
        "Classic Light",                   // name
        260,                               // price
        std::vector<int>{16, 17, 18, 19},  // bobs
        static_cast<FurnitureCategory>(2), // category
        108,                               // u2
        1,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(1),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            3, 3, 3, 3, 3, 3} // popularity
    );
    break;

  case FurnitureType::CLASSIC_YELLOW:
    furniture = std::make_unique<Furniture>(
        FurnitureType::CLASSIC_YELLOW,
        2,                                 // type
        32,                                // q
        12,                                // supplier
        "Classic Yellow",                  // name
        240,                               // price
        std::vector<int>{24, 25, 26, 27},  // bobs
        static_cast<FurnitureCategory>(2), // category
        109,                               // u2
        1,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(1),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            3, 3, 3, 2, 2, 3} // popularity
    );
    break;

  case FurnitureType::CLASSIC_BLUE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::CLASSIC_BLUE,
        2,                                 // type
        30,                                // q
        12,                                // supplier
        "Classic Blue",                    // name
        250,                               // price
        std::vector<int>{20, 21, 22, 23},  // bobs
        static_cast<FurnitureCategory>(2), // category
        8,                                 // u2
        1,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(1),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            2, 3, 4, 3, 2, 2} // popularity
    );
    break;

  case FurnitureType::SIBIRIA:
    furniture = std::make_unique<Furniture>(
        FurnitureType::SIBIRIA,
        2,                                 // type
        38,                                // q
        13,                                // supplier
        "Sibiria",                         // name
        450,                               // price
        std::vector<int>{28, 29, 30, 31},  // bobs
        static_cast<FurnitureCategory>(2), // category
        10,                                // u2
        1,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(1),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            1, 0, 0, 3, 3, 4} // popularity
    );
    break;

  case FurnitureType::WESTERN:
    furniture = std::make_unique<Furniture>(
        FurnitureType::WESTERN,
        2,                                 // type
        40,                                // q
        13,                                // supplier
        "Western",                         // name
        400,                               // price
        std::vector<int>{32, 33, 34, 35},  // bobs
        static_cast<FurnitureCategory>(2), // category
        10,                                // u2
        1,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(1),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            1, 1, 1, 3, 3, 4} // popularity
    );
    break;

  case FurnitureType::STRIP_CHAIR_YELLOW:
    furniture = std::make_unique<Furniture>(
        FurnitureType::STRIP_CHAIR_YELLOW,
        2,                                 // type
        95,                                // q
        24,                                // supplier
        "Strip Chair Yellow",              // name
        1300,                              // price
        std::vector<int>{36, 37, 38, 39},  // bobs
        static_cast<FurnitureCategory>(2), // category
        3,                                 // u2
        1,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(1),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            -1, -1, 0, 5, 10, 12} // popularity
    );
    break;

  case FurnitureType::STRIP_CHAIR_BLUE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::STRIP_CHAIR_BLUE,
        2,                                 // type
        95,                                // q
        24,                                // supplier
        "Strip Chair Blue",                // name
        1300,                              // price
        std::vector<int>{40, 41, 42, 43},  // bobs
        static_cast<FurnitureCategory>(2), // category
        250,                               // u2
        1,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(1),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            -1, -1, 0, 5, 10, 12} // popularity
    );
    break;

  case FurnitureType::CAESAR_GREEN:
    furniture = std::make_unique<Furniture>(
        FurnitureType::CAESAR_GREEN,
        2,                                 // type
        60,                                // q
        13,                                // supplier
        "Caesar Green",                    // name
        590,                               // price
        std::vector<int>{44, 45, 44, 45},  // bobs
        static_cast<FurnitureCategory>(2), // category
        250,                               // u2
        1,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(1),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            0, 0, 0, 3, 4, 5} // popularity
    );
    break;

  case FurnitureType::CAESAR_YELLOW:
    furniture = std::make_unique<Furniture>(
        FurnitureType::CAESAR_YELLOW,
        2,                                 // type
        60,                                // q
        13,                                // supplier
        "Caesar Yellow",                   // name
        590,                               // price
        std::vector<int>{46, 47, 46, 47},  // bobs
        static_cast<FurnitureCategory>(2), // category
        250,                               // u2
        1,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(1),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            0, 0, 0, 3, 3, 4} // popularity
    );
    break;

  case FurnitureType::CAESAR_BLUE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::CAESAR_BLUE,
        2,                                 // type
        60,                                // q
        13,                                // supplier
        "Caesar Blue",                     // name
        590,                               // price
        std::vector<int>{48, 49, 48, 49},  // bobs
        static_cast<FurnitureCategory>(2), // category
        250,                               // u2
        1,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(1),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            0, 0, 0, 3, 4, 5} // popularity
    );
    break;

  case FurnitureType::BELGRADE_BLUE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::BELGRADE_BLUE,
        2,                                 // type
        45,                                // q
        22,                                // supplier
        "Belgrade Blue",                   // name
        400,                               // price
        std::vector<int>{50, 51, 52, 53},  // bobs
        static_cast<FurnitureCategory>(2), // category
        6,                                 // u2
        1,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(1),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            3, 3, 3, 3, 3, 3} // popularity
    );
    break;

  case FurnitureType::BELGRADE_GREEN:
    furniture = std::make_unique<Furniture>(
        FurnitureType::BELGRADE_GREEN,
        2,                                 // type
        45,                                // q
        22,                                // supplier
        "Belgrade Green",                  // name
        400,                               // price
        std::vector<int>{54, 55, 56, 57},  // bobs
        static_cast<FurnitureCategory>(2), // category
        2,                                 // u2
        1,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(1),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            4, 3, 3, 4, 3, 3} // popularity
    );
    break;

  case FurnitureType::FOUNTAIN:
    furniture = std::make_unique<Furniture>(
        FurnitureType::FOUNTAIN,
        4,                                    // type
        70,                                   // q
        26,                                   // supplier
        "Fountain",                           // name
        9000,                                 // price
        std::vector<int>{324, 324, 324, 324}, // bobs
        static_cast<FurnitureCategory>(3),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            7, 3, 2, 12, 20, 20} // popularity
    );
    break;

  case FurnitureType::CAPPUCCINO:
    furniture = std::make_unique<Furniture>(
        FurnitureType::CAPPUCCINO,
        2,                                 // type
        46,                                // q
        12,                                // supplier
        "Cappuccino",                      // name
        950,                               // price
        std::vector<int>{62, 62, 62, 62},  // bobs
        static_cast<FurnitureCategory>(1), // category
        18,                                // u2
        2,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(2),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            5, 6, 6, 6, 6, 5} // popularity
    );
    break;

  case FurnitureType::SIZILIANO:
    furniture = std::make_unique<Furniture>(
        FurnitureType::SIZILIANO,
        2,                                 // type
        52,                                // q
        12,                                // supplier
        "Siziliano",                       // name
        1400,                              // price
        std::vector<int>{63, 63, 63, 63},  // bobs
        static_cast<FurnitureCategory>(1), // category
        2,                                 // u2
        2,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(2),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            6, 8, 4, 8, 8, 8} // popularity
    );
    break;

  case FurnitureType::ROUND_GRILL:
    furniture = std::make_unique<Furniture>(
        FurnitureType::ROUND_GRILL,
        2,                                 // type
        20,                                // q
        15,                                // supplier
        "Round Grill",                     // name
        100,                               // price
        std::vector<int>{64, 64, 64, 64},  // bobs
        static_cast<FurnitureCategory>(1), // category
        21,                                // u2
        2,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(2),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            2, 2, 2, 2, 2, 2} // popularity
    );
    break;

  case FurnitureType::CITY_LINE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::CITY_LINE,
        2,                                 // type
        46,                                // q
        12,                                // supplier
        "City Line",                       // name
        750,                               // price
        std::vector<int>{65, 65, 65, 65},  // bobs
        static_cast<FurnitureCategory>(1), // category
        18,                                // u2
        2,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(2),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            4, 5, 5, 4, 3, 2} // popularity
    );
    break;

  case FurnitureType::SKY_LINE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::SKY_LINE,
        2,                                 // type
        36,                                // q
        12,                                // supplier
        "Sky Line",                        // name
        650,                               // price
        std::vector<int>{66, 66, 66, 66},  // bobs
        static_cast<FurnitureCategory>(1), // category
        20,                                // u2
        2,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(2),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            4, 4, 3, 3, 4, 3} // popularity
    );
    break;

  case FurnitureType::FOUR_PLACE_SETTING:
    furniture = std::make_unique<Furniture>(
        FurnitureType::FOUR_PLACE_SETTING,
        2,                                 // type
        60,                                // q
        13,                                // supplier
        "Four Place Setting",              // name
        3450,                              // price
        std::vector<int>{67, 68, 67, 68},  // bobs
        static_cast<FurnitureCategory>(1), // category
        25,                                // u2
        4,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(2),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            3, 3, 4, 4, 4, 4} // popularity
    );
    break;

  case FurnitureType::MARBLE_OVAL:
    furniture = std::make_unique<Furniture>(
        FurnitureType::MARBLE_OVAL,
        2,                                 // type
        85,                                // q
        24,                                // supplier
        "Marble Oval",                     // name
        15000,                             // price
        std::vector<int>{69, 70, 69, 70},  // bobs
        static_cast<FurnitureCategory>(1), // category
        250,                               // u2
        4,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(2),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            2, 2, 7, 10, 25, 30} // popularity
    );
    break;

  case FurnitureType::ROYAL:
    furniture = std::make_unique<Furniture>(
        FurnitureType::ROYAL,
        2,                                 // type
        90,                                // q
        24,                                // supplier
        "Royal",                           // name
        12500,                             // price
        std::vector<int>{71, 71, 71, 71},  // bobs
        static_cast<FurnitureCategory>(1), // category
        250,                               // u2
        2,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(2),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            5, 8, 8, 12, 25, 30} // popularity
    );
    break;

  case FurnitureType::OVAL_SETTING:
    furniture = std::make_unique<Furniture>(
        FurnitureType::OVAL_SETTING,
        2,                                 // type
        70,                                // q
        13,                                // supplier
        "Oval setting",                    // name
        4500,                              // price
        std::vector<int>{72, 73, 72, 73},  // bobs
        static_cast<FurnitureCategory>(1), // category
        23,                                // u2
        6,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(2),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            4, 4, 5, 8, 8, 6} // popularity
    );
    break;

  case FurnitureType::MC_DONUT_SMALL:
    furniture = std::make_unique<Furniture>(
        FurnitureType::MC_DONUT_SMALL,
        2,                                 // type
        50,                                // q
        20,                                // supplier
        "Mc Donut small",                  // name
        900,                               // price
        std::vector<int>{74, 74, 74, 74},  // bobs
        static_cast<FurnitureCategory>(1), // category
        111,                               // u2
        2,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(2),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            6, 5, 3, 2, 2, 2} // popularity
    );
    break;

  case FurnitureType::WOODEN_CHAIR:
    furniture = std::make_unique<Furniture>(
        FurnitureType::WOODEN_CHAIR,
        2,                                 // type
        25,                                // q
        14,                                // supplier
        "Wooden Chair",                    // name
        80,                                // price
        std::vector<int>{75, 76, 77, 78},  // bobs
        static_cast<FurnitureCategory>(2), // category
        34,                                // u2
        1,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(1),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            2, 2, 2, 2, 2, 2} // popularity
    );
    break;

  case FurnitureType::BAVARIAN_CHAIR:
    furniture = std::make_unique<Furniture>(
        FurnitureType::BAVARIAN_CHAIR,
        2,                                 // type
        22,                                // q
        14,                                // supplier
        "Bavarian Chair",                  // name
        70,                                // price
        std::vector<int>{79, 80, 81, 82},  // bobs
        static_cast<FurnitureCategory>(2), // category
        35,                                // u2
        1,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(1),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            1, 1, 2, 2, 2, 2} // popularity
    );
    break;

  case FurnitureType::MODERN_SITTING:
    furniture = std::make_unique<Furniture>(
        FurnitureType::MODERN_SITTING,
        2,                                 // type
        58,                                // q
        23,                                // supplier
        "Modern Sitting",                  // name
        1600,                              // price
        std::vector<int>{83, 84, 85, 86},  // bobs
        static_cast<FurnitureCategory>(2), // category
        80,                                // u2
        1,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(1),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            4, 4, 4, 6, 10, 4} // popularity
    );
    break;

  case FurnitureType::WAITING_ROOM_WHITE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::WAITING_ROOM_WHITE,
        2,                                 // type
        30,                                // q
        25,                                // supplier
        "Waiting Room White",              // name
        190,                               // price
        std::vector<int>{87, 88, 89, 90},  // bobs
        static_cast<FurnitureCategory>(2), // category
        89,                                // u2
        1,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(1),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            3, 3, 3, 1, 1, 0} // popularity
    );
    break;

  case FurnitureType::WAITING_ROOM_GREEN:
    furniture = std::make_unique<Furniture>(
        FurnitureType::WAITING_ROOM_GREEN,
        2,                                 // type
        30,                                // q
        25,                                // supplier
        "Waiting Room Green",              // name
        190,                               // price
        std::vector<int>{91, 92, 93, 94},  // bobs
        static_cast<FurnitureCategory>(2), // category
        86,                                // u2
        1,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(1),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            3, 3, 3, 1, 0, -1} // popularity
    );
    break;

  case FurnitureType::WAITING_ROOM_RED:
    furniture = std::make_unique<Furniture>(
        FurnitureType::WAITING_ROOM_RED,
        2,                                 // type
        30,                                // q
        25,                                // supplier
        "Waiting Room Red",                // name
        190,                               // price
        std::vector<int>{95, 96, 97, 98},  // bobs
        static_cast<FurnitureCategory>(2), // category
        87,                                // u2
        1,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(1),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            3, 3, 4, 1, 0, -1} // popularity
    );
    break;

  case FurnitureType::WOODEN_BENCH:
    furniture = std::make_unique<Furniture>(
        FurnitureType::WOODEN_BENCH,
        2,                                  // type
        20,                                 // q
        21,                                 // supplier
        "Wooden Bench",                     // name
        250,                                // price
        std::vector<int>{99, 100, 99, 100}, // bobs
        static_cast<FurnitureCategory>(2),  // category
        14,                                 // u2
        2,                                  // capacity
        0,                                  // oven_bake_time
        static_cast<PlacementLayer>(1),     // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            2, 2, 3, 3, 1, 1} // popularity
    );
    break;

  case FurnitureType::SALOON_RED:
    furniture = std::make_unique<Furniture>(
        FurnitureType::SALOON_RED,
        2,                                    // type
        30,                                   // q
        14,                                   // supplier
        "Saloon Red",                         // name
        150,                                  // price
        std::vector<int>{101, 102, 103, 104}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        4,                                    // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            3, 3, 3, 3, 3, 3} // popularity
    );
    break;

  case FurnitureType::SALOON_GREEN:
    furniture = std::make_unique<Furniture>(
        FurnitureType::SALOON_GREEN,
        2,                                    // type
        30,                                   // q
        14,                                   // supplier
        "Saloon Green",                       // name
        150,                                  // price
        std::vector<int>{105, 106, 107, 108}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        4,                                    // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            3, 3, 3, 3, 3, 3} // popularity
    );
    break;

  case FurnitureType::SALOON_WHITE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::SALOON_WHITE,
        2,                                    // type
        30,                                   // q
        14,                                   // supplier
        "Saloon White",                       // name
        150,                                  // price
        std::vector<int>{109, 110, 111, 112}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        4,                                    // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            2, 2, 2, 2, 2, 2} // popularity
    );
    break;

  case FurnitureType::HARDCORE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::HARDCORE,
        2,                                    // type
        25,                                   // q
        16,                                   // supplier
        "Hardcore",                           // name
        200,                                  // price
        std::vector<int>{113, 114, 115, 116}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        1,                                    // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            2, 2, 4, 2, 1, 1} // popularity
    );
    break;

  case FurnitureType::KITCHEN_WHITE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::KITCHEN_WHITE,
        2,                                    // type
        20,                                   // q
        15,                                   // supplier
        "Kitchen White",                      // name
        40,                                   // price
        std::vector<int>{117, 118, 119, 120}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        30,                                   // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            1, 1, 1, 1, 1, 1} // popularity
    );
    break;

  case FurnitureType::KITCHEN_BLUE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::KITCHEN_BLUE,
        2,                                    // type
        20,                                   // q
        15,                                   // supplier
        "Kitchen Blue",                       // name
        40,                                   // price
        std::vector<int>{121, 122, 123, 124}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        31,                                   // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            1, 1, 1, 1, 1, 1} // popularity
    );
    break;

  case FurnitureType::KITCHEN_GREEN:
    furniture = std::make_unique<Furniture>(
        FurnitureType::KITCHEN_GREEN,
        2,                                    // type
        20,                                   // q
        15,                                   // supplier
        "Kitchen Green",                      // name
        40,                                   // price
        std::vector<int>{125, 126, 127, 128}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        32,                                   // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            1, 1, 1, 1, 1, 1} // popularity
    );
    break;

  case FurnitureType::GARDEN_WHITE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::GARDEN_WHITE,
        2,                                    // type
        28,                                   // q
        16,                                   // supplier
        "Garden White",                       // name
        75,                                   // price
        std::vector<int>{129, 130, 131, 132}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        30,                                   // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            2, 2, 3, 3, 1, 1} // popularity
    );
    break;

  case FurnitureType::GARDEN_YELLOW:
    furniture = std::make_unique<Furniture>(
        FurnitureType::GARDEN_YELLOW,
        2,                                    // type
        28,                                   // q
        16,                                   // supplier
        "Garden Yellow",                      // name
        75,                                   // price
        std::vector<int>{133, 134, 135, 136}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        31,                                   // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            2, 2, 3, 3, 1, 1} // popularity
    );
    break;

  case FurnitureType::GARDEN_RED:
    furniture = std::make_unique<Furniture>(
        FurnitureType::GARDEN_RED,
        2,                                    // type
        28,                                   // q
        16,                                   // supplier
        "Garden Red",                         // name
        75,                                   // price
        std::vector<int>{137, 138, 139, 140}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        32,                                   // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            2, 2, 3, 3, 1, 1} // popularity
    );
    break;

  case FurnitureType::GARDEN_BLUE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::GARDEN_BLUE,
        2,                                    // type
        28,                                   // q
        16,                                   // supplier
        "Garden Blue",                        // name
        75,                                   // price
        std::vector<int>{141, 142, 143, 144}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        36,                                   // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            2, 2, 3, 3, 1, 1} // popularity
    );
    break;

  case FurnitureType::BASIC_TABLE_WHITE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::BASIC_TABLE_WHITE,
        2,                                    // type
        40,                                   // q
        14,                                   // supplier
        "Basic Table White",                  // name
        350,                                  // price
        std::vector<int>{145, 145, 145, 145}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        110,                                  // u2
        2,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            4, 4, 4, 4, 4, 4} // popularity
    );
    break;

  case FurnitureType::BASIC_TABLE_GREEN:
    furniture = std::make_unique<Furniture>(
        FurnitureType::BASIC_TABLE_GREEN,
        2,                                    // type
        40,                                   // q
        14,                                   // supplier
        "Basic Table Green",                  // name
        350,                                  // price
        std::vector<int>{146, 146, 146, 146}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        109,                                  // u2
        2,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            5, 5, 5, 3, 3, 3} // popularity
    );
    break;

  case FurnitureType::BASIC_TABLE_BLUE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::BASIC_TABLE_BLUE,
        2,                                    // type
        40,                                   // q
        14,                                   // supplier
        "Basic Table Blue",                   // name
        350,                                  // price
        std::vector<int>{147, 147, 147, 147}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        110,                                  // u2
        2,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            4, 4, 4, 4, 2, 3} // popularity
    );
    break;

  case FurnitureType::SMALL_CHECK_GREEN:
    furniture = std::make_unique<Furniture>(
        FurnitureType::SMALL_CHECK_GREEN,
        2,                                    // type
        30,                                   // q
        25,                                   // supplier
        "Small Check Green",                  // name
        200,                                  // price
        std::vector<int>{148, 148, 148, 148}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        26,                                   // u2
        2,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            3, 4, 3, 3, 0, 3} // popularity
    );
    break;

  case FurnitureType::SMALL_CHECK_RED:
    furniture = std::make_unique<Furniture>(
        FurnitureType::SMALL_CHECK_RED,
        2,                                    // type
        30,                                   // q
        25,                                   // supplier
        "Small Check Red",                    // name
        200,                                  // price
        std::vector<int>{149, 149, 149, 149}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        26,                                   // u2
        2,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            3, 3, 4, 3, 0, 3} // popularity
    );
    break;

  case FurnitureType::SMALL_CHECK_BLUE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::SMALL_CHECK_BLUE,
        2,                                    // type
        30,                                   // q
        25,                                   // supplier
        "Small Check Blue",                   // name
        200,                                  // price
        std::vector<int>{150, 150, 150, 150}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        26,                                   // u2
        2,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            3, 3, 3, 4, 0, 3} // popularity
    );
    break;

  case FurnitureType::GARDEN_TABLE_RED:
    furniture = std::make_unique<Furniture>(
        FurnitureType::GARDEN_TABLE_RED,
        2,                                    // type
        30,                                   // q
        16,                                   // supplier
        "Garden Table Red",                   // name
        320,                                  // price
        std::vector<int>{151, 152, 151, 152}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        62,                                   // u2
        4,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            2, 2, 2, 1, 1, 1} // popularity
    );
    break;

  case FurnitureType::GARDEN_TABLE_BLUE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::GARDEN_TABLE_BLUE,
        2,                                    // type
        30,                                   // q
        16,                                   // supplier
        "Garden Table Blue",                  // name
        320,                                  // price
        std::vector<int>{153, 154, 153, 154}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        61,                                   // u2
        4,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            1, 1, 1, 2, 2, 2} // popularity
    );
    break;

  case FurnitureType::PLASTIC_BLACK:
    furniture = std::make_unique<Furniture>(
        FurnitureType::PLASTIC_BLACK,
        2,                                    // type
        20,                                   // q
        15,                                   // supplier
        "Plastic Black",                      // name
        160,                                  // price
        std::vector<int>{155, 155, 155, 155}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        57,                                   // u2
        2,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            2, 2, 2, 2, 2, 2} // popularity
    );
    break;

  case FurnitureType::PLASTIC_WHITE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::PLASTIC_WHITE,
        2,                                    // type
        20,                                   // q
        15,                                   // supplier
        "Plastic White",                      // name
        160,                                  // price
        std::vector<int>{156, 156, 156, 156}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        45,                                   // u2
        2,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            2, 2, 2, 2, 2, 2} // popularity
    );
    break;

  case FurnitureType::PLASTIC_RED:
    furniture = std::make_unique<Furniture>(
        FurnitureType::PLASTIC_RED,
        2,                                    // type
        20,                                   // q
        15,                                   // supplier
        "Plastic Red",                        // name
        160,                                  // price
        std::vector<int>{157, 157, 157, 157}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        57,                                   // u2
        2,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            2, 1, 2, 1, 1, 1} // popularity
    );
    break;

  case FurnitureType::PLASTIC_GREEN:
    furniture = std::make_unique<Furniture>(
        FurnitureType::PLASTIC_GREEN,
        2,                                    // type
        20,                                   // q
        15,                                   // supplier
        "Plastic Green",                      // name
        160,                                  // price
        std::vector<int>{158, 158, 158, 158}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        46,                                   // u2
        2,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            2, 1, 1, 2, 1, 1} // popularity
    );
    break;

  case FurnitureType::WOOD_DARK:
    furniture = std::make_unique<Furniture>(
        FurnitureType::WOOD_DARK,
        2,                                    // type
        34,                                   // q
        14,                                   // supplier
        "Wood dark",                          // name
        200,                                  // price
        std::vector<int>{159, 159, 159, 159}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        58,                                   // u2
        2,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            3, 3, 3, 3, 3, 3} // popularity
    );
    break;

  case FurnitureType::WOOD_LIGHT:
    furniture = std::make_unique<Furniture>(
        FurnitureType::WOOD_LIGHT,
        2,                                    // type
        34,                                   // q
        14,                                   // supplier
        "Wood light",                         // name
        240,                                  // price
        std::vector<int>{160, 160, 160, 160}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        45,                                   // u2
        2,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            4, 4, 3, 3, 3, 4} // popularity
    );
    break;

  case FurnitureType::LARGE_CHECK_RED:
    furniture = std::make_unique<Furniture>(
        FurnitureType::LARGE_CHECK_RED,
        2,                                    // type
        35,                                   // q
        25,                                   // supplier
        "Large Check Red",                    // name
        400,                                  // price
        std::vector<int>{161, 162, 161, 162}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        117,                                  // u2
        4,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            4, 4, 5, 4, 0, 1} // popularity
    );
    break;

  case FurnitureType::LARGE_CHECK_BLUE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::LARGE_CHECK_BLUE,
        2,                                    // type
        35,                                   // q
        25,                                   // supplier
        "Large Check Blue",                   // name
        400,                                  // price
        std::vector<int>{163, 164, 163, 164}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        47,                                   // u2
        4,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            4, 4, 4, 4, 0, 1} // popularity
    );
    break;

  case FurnitureType::OAK_DARK:
    furniture = std::make_unique<Furniture>(
        FurnitureType::OAK_DARK,
        2,                                    // type
        38,                                   // q
        21,                                   // supplier
        "Oak dark",                           // name
        450,                                  // price
        std::vector<int>{165, 166, 165, 166}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        62,                                   // u2
        4,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            3, 3, 4, 3, 1, 4} // popularity
    );
    break;

  case FurnitureType::OAK_LIGHT:
    furniture = std::make_unique<Furniture>(
        FurnitureType::OAK_LIGHT,
        2,                                    // type
        38,                                   // q
        21,                                   // supplier
        "Oak light",                          // name
        550,                                  // price
        std::vector<int>{167, 168, 167, 168}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        22,                                   // u2
        4,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            4, 3, 3, 3, 2, 3} // popularity
    );
    break;

  case FurnitureType::BISTRO_RED:
    furniture = std::make_unique<Furniture>(
        FurnitureType::BISTRO_RED,
        2,                                    // type
        24,                                   // q
        20,                                   // supplier
        "Bistro Red",                         // name
        1200,                                 // price
        std::vector<int>{169, 169, 169, 169}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        21,                                   // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            5, 3, 2, 2, 0, 1} // popularity
    );
    break;

  case FurnitureType::BISTRO_BLUE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::BISTRO_BLUE,
        2,                                    // type
        24,                                   // q
        20,                                   // supplier
        "Bistro Blue",                        // name
        1200,                                 // price
        std::vector<int>{170, 170, 170, 170}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        47,                                   // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            4, 3, 2, 2, 2, 1} // popularity
    );
    break;

  case FurnitureType::BISTRO_YELLOW:
    furniture = std::make_unique<Furniture>(
        FurnitureType::BISTRO_YELLOW,
        2,                                    // type
        24,                                   // q
        20,                                   // supplier
        "Bistro Yellow",                      // name
        1200,                                 // price
        std::vector<int>{171, 171, 171, 171}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        21,                                   // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            4, 3, 2, 2, 1, 1} // popularity
    );
    break;

  case FurnitureType::MODERN_MIX:
    furniture = std::make_unique<Furniture>(
        FurnitureType::MODERN_MIX,
        2,                                    // type
        33,                                   // q
        17,                                   // supplier
        "Modern Mix",                         // name
        300,                                  // price
        std::vector<int>{172, 173, 174, 175}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            2, 2, 2, 0, 1, 0} // popularity
    );
    break;

  case FurnitureType::OFFICE_CHAIR:
    furniture = std::make_unique<Furniture>(
        FurnitureType::OFFICE_CHAIR,
        2,                                    // type
        24,                                   // q
        17,                                   // supplier
        "Office Chair",                       // name
        150,                                  // price
        std::vector<int>{176, 177, 178, 179}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            1, 2, 2, 3, 3, 0} // popularity
    );
    break;

  case FurnitureType::STOOL_RED:
    furniture = std::make_unique<Furniture>(
        FurnitureType::STOOL_RED,
        2,                                    // type
        35,                                   // q
        17,                                   // supplier
        "Stool Red",                          // name
        180,                                  // price
        std::vector<int>{180, 180, 180, 180}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        87,                                   // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            3, 2, 3, 2, 2, 0} // popularity
    );
    break;

  case FurnitureType::STOOL_BLUE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::STOOL_BLUE,
        2,                                    // type
        35,                                   // q
        17,                                   // supplier
        "Stool Blue",                         // name
        180,                                  // price
        std::vector<int>{181, 181, 181, 181}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        86,                                   // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            2, 2, 2, 2, 3, 0} // popularity
    );
    break;

  case FurnitureType::STOOL_YELLOW:
    furniture = std::make_unique<Furniture>(
        FurnitureType::STOOL_YELLOW,
        2,                                    // type
        35,                                   // q
        17,                                   // supplier
        "Stool Yellow",                       // name
        180,                                  // price
        std::vector<int>{182, 182, 182, 182}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        89,                                   // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            2, 2, 2, 2, 2, 0} // popularity
    );
    break;

  case FurnitureType::STOOL_GREEN:
    furniture = std::make_unique<Furniture>(
        FurnitureType::STOOL_GREEN,
        2,                                    // type
        35,                                   // q
        17,                                   // supplier
        "Stool Green",                        // name
        180,                                  // price
        std::vector<int>{183, 183, 183, 183}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        86,                                   // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            2, 3, 2, 2, 2, 0} // popularity
    );
    break;

  case FurnitureType::STOOL_BLACK:
    furniture = std::make_unique<Furniture>(
        FurnitureType::STOOL_BLACK,
        2,                                    // type
        35,                                   // q
        17,                                   // supplier
        "Stool Black",                        // name
        160,                                  // price
        std::vector<int>{184, 184, 184, 184}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        88,                                   // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            1, 1, 2, 2, 2, 0} // popularity
    );
    break;

  case FurnitureType::STOOL_WHITE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::STOOL_WHITE,
        2,                                    // type
        35,                                   // q
        17,                                   // supplier
        "Stool White",                        // name
        160,                                  // price
        std::vector<int>{185, 185, 185, 185}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        90,                                   // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            1, 1, 2, 2, 2, 0} // popularity
    );
    break;

  case FurnitureType::NOSTALGICO_YELLOW:
    furniture = std::make_unique<Furniture>(
        FurnitureType::NOSTALGICO_YELLOW,
        2,                                    // type
        44,                                   // q
        18,                                   // supplier
        "Nostalgico Yellow",                  // name
        250,                                  // price
        std::vector<int>{186, 187, 188, 189}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        79,                                   // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            1, 1, 1, 3, 2, 4} // popularity
    );
    break;

  case FurnitureType::NOSTALGICO_GREEN:
    furniture = std::make_unique<Furniture>(
        FurnitureType::NOSTALGICO_GREEN,
        2,                                    // type
        44,                                   // q
        18,                                   // supplier
        "Nostalgico Green",                   // name
        250,                                  // price
        std::vector<int>{190, 191, 192, 193}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        80,                                   // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            1, 1, 1, 3, 2, 4} // popularity
    );
    break;

  case FurnitureType::STUDIO_BLUE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::STUDIO_BLUE,
        2,                                    // type
        80,                                   // q
        18,                                   // supplier
        "Studio Blue",                        // name
        1200,                                 // price
        std::vector<int>{194, 195, 196, 197}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            6, 6, 6, 6, 5, 2} // popularity
    );
    break;

  case FurnitureType::STUDIO_RED:
    furniture = std::make_unique<Furniture>(
        FurnitureType::STUDIO_RED,
        2,                                    // type
        80,                                   // q
        18,                                   // supplier
        "Studio Red",                         // name
        1200,                                 // price
        std::vector<int>{198, 199, 200, 201}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            7, 6, 8, 5, 2, 2} // popularity
    );
    break;

  case FurnitureType::STUDIO_YELLOW:
    furniture = std::make_unique<Furniture>(
        FurnitureType::STUDIO_YELLOW,
        2,                                    // type
        80,                                   // q
        18,                                   // supplier
        "Studio Yellow",                      // name
        1200,                                 // price
        std::vector<int>{202, 203, 204, 205}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            6, 6, 6, 5, 5, 2} // popularity
    );
    break;

  case FurnitureType::COMFORT_YELLOW:
    furniture = std::make_unique<Furniture>(
        FurnitureType::COMFORT_YELLOW,
        2,                                    // type
        90,                                   // q
        18,                                   // supplier
        "Comfort Yellow",                     // name
        2000,                                 // price
        std::vector<int>{206, 207, 208, 209}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            6, 6, 6, 7, 8, 5} // popularity
    );
    break;

  case FurnitureType::COMFORT_RED:
    furniture = std::make_unique<Furniture>(
        FurnitureType::COMFORT_RED,
        2,                                    // type
        90,                                   // q
        18,                                   // supplier
        "Comfort Red",                        // name
        2000,                                 // price
        std::vector<int>{210, 211, 212, 213}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            6, 6, 7, 7, 7, 5} // popularity
    );
    break;

  case FurnitureType::MEDITATION_GREEN:
    furniture = std::make_unique<Furniture>(
        FurnitureType::MEDITATION_GREEN,
        2,                                    // type
        55,                                   // q
        19,                                   // supplier
        "Meditation Green",                   // name
        480,                                  // price
        std::vector<int>{214, 214, 214, 214}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            4, 4, 1, 3, 1, -1} // popularity
    );
    break;

  case FurnitureType::MEDITATION_RED:
    furniture = std::make_unique<Furniture>(
        FurnitureType::MEDITATION_RED,
        2,                                    // type
        55,                                   // q
        19,                                   // supplier
        "Meditation Red",                     // name
        480,                                  // price
        std::vector<int>{215, 215, 215, 215}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            4, 4, 1, 3, 1, -1} // popularity
    );
    break;

  case FurnitureType::MEDITATION_BLUE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::MEDITATION_BLUE,
        2,                                    // type
        55,                                   // q
        19,                                   // supplier
        "Meditation Blue",                    // name
        480,                                  // price
        std::vector<int>{216, 216, 216, 216}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            4, 4, 1, 3, 1, -1} // popularity
    );
    break;

  case FurnitureType::BARBECUE_LIGHT:
    furniture = std::make_unique<Furniture>(
        FurnitureType::BARBECUE_LIGHT,
        2,                                    // type
        38,                                   // q
        21,                                   // supplier
        "Barbecue light",                     // name
        250,                                  // price
        std::vector<int>{217, 218, 217, 218}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        74,                                   // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            1, 1, 4, 4, 2, -1} // popularity
    );
    break;

  case FurnitureType::BARBECUE_DARK:
    furniture = std::make_unique<Furniture>(
        FurnitureType::BARBECUE_DARK,
        2,                                    // type
        38,                                   // q
        21,                                   // supplier
        "Barbecue dark",                      // name
        250,                                  // price
        std::vector<int>{219, 220, 219, 220}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        75,                                   // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            1, 1, 4, 4, 2, -1} // popularity
    );
    break;

  case FurnitureType::POP_BLUE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::POP_BLUE,
        2,                                    // type
        38,                                   // q
        19,                                   // supplier
        "Pop Blue",                           // name
        500,                                  // price
        std::vector<int>{221, 222, 223, 224}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        76,                                   // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            4, 4, 4, 4, 2, 1} // popularity
    );
    break;

  case FurnitureType::POP_RED:
    furniture = std::make_unique<Furniture>(
        FurnitureType::POP_RED,
        2,                                    // type
        38,                                   // q
        19,                                   // supplier
        "Pop Red",                            // name
        500,                                  // price
        std::vector<int>{225, 226, 227, 228}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        77,                                   // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            4, 4, 5, 4, 0, 1} // popularity
    );
    break;

  case FurnitureType::POP_BLACK:
    furniture = std::make_unique<Furniture>(
        FurnitureType::POP_BLACK,
        2,                                    // type
        38,                                   // q
        19,                                   // supplier
        "Pop Black",                          // name
        500,                                  // price
        std::vector<int>{229, 230, 231, 232}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        78,                                   // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            2, 3, 4, 4, 2, 1} // popularity
    );
    break;

  case FurnitureType::POP_WHITE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::POP_WHITE,
        2,                                    // type
        38,                                   // q
        19,                                   // supplier
        "Pop White",                          // name
        500,                                  // price
        std::vector<int>{233, 234, 235, 236}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        76,                                   // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            2, 3, 4, 4, 2, 1} // popularity
    );
    break;

  case FurnitureType::MODERN_ART_1:
    furniture = std::make_unique<Furniture>(
        FurnitureType::MODERN_ART_1,
        2,                                    // type
        38,                                   // q
        17,                                   // supplier
        "Modern Art 1",                       // name
        650,                                  // price
        std::vector<int>{237, 237, 237, 237}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        95,                                   // u2
        2,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            5, 5, 4, 1, 0, -1} // popularity
    );
    break;

  case FurnitureType::MODERN_ART_2:
    furniture = std::make_unique<Furniture>(
        FurnitureType::MODERN_ART_2,
        2,                                    // type
        44,                                   // q
        17,                                   // supplier
        "Modern Art 2",                       // name
        700,                                  // price
        std::vector<int>{238, 238, 238, 238}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        96,                                   // u2
        2,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            5, 5, 4, 1, 0, -1} // popularity
    );
    break;

  case FurnitureType::NOSTALGICA_YELLOW:
    furniture = std::make_unique<Furniture>(
        FurnitureType::NOSTALGICA_YELLOW,
        2,                                    // type
        64,                                   // q
        18,                                   // supplier
        "Nostalgica Yellow",                  // name
        2590,                                 // price
        std::vector<int>{239, 239, 239, 239}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        92,                                   // u2
        2,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            5, 5, 5, 5, 5, 5} // popularity
    );
    break;

  case FurnitureType::NOSTALGICA_WHITE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::NOSTALGICA_WHITE,
        2,                                    // type
        64,                                   // q
        18,                                   // supplier
        "Nostalgica White",                   // name
        2390,                                 // price
        std::vector<int>{240, 240, 240, 240}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        91,                                   // u2
        2,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            5, 5, 5, 5, 5, 5} // popularity
    );
    break;

  case FurnitureType::MODERN_ART_3:
    furniture = std::make_unique<Furniture>(
        FurnitureType::MODERN_ART_3,
        2,                                    // type
        90,                                   // q
        18,                                   // supplier
        "Modern Art 3",                       // name
        5900,                                 // price
        std::vector<int>{241, 242, 241, 242}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        250,                                  // u2
        4,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            8, 8, 4, 6, 8, 1} // popularity
    );
    break;

  case FurnitureType::BANGKOK_BLUE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::BANGKOK_BLUE,
        2,                                    // type
        50,                                   // q
        19,                                   // supplier
        "Bangkok Blue",                       // name
        1000,                                 // price
        std::vector<int>{243, 243, 243, 243}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        250,                                  // u2
        2,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            6, 7, 6, 5, 3, -1} // popularity
    );
    break;

  case FurnitureType::BANGKOK_RED:
    furniture = std::make_unique<Furniture>(
        FurnitureType::BANGKOK_RED,
        2,                                    // type
        50,                                   // q
        19,                                   // supplier
        "Bangkok Red",                        // name
        1000,                                 // price
        std::vector<int>{244, 244, 244, 244}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        250,                                  // u2
        2,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            7, 6, 7, 5, 1, -1} // popularity
    );
    break;

  case FurnitureType::BANGKOK_GREEN:
    furniture = std::make_unique<Furniture>(
        FurnitureType::BANGKOK_GREEN,
        2,                                    // type
        50,                                   // q
        19,                                   // supplier
        "Bangkok Green",                      // name
        1000,                                 // price
        std::vector<int>{245, 245, 245, 245}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        250,                                  // u2
        2,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            6, 7, 6, 6, 3, -1} // popularity
    );
    break;

  case FurnitureType::VELVET_RED:
    furniture = std::make_unique<Furniture>(
        FurnitureType::VELVET_RED,
        2,                                    // type
        70,                                   // q
        20,                                   // supplier
        "Velvet Red",                         // name
        300,                                  // price
        std::vector<int>{246, 247, 248, 249}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        1,                                    // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            6, 2, 2, 2, 1, 2} // popularity
    );
    break;

  case FurnitureType::VELVET_YELLOW:
    furniture = std::make_unique<Furniture>(
        FurnitureType::VELVET_YELLOW,
        2,                                    // type
        70,                                   // q
        20,                                   // supplier
        "Velvet Yellow",                      // name
        280,                                  // price
        std::vector<int>{250, 251, 252, 253}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        1,                                    // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            5, 2, 2, 2, 2, 2} // popularity
    );
    break;

  case FurnitureType::VELVET_BLUE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::VELVET_BLUE,
        2,                                    // type
        70,                                   // q
        20,                                   // supplier
        "Velvet Blue",                        // name
        260,                                  // price
        std::vector<int>{254, 255, 256, 257}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        1,                                    // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            3, 2, 2, 2, 2, 2} // popularity
    );
    break;

  case FurnitureType::BASKET_CHAIR:
    furniture = std::make_unique<Furniture>(
        FurnitureType::BASKET_CHAIR,
        2,                                    // type
        66,                                   // q
        21,                                   // supplier
        "Basket Chair",                       // name
        1000,                                 // price
        std::vector<int>{258, 259, 260, 261}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        1,                                    // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            3, 3, 4, 7, 7, 6} // popularity
    );
    break;

  case FurnitureType::LEATHER_RED:
    furniture = std::make_unique<Furniture>(
        FurnitureType::LEATHER_RED,
        2,                                    // type
        78,                                   // q
        22,                                   // supplier
        "Leather Red",                        // name
        900,                                  // price
        std::vector<int>{262, 263, 264, 265}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        107,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            4, 4, 4, 4, 4, 4} // popularity
    );
    break;

  case FurnitureType::LEATHER_BLUE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::LEATHER_BLUE,
        2,                                    // type
        78,                                   // q
        22,                                   // supplier
        "Leather Blue",                       // name
        900,                                  // price
        std::vector<int>{266, 267, 268, 269}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        106,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            4, 4, 5, 4, 2, 4} // popularity
    );
    break;

  case FurnitureType::BUSINESS_WHITE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::BUSINESS_WHITE,
        2,                                    // type
        60,                                   // q
        23,                                   // supplier
        "Business White",                     // name
        1200,                                 // price
        std::vector<int>{270, 271, 272, 273}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        29,                                   // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            5, 5, 5, 6, 8, 5} // popularity
    );
    break;

  case FurnitureType::DALLAS:
    furniture = std::make_unique<Furniture>(
        FurnitureType::DALLAS,
        2,                                    // type
        65,                                   // q
        23,                                   // supplier
        "Dallas",                             // name
        900,                                  // price
        std::vector<int>{274, 275, 276, 277}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        108,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            6, 6, 6, 6, 7, 4} // popularity
    );
    break;

  case FurnitureType::BILLIARD_RED:
    furniture = std::make_unique<Furniture>(
        FurnitureType::BILLIARD_RED,
        2,                                    // type
        80,                                   // q
        22,                                   // supplier
        "Billiard Red",                       // name
        1400,                                 // price
        std::vector<int>{278, 279, 280, 281}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            6, 5, 6, 5, 4, 5} // popularity
    );
    break;

  case FurnitureType::BILLIARD_GREEN:
    furniture = std::make_unique<Furniture>(
        FurnitureType::BILLIARD_GREEN,
        2,                                    // type
        80,                                   // q
        22,                                   // supplier
        "Billiard Green",                     // name
        1500,                                 // price
        std::vector<int>{282, 283, 284, 285}, // bobs
        static_cast<FurnitureCategory>(2),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(1),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            5, 6, 5, 5, 5, 5} // popularity
    );
    break;

  case FurnitureType::MARBLE_ROUND:
    furniture = std::make_unique<Furniture>(
        FurnitureType::MARBLE_ROUND,
        2,                                    // type
        90,                                   // q
        24,                                   // supplier
        "Marble Round",                       // name
        8800,                                 // price
        std::vector<int>{286, 286, 286, 286}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        24,                                   // u2
        2,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            9, 9, 9, 14, 15, 14} // popularity
    );
    break;

  case FurnitureType::MARBLE_DARK:
    furniture = std::make_unique<Furniture>(
        FurnitureType::MARBLE_DARK,
        2,                                    // type
        90,                                   // q
        22,                                   // supplier
        "Marble Dark",                        // name
        5500,                                 // price
        std::vector<int>{287, 287, 287, 287}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        116,                                  // u2
        4,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            7, 7, 7, 7, 9, 9} // popularity
    );
    break;

  case FurnitureType::MARBLE_LIGHT:
    furniture = std::make_unique<Furniture>(
        FurnitureType::MARBLE_LIGHT,
        2,                                    // type
        90,                                   // q
        22,                                   // supplier
        "Marble Light",                       // name
        6500,                                 // price
        std::vector<int>{288, 288, 288, 288}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        24,                                   // u2
        4,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            8, 8, 7, 7, 9, 8} // popularity
    );
    break;

  case FurnitureType::MINI_TABLE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::MINI_TABLE,
        2,                                    // type
        80,                                   // q
        20,                                   // supplier
        "Mini Table",                         // name
        1200,                                 // price
        std::vector<int>{289, 289, 289, 289}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            6, 3, 3, 3, 3, 0} // popularity
    );
    break;

  case FurnitureType::NEW_YORK:
    furniture = std::make_unique<Furniture>(
        FurnitureType::NEW_YORK,
        2,                                    // type
        99,                                   // q
        23,                                   // supplier
        "New York",                           // name
        9900,                                 // price
        std::vector<int>{290, 290, 290, 290}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        250,                                  // u2
        4,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            7, 8, 6, 8, 24, 6} // popularity
    );
    break;

  case FurnitureType::TRANSPARENT:
    furniture = std::make_unique<Furniture>(
        FurnitureType::TRANSPARENT,
        2,                                    // type
        89,                                   // q
        23,                                   // supplier
        "Transparent",                        // name
        7800,                                 // price
        std::vector<int>{291, 292, 291, 292}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        250,                                  // u2
        4,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            7, 7, 5, 7, 15, 4} // popularity
    );
    break;

  case FurnitureType::OCTOPUS:
    furniture = std::make_unique<Furniture>(
        FurnitureType::OCTOPUS,
        2,                                    // type
        72,                                   // q
        23,                                   // supplier
        "Octopus",                            // name
        4900,                                 // price
        std::vector<int>{293, 294, 293, 294}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        113,                                  // u2
        4,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            7, 7, 7, 7, 13, 5} // popularity
    );
    break;

  case FurnitureType::ATHENS:
    furniture = std::make_unique<Furniture>(
        FurnitureType::ATHENS,
        2,                                    // type
        95,                                   // q
        13,                                   // supplier
        "Athens",                             // name
        6500,                                 // price
        std::vector<int>{295, 296, 295, 296}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        122,                                  // u2
        4,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            5, 5, 8, 8, 9, 9} // popularity
    );
    break;

  case FurnitureType::ROME:
    furniture = std::make_unique<Furniture>(
        FurnitureType::ROME,
        2,                                    // type
        95,                                   // q
        13,                                   // supplier
        "Rome",                               // name
        7500,                                 // price
        std::vector<int>{297, 298, 297, 298}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        23,                                   // u2
        4,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            8, 8, 8, 8, 9, 9} // popularity
    );
    break;

  case FurnitureType::LONDON:
    furniture = std::make_unique<Furniture>(
        FurnitureType::LONDON,
        2,                                    // type
        82,                                   // q
        18,                                   // supplier
        "London",                             // name
        4900,                                 // price
        std::vector<int>{299, 300, 299, 300}, // bobs
        static_cast<FurnitureCategory>(1),    // category
        94,                                   // u2
        4,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            5, 7, 6, 7, 11, 2} // popularity
    );
    break;

  case FurnitureType::DECORATIVE_WALL:
    furniture = std::make_unique<Furniture>(
        FurnitureType::DECORATIVE_WALL,
        4,                                    // type
        58,                                   // q
        26,                                   // supplier
        "Decorative Wall",                    // name
        1000,                                 // price
        std::vector<int>{301, 302, 301, 302}, // bobs
        static_cast<FurnitureCategory>(3),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            5, 5, 7, 7, 8, 8} // popularity
    );
    break;

  case FurnitureType::TABLE_PLANT_1:
    furniture = std::make_unique<Furniture>(
        FurnitureType::TABLE_PLANT_1,
        4,                                    // type
        40,                                   // q
        27,                                   // supplier
        "Table Plant 1",                      // name
        40,                                   // price
        std::vector<int>{303, 303, 303, 303}, // bobs
        static_cast<FurnitureCategory>(3),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(3),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            1, 1, 2, 2, 3, 3} // popularity
    );
    break;

  case FurnitureType::TABLE_PLANT_2:
    furniture = std::make_unique<Furniture>(
        FurnitureType::TABLE_PLANT_2,
        4,                                    // type
        36,                                   // q
        27,                                   // supplier
        "Table Plant 2",                      // name
        20,                                   // price
        std::vector<int>{304, 304, 304, 304}, // bobs
        static_cast<FurnitureCategory>(3),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(3),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            1, 1, 1, 2, 2, 2} // popularity
    );
    break;

  case FurnitureType::FLOOR_PLANT_1:
    furniture = std::make_unique<Furniture>(
        FurnitureType::FLOOR_PLANT_1,
        4,                                    // type
        45,                                   // q
        27,                                   // supplier
        "Floor Plant 1",                      // name
        100,                                  // price
        std::vector<int>{305, 305, 305, 305}, // bobs
        static_cast<FurnitureCategory>(3),    // category
        123,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            2, 2, 2, 2, 2, 2} // popularity
    );
    break;

  case FurnitureType::FLOOR_PLANT_2:
    furniture = std::make_unique<Furniture>(
        FurnitureType::FLOOR_PLANT_2,
        4,                                    // type
        55,                                   // q
        27,                                   // supplier
        "Floor Plant 2",                      // name
        150,                                  // price
        std::vector<int>{306, 306, 306, 306}, // bobs
        static_cast<FurnitureCategory>(3),    // category
        121,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            2, 2, 2, 3, 3, 3} // popularity
    );
    break;

  case FurnitureType::FLOOR_PLANT_3:
    furniture = std::make_unique<Furniture>(
        FurnitureType::FLOOR_PLANT_3,
        4,                                    // type
        65,                                   // q
        27,                                   // supplier
        "Floor Plant 3",                      // name
        200,                                  // price
        std::vector<int>{307, 307, 307, 307}, // bobs
        static_cast<FurnitureCategory>(3),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            3, 3, 4, 4, 4, 5} // popularity
    );
    break;

  case FurnitureType::PLACE_INVADERS:
    furniture = std::make_unique<Furniture>(
        FurnitureType::PLACE_INVADERS,
        4,                                    // type
        25,                                   // q
        28,                                   // supplier
        "Place Invaders",                     // name
        2000,                                 // price
        std::vector<int>{308, 309, 310, 311}, // bobs
        static_cast<FurnitureCategory>(5),    // category
        125,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            10, 14, 16, 2, -1, -4} // popularity
    );
    break;

  case FurnitureType::DARTH_INVADERS:
    furniture = std::make_unique<Furniture>(
        FurnitureType::DARTH_INVADERS,
        4,                                    // type
        35,                                   // q
        28,                                   // supplier
        "Darth Invaders",                     // name
        2500,                                 // price
        std::vector<int>{308, 309, 310, 311}, // bobs
        static_cast<FurnitureCategory>(5),    // category
        127,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            10, 9, 6, 2, -1, -4} // popularity
    );
    break;

  case FurnitureType::T_TRIS_VERSION_598:
    furniture = std::make_unique<Furniture>(
        FurnitureType::T_TRIS_VERSION_598,
        4,                                    // type
        50,                                   // q
        28,                                   // supplier
        "T-Tris Version 598",                 // name
        3000,                                 // price
        std::vector<int>{308, 309, 310, 311}, // bobs
        static_cast<FurnitureCategory>(5),    // category
        131,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            2, 4, 6, 12, 14, 8} // popularity
    );
    break;

  case FurnitureType::ZURRICANE_8:
    furniture = std::make_unique<Furniture>(
        FurnitureType::ZURRICANE_8,
        4,                                    // type
        45,                                   // q
        28,                                   // supplier
        "Zurricane 8",                        // name
        3500,                                 // price
        std::vector<int>{308, 309, 310, 311}, // bobs
        static_cast<FurnitureCategory>(5),    // category
        130,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            12, 11, 20, 0, -1, -4} // popularity
    );
    break;

  case FurnitureType::MAD_PIZZA:
    furniture = std::make_unique<Furniture>(
        FurnitureType::MAD_PIZZA,
        4,                                    // type
        10,                                   // q
        29,                                   // supplier
        "Mad Pizza",                          // name
        4000,                                 // price
        std::vector<int>{308, 309, 310, 311}, // bobs
        static_cast<FurnitureCategory>(5),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            -9, -9, -9, -9, -9, -9} // popularity
    );
    break;

  case FurnitureType::PIZZA_TYCOON:
    furniture = std::make_unique<Furniture>(
        FurnitureType::PIZZA_TYCOON,
        4,                                    // type
        65,                                   // q
        28,                                   // supplier
        "Pizza Tycoon",                       // name
        5000,                                 // price
        std::vector<int>{308, 309, 310, 311}, // bobs
        static_cast<FurnitureCategory>(5),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            25, 20, 10, 10, 5, 5} // popularity
    );
    break;

  case FurnitureType::PINBALL_NIGHTMARE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::PINBALL_NIGHTMARE,
        4,                                    // type
        85,                                   // q
        29,                                   // supplier
        "Pinball Nightmare",                  // name
        6000,                                 // price
        std::vector<int>{308, 309, 310, 311}, // bobs
        static_cast<FurnitureCategory>(5),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            14, 13, 6, 2, -1, -2} // popularity
    );
    break;

  case FurnitureType::IMMORTAL_KOMBAT:
    furniture = std::make_unique<Furniture>(
        FurnitureType::IMMORTAL_KOMBAT,
        4,                                    // type
        95,                                   // q
        29,                                   // supplier
        "Immortal Kombat",                    // name
        8000,                                 // price
        std::vector<int>{308, 309, 310, 311}, // bobs
        static_cast<FurnitureCategory>(5),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            5, 9, 20, 20, 30, 10} // popularity
    );
    break;

  case FurnitureType::PIZZA_TYCOON_2:
    furniture = std::make_unique<Furniture>(
        FurnitureType::PIZZA_TYCOON_2,
        4,                                    // type
        99,                                   // q
        29,                                   // supplier
        "Pizza Tycoon 2",                     // name
        9000,                                 // price
        std::vector<int>{308, 309, 310, 311}, // bobs
        static_cast<FurnitureCategory>(5),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            9, 9, 9, 9, 1, -2} // popularity
    );
    break;

  case FurnitureType::COLONIZATION:
    furniture = std::make_unique<Furniture>(
        FurnitureType::COLONIZATION,
        4,                                    // type
        10,                                   // q
        29,                                   // supplier
        "Colonization",                       // name
        9500,                                 // price
        std::vector<int>{308, 309, 310, 311}, // bobs
        static_cast<FurnitureCategory>(5),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            5, 9, 18, 16, 6, -1} // popularity
    );
    break;

  case FurnitureType::SPACE_DREAM:
    furniture = std::make_unique<Furniture>(
        FurnitureType::SPACE_DREAM,
        4,                                    // type
        25,                                   // q
        28,                                   // supplier
        "Space Dream",                        // name
        2000,                                 // price
        std::vector<int>{312, 313, 314, 315}, // bobs
        static_cast<FurnitureCategory>(5),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            5, 8, 8, 2, -1, -2} // popularity
    );
    break;

  case FurnitureType::FINAL_FIGHT:
    furniture = std::make_unique<Furniture>(
        FurnitureType::FINAL_FIGHT,
        4,                                    // type
        35,                                   // q
        28,                                   // supplier
        "Final Fight",                        // name
        2500,                                 // price
        std::vector<int>{312, 313, 314, 315}, // bobs
        static_cast<FurnitureCategory>(5),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            5, 11, 10, 2, -1, -2} // popularity
    );
    break;

  case FurnitureType::CYBER_DEATH:
    furniture = std::make_unique<Furniture>(
        FurnitureType::CYBER_DEATH,
        4,                                    // type
        45,                                   // q
        28,                                   // supplier
        "Cyber Death",                        // name
        3000,                                 // price
        std::vector<int>{312, 313, 314, 315}, // bobs
        static_cast<FurnitureCategory>(5),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            5, 8, 8, 2, -1, -2} // popularity
    );
    break;

  case FurnitureType::MACHO_MAN:
    furniture = std::make_unique<Furniture>(
        FurnitureType::MACHO_MAN,
        4,                                    // type
        55,                                   // q
        29,                                   // supplier
        "Macho Man",                          // name
        3500,                                 // price
        std::vector<int>{312, 313, 314, 315}, // bobs
        static_cast<FurnitureCategory>(5),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            5, 11, 10, 2, -1, -2} // popularity
    );
    break;

  case FurnitureType::JUKEBOX:
    furniture = std::make_unique<Furniture>(
        FurnitureType::JUKEBOX,
        4,                                    // type
        60,                                   // q
        30,                                   // supplier
        "Jukebox",                            // name
        5000,                                 // price
        std::vector<int>{316, 317, 318, 319}, // bobs
        static_cast<FurnitureCategory>(4),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            6, 6, 6, 5, 3, 0} // popularity
    );
    break;

  case FurnitureType::TV_AERIAL:
    furniture = std::make_unique<Furniture>(
        FurnitureType::TV_AERIAL,
        4,                                    // type
        25,                                   // q
        30,                                   // supplier
        "TV (Aerial)",                        // name
        2000,                                 // price
        std::vector<int>{320, 321, 322, 323}, // bobs
        static_cast<FurnitureCategory>(4),    // category
        140,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(3),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            5, 5, 5, 5, 5, 5} // popularity
    );
    break;

  case FurnitureType::TV_CABLE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::TV_CABLE,
        4,                                    // type
        35,                                   // q
        30,                                   // supplier
        "TV (Cable)",                         // name
        3000,                                 // price
        std::vector<int>{320, 321, 322, 323}, // bobs
        static_cast<FurnitureCategory>(4),    // category
        141,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(3),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            8, 8, 8, 8, 8, 8} // popularity
    );
    break;

  case FurnitureType::TV_SATELLITE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::TV_SATELLITE,
        4,                                    // type
        25,                                   // q
        30,                                   // supplier
        "TV (Satellite)",                     // name
        4000,                                 // price
        std::vector<int>{320, 321, 322, 323}, // bobs
        static_cast<FurnitureCategory>(4),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(3),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            10, 10, 10, 10, 10, 10} // popularity
    );
    break;

  case FurnitureType::CIGARETTE_MACHINE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::CIGARETTE_MACHINE,
        4,                                    // type
        50,                                   // q
        31,                                   // supplier
        "Cigarette Machine",                  // name
        2500,                                 // price
        std::vector<int>{332, 333, 334, 335}, // bobs
        static_cast<FurnitureCategory>(6),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            0, 2, 8, 2, 4, 1} // popularity
    );
    break;

  case FurnitureType::CHOCOLATE_MACHINE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::CHOCOLATE_MACHINE,
        4,                                    // type
        60,                                   // q
        31,                                   // supplier
        "Chocolate Machine",                  // name
        1200,                                 // price
        std::vector<int>{328, 329, 330, 331}, // bobs
        static_cast<FurnitureCategory>(6),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            0, 11, 10, 2, 4, -3} // popularity
    );
    break;

  case FurnitureType::CHEWING_GUM_MACH:
    furniture = std::make_unique<Furniture>(
        FurnitureType::CHEWING_GUM_MACH,
        4,                                 // type
        60,                                // q
        99,                                // supplier
        "Chewing Gum Mach.",               // name
        900,                               // price
        std::vector<int>{58, 58, 58, 58},  // bobs
        static_cast<FurnitureCategory>(6), // category
        250,                               // u2
        1,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(3),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            10, 2, 0, 0, 0, 0} // popularity
    );
    break;

  case FurnitureType::TELEPHONE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::TELEPHONE,
        4,                                    // type
        60,                                   // q
        31,                                   // supplier
        "Telephone",                          // name
        3500,                                 // price
        std::vector<int>{336, 337, 338, 339}, // bobs
        static_cast<FurnitureCategory>(6),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            0, 2, 2, 2, 10, 2} // popularity
    );
    break;

  case FurnitureType::CANDLE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::CANDLE,
        4,                                 // type
        60,                                // q
        26,                                // supplier
        "Candle",                          // name
        10,                                // price
        std::vector<int>{61, 61, 61, 61},  // bobs
        static_cast<FurnitureCategory>(3), // category
        250,                               // u2
        1,                                 // capacity
        0,                                 // oven_bake_time
        static_cast<PlacementLayer>(3),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            1, -1, 0, 2, 3, 2} // popularity
    );
    break;

  case FurnitureType::ASHES_LARGE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::ASHES_LARGE,
        4,                                    // type
        0,                                    // q
        99,                                   // supplier
        "Ashes",                              // name
        0,                                    // price
        std::vector<int>{326, 326, 326, 326}, // bobs
        static_cast<FurnitureCategory>(0),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            -9, -9, -9, -9, -9, -9} // popularity
    );
    break;

  case FurnitureType::ASHES_SMALL:
    furniture = std::make_unique<Furniture>(
        FurnitureType::ASHES_SMALL,
        4,                                    // type
        0,                                    // q
        99,                                   // supplier
        "Ashes",                              // name
        0,                                    // price
        std::vector<int>{327, 327, 327, 327}, // bobs
        static_cast<FurnitureCategory>(0),    // category
        250,                                  // u2
        1,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            -9, -9, -9, -9, -9, -9} // popularity
    );
    break;

  case FurnitureType::GRILL_SMALL:
    furniture = std::make_unique<Furniture>(
        FurnitureType::GRILL_SMALL,
        3,                                 // type
        20,                                // q
        32,                                // supplier
        "Grill small",                     // name
        800,                               // price
        std::vector<int>{0, 0, 0, 0},      // bobs
        static_cast<FurnitureCategory>(8), // category
        8,                                 // u2
        2,                                 // capacity
        15,                                // oven_bake_time
        static_cast<PlacementLayer>(2),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            0, 0, 0, 0, 0, 0} // popularity
    );
    break;

  case FurnitureType::OVEN_SMALL:
    furniture = std::make_unique<Furniture>(
        FurnitureType::OVEN_SMALL,
        3,                                 // type
        38,                                // q
        32,                                // supplier
        "Oven small",                      // name
        1500,                              // price
        std::vector<int>{1, 1, 1, 1},      // bobs
        static_cast<FurnitureCategory>(8), // category
        8,                                 // u2
        2,                                 // capacity
        14,                                // oven_bake_time
        static_cast<PlacementLayer>(2),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            0, 0, 0, 0, 0, 0} // popularity
    );
    break;

  case FurnitureType::STONE_OVEN_SMALL:
    furniture = std::make_unique<Furniture>(
        FurnitureType::STONE_OVEN_SMALL,
        3,                                 // type
        58,                                // q
        33,                                // supplier
        "Stone Oven small",                // name
        3000,                              // price
        std::vector<int>{2, 2, 2, 2},      // bobs
        static_cast<FurnitureCategory>(8), // category
        8,                                 // u2
        2,                                 // capacity
        12,                                // oven_bake_time
        static_cast<PlacementLayer>(2),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            0, 0, 0, 0, 0, 0} // popularity
    );
    break;

  case FurnitureType::EXCLUSIVE_OVEN_1:
    furniture = std::make_unique<Furniture>(
        FurnitureType::EXCLUSIVE_OVEN_1,
        3,                                 // type
        85,                                // q
        33,                                // supplier
        "Exclusive Oven 1",                // name
        7000,                              // price
        std::vector<int>{8, 8, 8, 8},      // bobs
        static_cast<FurnitureCategory>(8), // category
        8,                                 // u2
        2,                                 // capacity
        11,                                // oven_bake_time
        static_cast<PlacementLayer>(2),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            0, 0, 0, 0, 0, 0} // popularity
    );
    break;

  case FurnitureType::ELECTRIC_SMALL:
    furniture = std::make_unique<Furniture>(
        FurnitureType::ELECTRIC_SMALL,
        3,                                 // type
        70,                                // q
        34,                                // supplier
        "Electric small",                  // name
        8000,                              // price
        std::vector<int>{3, 3, 3, 3},      // bobs
        static_cast<FurnitureCategory>(8), // category
        8,                                 // u2
        3,                                 // capacity
        8,                                 // oven_bake_time
        static_cast<PlacementLayer>(2),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            0, 0, 0, 0, 0, 0} // popularity
    );
    break;

  case FurnitureType::GRILL_LARGE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::GRILL_LARGE,
        3,                                 // type
        20,                                // q
        32,                                // supplier
        "Grill large",                     // name
        1500,                              // price
        std::vector<int>{4, 4, 4, 4},      // bobs
        static_cast<FurnitureCategory>(8), // category
        8,                                 // u2
        4,                                 // capacity
        15,                                // oven_bake_time
        static_cast<PlacementLayer>(2),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            0, 0, 0, 0, 0, 0} // popularity
    );
    break;

  case FurnitureType::OVEN_LARGE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::OVEN_LARGE,
        3,                                 // type
        38,                                // q
        32,                                // supplier
        "Oven large",                      // name
        2800,                              // price
        std::vector<int>{5, 5, 5, 5},      // bobs
        static_cast<FurnitureCategory>(8), // category
        8,                                 // u2
        4,                                 // capacity
        14,                                // oven_bake_time
        static_cast<PlacementLayer>(2),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            0, 0, 0, 0, 0, 0} // popularity
    );
    break;

  case FurnitureType::STONE_OVEN_LARGE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::STONE_OVEN_LARGE,
        3,                                 // type
        58,                                // q
        33,                                // supplier
        "Stone Oven large",                // name
        5600,                              // price
        std::vector<int>{6, 6, 6, 6},      // bobs
        static_cast<FurnitureCategory>(8), // category
        8,                                 // u2
        4,                                 // capacity
        12,                                // oven_bake_time
        static_cast<PlacementLayer>(2),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            0, 0, 0, 0, 0, 0} // popularity
    );
    break;

  case FurnitureType::EXCLUSIVE_OVEN_2:
    furniture = std::make_unique<Furniture>(
        FurnitureType::EXCLUSIVE_OVEN_2,
        3,                                 // type
        85,                                // q
        33,                                // supplier
        "Exclusive Oven 2",                // name
        12000,                             // price
        std::vector<int>{9, 9, 9, 9},      // bobs
        static_cast<FurnitureCategory>(8), // category
        8,                                 // u2
        4,                                 // capacity
        11,                                // oven_bake_time
        static_cast<PlacementLayer>(2),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            0, 0, 0, 0, 0, 0} // popularity
    );
    break;

  case FurnitureType::ELECTRIC_LARGE:
    furniture = std::make_unique<Furniture>(
        FurnitureType::ELECTRIC_LARGE,
        3,                                 // type
        70,                                // q
        34,                                // supplier
        "Electric large",                  // name
        15000,                             // price
        std::vector<int>{7, 7, 7, 7},      // bobs
        static_cast<FurnitureCategory>(8), // category
        8,                                 // u2
        6,                                 // capacity
        8,                                 // oven_bake_time
        static_cast<PlacementLayer>(2),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            0, 0, 0, 0, 0, 0} // popularity
    );
    break;

  case FurnitureType::HYPER_OVEN:
    furniture = std::make_unique<Furniture>(
        FurnitureType::HYPER_OVEN,
        3,                                 // type
        60,                                // q
        35,                                // supplier
        "Hyper Oven",                      // name
        25000,                             // price
        std::vector<int>{12, 12, 12, 12},  // bobs
        static_cast<FurnitureCategory>(8), // category
        8,                                 // u2
        5,                                 // capacity
        5,                                 // oven_bake_time
        static_cast<PlacementLayer>(2),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            0, 0, 0, 0, 0, 0} // popularity
    );
    break;

  case FurnitureType::BLACK_BOX:
    furniture = std::make_unique<Furniture>(
        FurnitureType::BLACK_BOX,
        3,                                 // type
        60,                                // q
        35,                                // supplier
        "Black Box",                       // name
        35000,                             // price
        std::vector<int>{11, 11, 11, 11},  // bobs
        static_cast<FurnitureCategory>(8), // category
        8,                                 // u2
        6,                                 // capacity
        4,                                 // oven_bake_time
        static_cast<PlacementLayer>(2),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            0, 0, 0, 0, 0, 0} // popularity
    );
    break;

  case FurnitureType::SUPER_TOASTER:
    furniture = std::make_unique<Furniture>(
        FurnitureType::SUPER_TOASTER,
        3,                                 // type
        60,                                // q
        35,                                // supplier
        "Super Toaster",                   // name
        50000,                             // price
        std::vector<int>{10, 10, 10, 10},  // bobs
        static_cast<FurnitureCategory>(8), // category
        8,                                 // u2
        8,                                 // capacity
        3,                                 // oven_bake_time
        static_cast<PlacementLayer>(2),    // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            0, 0, 0, 0, 0, 0} // popularity
    );
    break;

  case FurnitureType::ASHES_KITCHEN:
    furniture = std::make_unique<Furniture>(
        FurnitureType::ASHES_KITCHEN,
        3,                                    // type
        0,                                    // q
        99,                                   // supplier
        "Ashes",                              // name
        0,                                    // price
        std::vector<int>{326, 326, 326, 326}, // bobs
        static_cast<FurnitureCategory>(9),    // category
        2,                                    // u2
        0,                                    // capacity
        0,                                    // oven_bake_time
        static_cast<PlacementLayer>(2),       // placement_layer
        std::array<int, static_cast<std::size_t>(ConsumerType::Count)>{
            -9, -9, -9, -9, -9, -9} // popularity
    );
    break;
  }
  const auto* furniture_ptr = furniture.get();
  cache_[furniture_type] = std::move(furniture);
  return furniture_ptr;
}
