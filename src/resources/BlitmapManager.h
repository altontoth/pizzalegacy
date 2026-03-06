/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "resources/Blitmap.h"
#include "resources/BlitmapIcons.h"

class BlitmapManager {
public:
  BlitmapManager(VgaManager* vga_manager);
  Blitmap<Blitmap1Icon>& get1() { return blitmap1_; }
  Blitmap<Blitmap2Icon>& get2() { return blitmap2_; }
  Blitmap<Blitmap3Icon>& get3() { return blitmap3_; }
  Blitmap<Blitmap4Icon>& get4() { return blitmap4_; }

  template <typename IconEnum> Blitmap<IconEnum>& get();

private:
  // Each Blitmap is stored so we only load it once
  Blitmap<Blitmap1Icon> blitmap1_;
  Blitmap<Blitmap2Icon> blitmap2_;
  Blitmap<Blitmap3Icon> blitmap3_;
  Blitmap<Blitmap4Icon> blitmap4_;
};
