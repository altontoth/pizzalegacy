/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <bit>
#include <cstdint>

// Byte-order helpers, all on-disk game data is little-endian.
namespace endian {

constexpr uint16_t bswap16(uint16_t v) noexcept {
  return static_cast<uint16_t>((v >> 8) | (v << 8));
}

constexpr uint32_t bswap32(uint32_t v) noexcept {
  return ((v & 0xFF000000U) >> 24) | ((v & 0x00FF0000U) >> 8) |
         ((v & 0x0000FF00U) << 8) | ((v & 0x000000FFU) << 24);
}

constexpr uint64_t bswap64(uint64_t v) noexcept {
  return ((v & 0xFF00000000000000ULL) >> 56) |
         ((v & 0x00FF000000000000ULL) >> 40) |
         ((v & 0x0000FF0000000000ULL) >> 24) |
         ((v & 0x000000FF00000000ULL) >> 8) |
         ((v & 0x00000000FF000000ULL) << 8) |
         ((v & 0x0000000000FF0000ULL) << 24) |
         ((v & 0x000000000000FF00ULL) << 40) |
         ((v & 0x00000000000000FFULL) << 56);
}

// Host <-> little-endian. No-ops on little-endian hosts.
constexpr uint16_t to_le16(uint16_t v) noexcept {
  if constexpr (std::endian::native == std::endian::little) {
    return v;
  }
  else {
    return bswap16(v);
  }
}

constexpr uint32_t to_le32(uint32_t v) noexcept {
  if constexpr (std::endian::native == std::endian::little) {
    return v;
  }
  else {
    return bswap32(v);
  }
}

constexpr uint64_t to_le64(uint64_t v) noexcept {
  if constexpr (std::endian::native == std::endian::little) {
    return v;
  }
  else {
    return bswap64(v);
  }
}

constexpr uint16_t from_le16(uint16_t v) noexcept { return to_le16(v); }
constexpr uint32_t from_le32(uint32_t v) noexcept { return to_le32(v); }
constexpr uint64_t from_le64(uint64_t v) noexcept { return to_le64(v); }

// Assemble a value from little-endian bytes.
constexpr uint16_t u16le(uint8_t b0, uint8_t b1) noexcept {
  return static_cast<uint16_t>(b0 | (b1 << 8));
}

// Read little-endian scalars from a byte buffer.
constexpr uint16_t read_u16le(const uint8_t* p) noexcept {
  return u16le(p[0], p[1]);
}

constexpr uint32_t read_u24le(const uint8_t* p) noexcept {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16);
}

constexpr uint32_t read_u32le(const uint8_t* p) noexcept {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

} // namespace endian
