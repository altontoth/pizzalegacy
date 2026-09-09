/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include "core/finance/Money.h"

enum class SupplierType {
  FLOORING = 1,
  FURNITURE,
  KITCHENWARE,
  EXTRAS,
  VEGETABLES,
  MEAT,
  FRUIT,
  CHEESE,
};

enum class SupplierID {
  FREDS_TILE_EMPORIUM,
  USED_TILES,
  APEX_TILE_CENTER,
  CARPETS_UNLIMITED,
  EXCLUSIVE_CARPETS,
  USED_CARPET,
  EXCLUSIVE_TILES,
  CRAZY_TILES,
  HANNOS_WOODEN_FLOORS,
  MARBLE_FLOORS,
  FOIL_COATING,
  HOUSE_OF_STONE,
  RESTAURANT_FURNISHINGS,
  CLASSIC_CROWN,
  BARGAIN_BASEMENT,
  SECOND_HAND_FURNITURE,
  GARDEN_FURNITURE_LTD,
  MODERN_LIVING,
  MODERN_COMFORT,
  TRENDSETTER_STUDIO,
  SMALL_AND_SUNNY,
  PURELY_NATURAL,
  ELEGANT_AND_EXCLUSIVE,
  BUSINESS_CLASS,
  LUXURY_FURNISHINGS,
  YOUNG_ACTION,
  DANIELS_DECOR,
  BRUNIS_BLOOMS,
  VIDEOGAMES_AND_PINBALL,
  NEW_AGE_VIDEOGAMES,
  A_AND_C_ENTERTAINMENT,
  ALIS_SLOT_MACHINES,
  OVENS,
  PIZZA_OVENS,
  ELECTRIC_OVENS,
  RESTAURANT_RATIO_LTD,
};

struct SupplierData {
  SupplierID id;
  SupplierType type;
  uint8_t quality;
  std::string name;
  uint16_t price_per_tile;
  std::array<int8_t, 6> customer_ratings;
  uint16_t introduction_year;
  // The last year the supplier trades. 2200 basically means never.
  uint16_t withdrawal_year;
};

class Supplier {
public:
  explicit Supplier(const SupplierData& data);
  SupplierID id() const;
  uint8_t quality() const;
  void quality(uint8_t quality);
  std::string name() const;
  void name(std::string);
  SupplierType type() const;
  uint16_t introduction_year() const;
  uint16_t withdrawal_year() const;
  std::array<int8_t, 6> customer_ratings() const;
  Money price_per_tile() const;

private:
  SupplierID id_;
  SupplierType type_;
  uint8_t quality_;
  std::string name_;
  Money price_per_tile_;
  std::array<int8_t, 6> customer_ratings_;
  uint16_t introduction_year_;
  uint16_t withdrawal_year_;
};
