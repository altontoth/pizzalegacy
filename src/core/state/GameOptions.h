/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/DisplayConstants.h"
#include "resources/util/Platform.h"

struct GameOptions {
  bool map_fx = true;
  bool restaurant_fx = true;
  bool music = true;
  bool automatic_report = true;
  // Use 640x480 (4:3) CRT aspect ratio instead of 640x400
  bool crt_aspect_ratio = false;
  bool fix_legacy_bugs = true;
  bool qol_improvements = true;
  // Asset format (DOS or Amiga)
  platform::AssetFormat asset_format = platform::AssetFormat::DOS;
  ScreenResolution resolution = ScreenResolution::Res_640x400;
  bool auto_save = true;
  bool rmb_drag_inverted = false;
};
