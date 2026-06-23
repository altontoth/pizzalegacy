/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/supply/Supplier.h"

#include <cstddef>

Supplier::Supplier(const SupplierData& d)
    : id_(d.id), type_(d.type), quality_(d.quality), name_(d.name),
      price_per_tile_(static_cast<int64_t>(d.price_per_tile * 100)),
      customer_ratings_(d.customer_ratings),
      introduction_year_(d.introduction_year), u2_(d.u2), u3_(d.u3) {}

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
