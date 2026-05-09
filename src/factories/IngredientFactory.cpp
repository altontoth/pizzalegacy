/*
 * SPDX-FileCopyrightText: 2013 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "factories/IngredientFactory.h"
#include <memory>
#include "core/food/Ingredient.h"
#include "engine/Fatal.h"
#include "engine/Logger.h"

using namespace core::food;

std::unordered_map<int, std::unique_ptr<Ingredient>>
    IngredientFactory::cache_{};

Ingredient* IngredientFactory::create_ingredient(int ingredient_id) {
  std::unique_ptr<Ingredient> i;

  // Check if the ingredient is already in the cache
  auto it = cache_.find(ingredient_id);
  if (it != cache_.end()) {
    return it->second.get();
  }
  auto ingredient_type = static_cast<IngredientType>(ingredient_id);

  // clang-format off
  switch (ingredient_type) {
  case IngredientType::TOMATOES: {
    // The size of the bobs is expressed as x + y + z
    // where x = number of rotations at largest size, then medium size then
    // smallest size. So tomato biggest size is round so rotating doesn't do
    // anything, then 4 rotations for the half slice, and 1 rotation for the
    // small blob of tomato again.
    i = std::make_unique<Ingredient>(IngredientType::TOMATOES, to_string(ingredient_type),
                                     20, 10, 5, 19, 3, 1, 0, 2, 2,
                                     IngredientCategory::VEGETABLES,
                                     std::vector<int>{
                                       0,0,0,0,
                                       1,2,3,4,
                                       5,5,5,5,
                                     }
                                   );
    break;
  }
  case IngredientType::OLIVES: {
    i = std::make_unique<Ingredient>(IngredientType::OLIVES, to_string(ingredient_type),
                                     5, 5, 5, 34, 4, 2, 0, 9, 4,
                                     IngredientCategory::VEGETABLES, std::vector<int>{
                                      6, 7, 6, 7,
                                      6, 7, 6, 7,
                                      6, 7, 6, 7,
                                    });
    break;
  }
  case IngredientType::PEPPERS: {
    i = std::make_unique<Ingredient>(IngredientType::PEPPERS, to_string(ingredient_type),
                                     20, 10, 5, 28, 3, 3, 0, 5, 3,
                                     IngredientCategory::VEGETABLES, std::vector<int>{
                                      8,8,8,8,
                                      9,10,9,10,
                                      11,11,11,11,
                                    });
    break;
  }
  case IngredientType::ASPARAGUS: {
    i = std::make_unique<Ingredient>(IngredientType::ASPARAGUS, to_string(ingredient_type),
                                     60, 20, 5, 20, 2, 2, 0, 3, 2,
                                     IngredientCategory::VEGETABLES, std::vector<int>{
                                      12,15,12,15,
                                      16,17,18,19,
                                      20,20,20,20,
                                    });
    break;
  }
  case IngredientType::COURGETTES: {
    i = std::make_unique<Ingredient>(IngredientType::COURGETTES, to_string(ingredient_type),
                                     20, 10, 5, 28, 2, 2, 0, 4, 2,
                                     IngredientCategory::VEGETABLES, std::vector<int>{
                                      21,21,21,21,
                                      22,23,24,25,
                                      26,26,26,26,
                                    });
    break;
  }
  case IngredientType::CARROTS: {
    i = std::make_unique<Ingredient>(IngredientType::CARROTS, to_string(ingredient_type),
                                     40, 20, 5, 120, 2, 1, 1, 2, 1,
                                     IngredientCategory::VEGETABLES, std::vector<int>{
                                      27,28,29,30,
                                      31,31,31,31,
                                      32,32,32,32,
                                     });
    break;
  }
  case IngredientType::CUCUMBERS: {
    i = std::make_unique<Ingredient>(IngredientType::CUCUMBERS, to_string(ingredient_type),
                                     70, 20, 10, 10, 5, 0, 1, 1, 1,
                                     IngredientCategory::VEGETABLES, std::vector<int>{
                                      33,34,35,36,
                                      37,37,37,37,
                                      38,38,38,38,
                                    });
    break;
  }
  case IngredientType::RADISHES: {
    i = std::make_unique<Ingredient>(IngredientType::RADISHES, to_string(ingredient_type),
                                     10, 5, 8, 90, 5, 0, 0, 2, 1,
                                     IngredientCategory::VEGETABLES, std::vector<int>{
                                      39,40,41,42,
                                      43,43,43,43,
                                      44,44,44,44,
                                    });
    break;
  }
  case IngredientType::PEAS: {
    i = std::make_unique<Ingredient>(IngredientType::PEAS, to_string(ingredient_type),
                                     15, 10, 5, 93, 2, 2, 0, 2, 2,
                                     IngredientCategory::VEGETABLES, std::vector<int>{
                                      45,46,45,46,
                                      47,47,47,47,
                                      48,48,48,48,
                                    });
    break;
  }
  case IngredientType::BROCCOLI: {
    i = std::make_unique<Ingredient>(IngredientType::BROCCOLI, to_string(ingredient_type),
                                     10, 5, 3, 33, 2, 1, 0, 1, 1,
                                     IngredientCategory::VEGETABLES, std::vector<int>{
                                      49,50,51,52,
                                      53,53,53,53,
                                      54,54,54,54,
                                    });
    break;
  }
  case IngredientType::POTATOES: {
    i = std::make_unique<Ingredient>(IngredientType::POTATOES, to_string(ingredient_type),
                                     30, 10, 5, 85, 2, 3, 0, 3, 1,
                                     IngredientCategory::VEGETABLES, std::vector<int>{
                                      55,56,55,56,
                                      57,58,57,58,
                                      59,59,59,59,
                                    });
    break;
  }
  case IngredientType::CHILLIES: {
    i = std::make_unique<Ingredient>(IngredientType::CHILLIES, to_string(ingredient_type),
                                     30, 5, 2, 32, 5, 3, 0, 5, 7,
                                     IngredientCategory::VEGETABLES, std::vector<int>{
                                      60,61,62,63,
                                      64,64,64,64,
                                      65,65,65,65,
                                    });
    break;
  }
  case IngredientType::SPINACH: {
    i = std::make_unique<Ingredient>(IngredientType::SPINACH, to_string(ingredient_type),
                                     10, 5, 5, 34, 4, 2, 1, 4, 1,
                                     IngredientCategory::VEGETABLES, std::vector<int>{
                                      66,67,68,69,
                                      70,70,70,70,
                                      71,71,71,71,
                                    });
    break;
  }
  case IngredientType::SALAD: {
    i = std::make_unique<Ingredient>(IngredientType::SALAD, to_string(ingredient_type),
                                     6, 3, 1, 15, 2, 0, 0, 0, 1,
                                     IngredientCategory::VEGETABLES, std::vector<int>{
                                      72,73,74,75,
                                      76,76,76,76,
                                      77,77,77,77,
                                    });
    break;
  }
  case IngredientType::LEEKS: {
    i = std::make_unique<Ingredient>(IngredientType::LEEKS, to_string(ingredient_type),
                                     40, 10, 5, 22, 5, 1, 0, 1, 1,
                                     IngredientCategory::VEGETABLES, std::vector<int>{
                                      78,79,80,81,
                                      82,82,82,82,
                                      83,83,83,83,
                                    });

    break;
  }
  case IngredientType::ONIONS: {
    i = std::make_unique<Ingredient>(IngredientType::ONIONS, to_string(ingredient_type),
                                     30, 10, 5, 45, 6, 1, 0, 2, 4,
                                     IngredientCategory::VEGETABLES, std::vector<int>{
                                      84,85,86,87,
                                      88,88,88,88,
                                      89,89,89,89,
                                    });
    break;
  }
  case IngredientType::GARLIC: {
    i = std::make_unique<Ingredient>(IngredientType::GARLIC, to_string(ingredient_type),
                                     40, 10, 5, 140, 4, 2, 0, 4, 6,
                                     IngredientCategory::VEGETABLES, std::vector<int>{
                                      90,91,92,93,
                                      94,94,94,94,
                                      95,95,95,95,
                                    });
    break;
  }
  case IngredientType::SWEET_CORN: {
    i = std::make_unique<Ingredient>(IngredientType::SWEET_CORN, to_string(ingredient_type),
                                     250, 20, 5, 107, 4, 2, 0, 5,
                                     7, IngredientCategory::VEGETABLES, std::vector<int>{
                                      96,97,98,99,
                                      100,101,100,101,
                                      102,102,102,102,
                                    });
    break;
  }
  case IngredientType::CAULIFLOWER: {
    i = std::make_unique<Ingredient>(IngredientType::CAULIFLOWER, to_string(ingredient_type),
                                     30, 10, 5, 28, 2, 3, 0, 6,
                                     2, IngredientCategory::VEGETABLES, std::vector<int>{
                                      103,104,105,106,
                                      107,107,107,107,
                                      108,108,108,108,
                                    });
    break;
  }
  case IngredientType::MUSHROOMS: {
    i = std::make_unique<Ingredient>(IngredientType::MUSHROOMS, to_string(ingredient_type),
                                     20, 10, 5, 24, 4, 2, 0, 3, 1,
                                     IngredientCategory::VEGETABLES, std::vector<int>{
                                      127,128,129,130,
                                      113,114,115,116,
                                      117,117,117,117,
                                    });
    break;
  }
  case IngredientType::CHANTERELLES: {
    i = std::make_unique<Ingredient>(IngredientType::CHANTERELLES, to_string(ingredient_type),
                                     15, 8, 5, 22, 4, 2, 0, 3,
                                     1, IngredientCategory::VEGETABLES, std::vector<int>{
                                      118,119,120,121,
                                      122,123,124,125,
                                      126,126,126,126,
                                    });
    break;
  }
  case IngredientType::W_MUSHROOMS: {
    i = std::make_unique<Ingredient>(IngredientType::W_MUSHROOMS, to_string(ingredient_type),
                                     30, 10, 10, 34, 4, 2, 0, 3,
                                     2, IngredientCategory::VEGETABLES, std::vector<int>{
                                      109,110,111,112,
                                      113,114,115,116,
                                      117,117,117,117,
                                    });
    break;
  }
  case IngredientType::SALAMI: {
    i = std::make_unique<Ingredient>(IngredientType::SALAMI, to_string(ingredient_type),
                                     20, 6, 3, 180, 4, 5, 0, 3, 3,
                                     IngredientCategory::MEAT, std::vector<int>{
                                       131,131,131,131,
                                       132,133,132,133,
                                       134,134,134,134,
                                      });
    break;
  }
  case IngredientType::BACON: {
    i = std::make_unique<Ingredient>(IngredientType::BACON, to_string(ingredient_type),
                                     40, 20, 5, 168, 4, 9, 0, 1, 3,
                                     IngredientCategory::MEAT, std::vector<int>{
                                     135,136,135,136,
                                     137,138,137,138,
                                     139,139,139,139,
                                    });
    break;
  }
  case IngredientType::HAM: {
    i = std::make_unique<Ingredient>(IngredientType::HAM, to_string(ingredient_type),
                                     40, 20, 10, 234, 4, 7, 0, 1, 2,
                                     IngredientCategory::MEAT, std::vector<int>{
                                      140,141,140,141,
                                      142,143,142,143,
                                      144,144,144,144,
                                    });
    break;
  }
  case IngredientType::BEEF: {
    i = std::make_unique<Ingredient>(IngredientType::BEEF, to_string(ingredient_type),
                                     60, 20, 10, 253, 4, 6, 0, 2, 3,
                                     IngredientCategory::MEAT, std::vector<int>{
                                      145,146,145,146,
                                      147,147,147,147,
                                      148,148,148,148,
                                    });
    break;
  }
  case IngredientType::PORK: {
    i = std::make_unique<Ingredient>(IngredientType::PORK, to_string(ingredient_type),
                                     60, 20, 10, 305, 4, 6, 0, 3, 3,
                                     IngredientCategory::MEAT, std::vector<int>{
                                      149,150,149,150,
                                      147,147,147,147,
                                      148,148,148,148,
                                    });
    break;
  }
  case IngredientType::CHICKEN: {
    i = std::make_unique<Ingredient>(IngredientType::CHICKEN, to_string(ingredient_type),
                                     150, 20, 10, 144, 3, 7, 0, 1, 2,
                                     IngredientCategory::MEAT, std::vector<int>{
                                      151,152,153,154,
                                      155,156,155,156,
                                      320,320,320,320,
                                    });
    break;
  }
  case IngredientType::MINCEMEAT: {
    i = std::make_unique<Ingredient>(IngredientType::MINCEMEAT, to_string(ingredient_type),
                                     40, 20, 10, 213, 2, 7, 0, 2,
                                     2, IngredientCategory::MEAT, std::vector<int>{
                                      157,157,157,157,
                                      158,158,158,158,
                                      159,159,159,159,
                                    });
    break;
  }
  case IngredientType::SAUSAGE: {
    i = std::make_unique<Ingredient>(IngredientType::SAUSAGE, to_string(ingredient_type),
                                     50, 20, 10, 189, 6, 7, 0, 2, 4,
                                     IngredientCategory::MEAT, std::vector<int>{
                                      160,161,162,163,
                                      164,164,164,164,
                                      321,321,321,321,
                                    });
    break;
  }
  case IngredientType::SCAMPI: {
    i = std::make_unique<Ingredient>(IngredientType::SCAMPI, to_string(ingredient_type),
                                     80, 30, 10, 90, 5, 6, 0, 4, 3,
                                     IngredientCategory::MEAT, std::vector<int>{
                                      165,166,167,168,
                                      169,170,171,172,
                                      173,173,173,173,
                                    });
    break;
  }
  case IngredientType::SHRIMPS: {
    i = std::make_unique<Ingredient>(IngredientType::SHRIMPS, to_string(ingredient_type),
                                     40, 20, 10, 103, 4, 6, 0, 6, 2,
                                     IngredientCategory::MEAT, std::vector<int>{
                                      174,174,174,174,
                                      175,175,175,175,
                                      173,173,173,173,
                                    });
    break;
  }
  case IngredientType::SQUID: {
    i = std::make_unique<Ingredient>(IngredientType::SQUID, to_string(ingredient_type),
                                     80, 40, 5, 107, 4, 6, 0, 7, 5,
                                     IngredientCategory::MEAT, std::vector<int>{
                                      176,177,176,177,
                                      178,178,178,178,
                                      179,179,179,179,
                                    });
    break;
  }
  case IngredientType::ANCHOVY: {
    i = std::make_unique<Ingredient>(IngredientType::ANCHOVY, to_string(ingredient_type),
                                     100, 20, 5, 242, 5, 7, 0, 8, 5,
                                     IngredientCategory::MEAT, std::vector<int>{
                                      180,181,182,183,
                                      184,185,184,185,
                                      317,317,317,317,
                                    });
    break;
  }
  case IngredientType::SALMON: {
    i = std::make_unique<Ingredient>(IngredientType::SALMON, to_string(ingredient_type),
                                     80, 20, 10, 108, 5, 6, 0, 8, 4,
                                     IngredientCategory::MEAT, std::vector<int>{
                                       195,196,195,196,
                                       197,198,197,198,
                                       318,318,318,318,
                                    });
    break;
  }
  case IngredientType::LOBSTER: {
    i = std::make_unique<Ingredient>(IngredientType::LOBSTER, to_string(ingredient_type),
                                     250, 20, 10, 89, 4, 4, 0, 7, 5,
                                     IngredientCategory::MEAT, std::vector<int>{
                                      199,199,199,199,
                                      200,201,200,201,
                                      202,203,204,205,
                                    });
    break;
  }
  case IngredientType::CAVIAR: {
    i = std::make_unique<Ingredient>(IngredientType::CAVIAR, to_string(ingredient_type),
                                     50, 20, 10, 253, 9, 4, 0, 9, 5,
                                     IngredientCategory::MEAT, std::vector<int>{
                                     206,206,206,206,
                                     207,207,207,207,
                                     208,208,208,208,
                                    });
    break;
  }
  case IngredientType::COCKLES: {
    i = std::make_unique<Ingredient>(IngredientType::COCKLES, to_string(ingredient_type),
                                     70, 40, 20, 72, 3, 5, 0, 6, 3,
                                     IngredientCategory::MEAT, std::vector<int>{
                                      213,214,215,216,
                                      217,217,217,217,
                                      212,212,212,212,
                                    });
    break;
  }
  case IngredientType::MUSSELS: {
    i = std::make_unique<Ingredient>(IngredientType::MUSSELS, to_string(ingredient_type),
                                     60, 40, 20, 82, 3, 5, 0, 6, 3,
                                     IngredientCategory::MEAT, std::vector<int>{
                                      208,209,208,209,
                                      210,211,210,211,
                                      212,212,212,212,
                                    });
    break;
  }
  case IngredientType::APPLES: {
    i = std::make_unique<Ingredient>(IngredientType::APPLES, to_string(ingredient_type),
                                     40, 10, 5, 52, 4, 0, 4, 0, 1,
                                     IngredientCategory::FRUIT, std::vector<int>{
                                     219,220,219,220,
                                     221,221,221,221,
                                     319,319,319,319,
                                    });
    break;
  }
  case IngredientType::PEARS: {
    i = std::make_unique<Ingredient>(IngredientType::PEARS, to_string(ingredient_type),
                                     40, 10, 5, 59, 4, 0, 5, 0, 0,
                                     IngredientCategory::FRUIT, std::vector<int>{
                                      226,227,228,229,
                                      230,230,230,230,
                                      319,319,319,319,
                                    });
    break;
  }
  case IngredientType::MAN_ORANGES: {
    i = std::make_unique<Ingredient>(IngredientType::MAN_ORANGES, to_string(ingredient_type),
                                     40, 20, 10, 10, 5, 0, 3, 0,
                                     0, IngredientCategory::FRUIT, std::vector<int>{
                                      237,237,237,237,
                                      238,239,240,241,
                                      236,236,236,236,
                                    });
    break;
  }
  case IngredientType::PLUMS: {
    i = std::make_unique<Ingredient>(IngredientType::PLUMS, to_string(ingredient_type),
                                     30, 20, 10, 53, 3, 1, 3, 0, 0,
                                     IngredientCategory::FRUIT, std::vector<int>{
                                      242,243,242,243,
                                      244,245,244,245,
                                      246,246,246,246,
                                    });
    break;
  }
  case IngredientType::CHERRIES: {
    i = std::make_unique<Ingredient>(IngredientType::CHERRIES, to_string(ingredient_type),
                                     20, 10, 10, 64, 5, 0, 3, 0, 1,
                                     IngredientCategory::FRUIT, std::vector<int>{
                                      247,247,247,247,
                                      248,248,248,248,
                                      249,249,249,249,
                                    });
    break;
  }
  case IngredientType::BANANAS: {
    i = std::make_unique<Ingredient>(IngredientType::BANANAS, to_string(ingredient_type),
                                     100, 20, 10, 90, 4, 2, 8, 0, 1,
                                     IngredientCategory::FRUIT, std::vector<int>{
                                     250,251,252,253,
                                     254,254,254,254,
                                     255,255,255,255,
                                    });
    break;
  }
  case IngredientType::LEMONS: {
    i = std::make_unique<Ingredient>(IngredientType::LEMONS, to_string(ingredient_type),
                                     30, 10, 10, 30, 6, 0, 250, 0, 2,
                                     IngredientCategory::FRUIT, std::vector<int>{
                                     256,256,256,256,
                                     257,258,259,260,
                                     261,261,261,261,
                                    });
    break;
  }
  case IngredientType::PINEAPPLE: {
    i = std::make_unique<Ingredient>(IngredientType::PINEAPPLE, to_string(ingredient_type),
                                     80, 30, 10, 57, 4, 0, 7, 0, 1,
                                     IngredientCategory::FRUIT, std::vector<int>{
                                      268,268,268,268,
                                      269,270,271,272,
                                      273,273,273,273,
                                    });
    break;
  }
  case IngredientType::KIWI: {
    i = std::make_unique<Ingredient>(IngredientType::KIWI, to_string(ingredient_type),
                                     60, 20, 10, 60, 4, 0, 6, 0, 1,
                                     IngredientCategory::FRUIT, std::vector<int>{
                                      274,274,274,274,
                                      275,276,277,278,
                                      279,279,279,279,
                                    });

    break;
  }
  case IngredientType::STRAWBERRIES: {
    i = std::make_unique<Ingredient>(IngredientType::STRAWBERRIES, to_string(ingredient_type),
                                     20, 10, 10, 40, 3, 0, 4, 0,
                                     1, IngredientCategory::FRUIT, std::vector<int>{
                                      280,281,282,283,
                                      284,285,286,287,
                                      288,288,288,288,
                                    });
    break;
  }
  case IngredientType::RASPBERRIES: {
    i = std::make_unique<Ingredient>(IngredientType::RASPBERRIES, to_string(ingredient_type),
                                     20, 10, 10, 40, 2, 0, 6, 0,
                                     1, IngredientCategory::FRUIT, std::vector<int>{
                                      289,289,289,289,
                                      288,288,288,288,
                                      288,288,288,288,
                                    });
    break;
  }
  case IngredientType::BLACKBERRIES: {
    i = std::make_unique<Ingredient>(IngredientType::BLACKBERRIES, to_string(ingredient_type),
                                     20, 10, 10, 44, 2, 0, 2, 0,
                                     0, IngredientCategory::FRUIT, std::vector<int>{
                                      290,290,290,290,
                                      291,291,291,291,
                                      288,288,288,288,
                                    });
    break;
  }
  case IngredientType::GOOSEBERRIES: {
    i = std::make_unique<Ingredient>(IngredientType::GOOSEBERRIES, to_string(ingredient_type),
                                     40, 10, 10, 74, 2, 0, 7, 0,
                                     0, IngredientCategory::FRUIT, std::vector<int>{
                                      292,293,292,293,
                                      294,294,294,294,
                                      294,294,294,294,
                                    });
    break;
  }
  case IngredientType::EGG: {
    i = std::make_unique<Ingredient>(IngredientType::EGG, to_string(ingredient_type),
                                     60, 35, 35, 167, 2, 2, 1, 0, 1,
                                     IngredientCategory::CHEESE, std::vector<int>{
                                      295,296,295,296,
                                      297,298,297,298,
                                      316,316,316,316,
                                    });
    break;
  }
  case IngredientType::MOZZARELLA: {
    i = std::make_unique<Ingredient>(IngredientType::MOZZARELLA, to_string(ingredient_type),
                                     80, 20, 5, 200, 3, 6, 1, 6,
                                     1, IngredientCategory::CHEESE, std::vector<int>{
                                      299,300,299,300,
                                      301,301,301,301,
                                      305,305,305,305,
                                    });
    break;
  }
  case IngredientType::PROVOLONE: {
    i = std::make_unique<Ingredient>(IngredientType::PROVOLONE, to_string(ingredient_type),
                                     100, 20, 5, 200, 3, 6, 1, 4,
                                     1, IngredientCategory::CHEESE, std::vector<int>{
                                      302,303,302,303,
                                      304,304,304,304,
                                      305,305,305,305,
                                    });
    break;
  }
  case IngredientType::BEL_PAESE: {
    i = std::make_unique<Ingredient>(IngredientType::BEL_PAESE, to_string(ingredient_type),
                                     100, 20, 5, 200, 3, 6, 1, 4,
                                     1, IngredientCategory::CHEESE, std::vector<int>{
                                      306,307,306,307,
                                      308,308,308,308,
                                      305,305,305,305,
                                    });
    break;
  }
  case IngredientType::GORGONZOLA: {
    i = std::make_unique<Ingredient>(IngredientType::GORGONZOLA, to_string(ingredient_type),
                                     100, 20, 5, 200, 3, 6, 1, 6,
                                     3, IngredientCategory::CHEESE, std::vector<int>{
                                      309,309,309,309,
                                      310,311,310,311,
                                      312,312,312,312,
                                    });
    break;
  }
  case IngredientType::PAMERINO: {
    i = std::make_unique<Ingredient>(IngredientType::PAMERINO, to_string(ingredient_type),
                                     100, 20, 5, 200, 3, 6, 1, 6, 1,
                                     IngredientCategory::CHEESE, std::vector<int>{
                                     224,224,224,224,
                                     223,223,223,223,
                                     305,305,305,305,
                                    });
    break;
  }
  case IngredientType::PARMESAN: {
    i = std::make_unique<Ingredient>(IngredientType::PARMESAN, to_string(ingredient_type),
                                     100, 20, 5, 306, 4, 6, 1, 6, 1,
                                     IngredientCategory::CHEESE, std::vector<int>{
                                      313,314,315,225,
                                      223,223,223,223,
                                      305,305,305,305,
                                    });
    break;
  }
  case IngredientType::GOATS_CHEESE: {
    i = std::make_unique<Ingredient>(IngredientType::GOATS_CHEESE, to_string(ingredient_type),
                                     100, 20, 5, 170, 5, 6, 1,
                                     6, 1, IngredientCategory::CHEESE, std::vector<int>{
                                     222,222,222,222,
                                     218,218,218,218,
                                     312,312,312,312,
                                    });
    break;
  }
  case IngredientType::PARSLEY: {
    i = std::make_unique<Ingredient>(IngredientType::PARSLEY, to_string(ingredient_type),
                                     4, 2, 1, 61, 10, 0, 1, 2, 1,
                                     IngredientCategory::CHEESE, std::vector<int>{
                                      13,14,186,187,
                                      188,188,188,188,
                                      267,267,267,267,
                                    });
    break;
  }
  case IngredientType::LEMON_BALM: {
    i = std::make_unique<Ingredient>(IngredientType::LEMON_BALM, to_string(ingredient_type),
                                     4, 2, 1, 1, 14, 0, 1, 2, 1,
                                     IngredientCategory::CHEESE, std::vector<int>{
                                      189,190,191,192,
                                      188,188,188,188,
                                      267,267,267,267,
                                    });
    break;
  }
  case IngredientType::SAGE: {
    i = std::make_unique<Ingredient>(IngredientType::SAGE, to_string(ingredient_type),
                                     4, 2, 1, 46, 15, 0, 1, 1, 1,
                                     IngredientCategory::CHEESE, std::vector<int>{
                                      231,232,233,234,
                                      188,188,188,188,
                                      267,267,267,267,
                                    });
    break;
  }
  case IngredientType::GINGER: {
    i = std::make_unique<Ingredient>(IngredientType::GINGER, to_string(ingredient_type),
                                     4, 2, 1, 66, 19, 0, 1, 2, 1,
                                     IngredientCategory::CHEESE, std::vector<int>{
                                      235,262,263,264,
                                      188,188,188,188,
                                      267,267,267,267,
                                    });
    break;
  }
  case IngredientType::OREGANO: {
    i = std::make_unique<Ingredient>(IngredientType::OREGANO, to_string(ingredient_type),
                                     4, 2, 1, 35, 20, 0, 1, 3, 1,
                                     IngredientCategory::CHEESE, std::vector<int>{
                                      265,266,265,266,
                                      188,188,188,188,
                                      267,267,267,267,
                                    });
    break;
  }
  // clang-format on
  default:
    fatal(compat::format("IngredientId {} is unknown.", ingredient_id));
    break;
  }

  auto* ingredient_ptr = i.get();
  cache_[ingredient_id] = std::move(i);

  return ingredient_ptr;
}
