/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include "resources/BlitmapManager.h"
#include "resources/FontManager.h"

class StatBar {
public:
  explicit StatBar(BlitmapManager* blitmap_manager, FontManager* font);

  Surface create_stat_bar(const std::string_view& label, int value);

private:
  Surface& pick_color_surface(int value);

  const Blitmap<Blitmap3Icon>& blitmap3_;
  FontManager* font_;
  int bar_width_ = 0;
  int bar_height_ = 0;

  Surface bar_empty_;
  Surface bar_red_;
  Surface bar_yellow_;
  Surface bar_green_;
};
