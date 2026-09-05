/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <cstdint>
#include <span>

class RawRenderable {
public:
  virtual int picture_width() const = 0;
  virtual int picture_height() const = 0;
  virtual std::span<const uint8_t> picture_data() const = 0;
  virtual uint8_t palette_index() const = 0;
};
