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
  Mouse,      // follow cursor
  InputPanel, // fixed slot inside input panel
};

struct TooltipRequest {
  std::string text;

  TooltipAnchor anchor = TooltipAnchor::InputPanel;
  ViewPoint screenPos = {0, 0}; // only used when anchor == Mouse

  FontFace font = FontFace::CSTANDAR;
  uint8_t palette = 0;
  uint8_t bg_color = 17;
  uint8_t text_color = 240;
  uint8_t border_color = 0; // 0 => no border
  int padding = 4;
};
