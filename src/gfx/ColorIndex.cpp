/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "gfx/ColorIndex.h"

#include "resources/util/Platform.h"

namespace gfx {

uint8_t ColorIndex::get() const {
  if (platform::is_amiga() && amiga.has_value()) {
    return amiga.value();
  }
  return dos;
}

} // namespace gfx
