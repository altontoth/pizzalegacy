/*
 * SPDX-FileCopyrightText: 2020 Morgan McGuire & Mara Gagiu
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cstdint>
#include "engine/gfx/PaletteHandle.h"
#include "engine/gfx/Upscaler.h"

namespace gfx {

// Brightness of each color index, which MMPX uses to tell foreground from
// background.
using LumaTable = std::array<uint32_t, 256>;

[[nodiscard]] LumaTable luma_table(PaletteHandle palette);

void mmpx(const uint8_t* src, int src_pitch, int w, int h, uint8_t* dst,
          int dst_pitch, UpscaleEdge edge, const LumaTable& luma);

} // namespace gfx
