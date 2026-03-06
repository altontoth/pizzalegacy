/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "factories/FloorPatternFactory.h"
#include "core/supply/Supplier.h"
#include "engine/Fatal.h"
#include "factories/SupplierFactory.h"
#include "resources/util/FileUtils.h"
#include "resources/util/Platform.h"

std::unordered_map<SupplierID, std::vector<FloorPattern>>
    FloorPatternFactory::by_supplier_{};
std::string FloorPatternFactory::kachinfo_path_ =
    "d/kachinfo" + std::string(platform::is_amiga() ? "" : ".pc");

namespace {
std::vector<uint8_t> slurp(const std::string& path) {
  auto f = fileutils::open(path);
  return {std::istreambuf_iterator<char>(f), {}};
}

inline SupplierID to_supplier_id(uint8_t raw) {
  const auto norm = static_cast<uint8_t>(raw - 1);
  return static_cast<SupplierID>(norm);
}
} // namespace

void FloorPatternFactory::preload_from_path(const std::string& path) {
  kachinfo_path_ = path;
  ensure_loaded();
}

const std::vector<FloorPattern>& FloorPatternFactory::create(SupplierID sid) {
  ensure_loaded();
  auto it = by_supplier_.find(sid);
  static const std::vector<FloorPattern> kEmpty;
  if (it == by_supplier_.end()) {
    return kEmpty;
  }
  return it->second;
}

void FloorPatternFactory::ensure_loaded() {
  if (by_supplier_.empty()) {
    auto bytes = slurp(kachinfo_path_);
    parse_and_index(bytes);
  }
}

void FloorPatternFactory::parse_and_index(std::span<const uint8_t> bytes) {
  const uint8_t count = bytes[0];
  constexpr size_t rec = 6;
  const size_t expected = 1 + (static_cast<size_t>(count) * rec);
  if (bytes.size() != expected) {
    fatal("FloorPatternFactory: size mismatch in kachinfo");
  }

  by_supplier_.clear();
  by_supplier_.reserve(16); // there are ~15 vendors

  for (size_t off = 1; off < bytes.size(); off += rec) {
    const uint8_t raw_vendor = bytes[off + 0];
    const uint8_t i0 = bytes[off + 1];
    const uint8_t i1 = bytes[off + 2];
    const uint8_t i2 = bytes[off + 3];
    const uint8_t i3 = bytes[off + 4];
    const uint8_t scale = bytes[off + 5];

    const SupplierID sid = to_supplier_id(raw_vendor);

    const Supplier* sup = SupplierFactory::create_supplier(sid);
    if (!sup) {
      fatal("FloorPatternFactory: unknown supplier id " +
            std::to_string(raw_vendor));
    }

    auto norm_idx = [](uint8_t v) -> uint8_t { return v - 1; };
    std::array<uint8_t, 4> quad{norm_idx(i0), norm_idx(i1), norm_idx(i2),
                                norm_idx(i3)};
    for (auto idx : quad) {
      if (idx >= 16) {
        fatal("FloorPatternFactory: tile index >= 16");
      }
    }

    FloorPattern fp{
        .vendor_id = static_cast<uint8_t>(static_cast<uint8_t>(sid)),
        .tile_indices = quad,
    };
    {
      const auto base = sup->customer_ratings();
      static_assert(fp.popularity.size() == 6, "ConsumerType::Count must be 6");
      for (size_t k = 0; k < fp.popularity.size(); ++k) {
        fp.popularity[k] = base[k];
      }
    }
    fp.popularity_factor = scale;

    by_supplier_[sid].push_back(fp);
  }
}
