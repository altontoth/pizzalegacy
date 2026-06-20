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
  bool automatic_report = false;
  // Use 640x480 (4:3) CRT aspect ratio instead of 640x400
  bool crt_aspect_ratio = false;
  bool fix_legacy_bugs = true;
  bool qol_improvements = true;
  // Asset format (English or German DOS)
  platform::AssetFormat asset_format = platform::AssetFormat::ENGLISH;
  ScreenResolution resolution = ScreenResolution::Res_640x400;
  bool auto_save = true;
  bool rmb_drag_inverted = false;
  bool scroll_zoom_inverted = false;
  bool software_cursor = true;
};
