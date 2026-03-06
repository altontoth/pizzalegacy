/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/supply/Supplier.h"

#include <cstddef>

Supplier::Supplier(SupplierID id, SupplierType type, uint8_t quality,
                   std::string name, uint16_t price_per_tile,
                   std::array<uint8_t, 6> customer_ratings,
                   uint16_t introduction_year, uint8_t u2, uint8_t u3)
    : id_(id), type_(type), quality_(quality), name_(name),
      price_per_tile_(static_cast<int64_t>(price_per_tile * 100)),
      customer_ratings_(customer_ratings),
      introduction_year_(introduction_year), u2_(u2), u3_(u3) {}

uint8_t Supplier::quality() const { return quality_; }

void Supplier::quality(uint8_t quality) { quality_ = quality; }

std::string Supplier::name() const { return name_; }

void Supplier::name(std::string name) { name_ = name; }

SupplierID Supplier::id() const { return id_; }

SupplierType Supplier::type() const { return type_; }

uint16_t Supplier::introduction_year() const { return introduction_year_; }

std::array<uint8_t, 6> Supplier::customer_ratings() const {
  return customer_ratings_;
}

Money Supplier::price_per_tile() const { return price_per_tile_; }
