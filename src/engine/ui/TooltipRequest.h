/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/geom/Spaces.h"
#include "resources/FontTypes.h"

#include <cstdint>
#include <string>

enum class TooltipAnchor {
  Mouse, // pos is the cursor; the box sits just above it
  Fixed, // pos is the text center
};

struct TooltipRequest {
  std::string text;

  TooltipAnchor anchor = TooltipAnchor::Fixed;
  ViewPoint pos = {0, 0};

  FontFace font = FontFace::CSTANDAR;
  uint8_t palette = 0;
  uint8_t bg_color = 17;
  uint8_t text_color = 240;
  uint8_t border_color = 0; // 0 => no border
  int padding = 4;
};
