/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "engine/Endian.h"

namespace fileutils {

// Sequential cursor over an in-memory byte buffer, offering the subset of
// std::istream operations the binary loaders use. Reads past the end yield
// zeros rather than faulting, matching how the legacy formats are parsed.
class ByteReader {
public:
  explicit ByteReader(std::span<const uint8_t> data) : data_(data) {}

  uint8_t get() { return pos_ < data_.size() ? data_[pos_++] : 0; }

  void read(void* dst, std::size_t n) {
    std::size_t avail = pos_ < data_.size() ? data_.size() - pos_ : 0;
    std::size_t copy = n < avail ? n : avail;
    std::memcpy(dst, data_.data() + pos_, copy);
    if (copy < n) {
      std::memset(static_cast<uint8_t*>(dst) + copy, 0, n - copy);
    }
    pos_ += n;
  }

  uint16_t u16le() {
    uint8_t bytes[2];
    read(bytes, sizeof(bytes));
    return endian::read_u16le(bytes);
  }

  uint32_t u32le() {
    uint8_t bytes[4];
    read(bytes, sizeof(bytes));
    return endian::read_u32le(bytes);
  }

  void seek(std::size_t pos) { pos_ = pos; }
  void skip(std::size_t n) { pos_ += n; }

  std::size_t tell() const { return pos_; }
  bool has_more() const { return pos_ < data_.size(); }
  std::size_t size() const { return data_.size(); }

private:
  std::span<const uint8_t> data_;
  std::size_t pos_ = 0;
};

} // namespace fileutils
