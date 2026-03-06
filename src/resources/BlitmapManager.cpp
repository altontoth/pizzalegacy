/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "resources/BlitmapManager.h"
#include <cassert>

template <typename IconEnum> struct BlitmapIndex;
template <> struct BlitmapIndex<Blitmap1Icon> {
  static constexpr int value = 0;
};
template <> struct BlitmapIndex<Blitmap2Icon> {
  static constexpr int value = 1;
};
template <> struct BlitmapIndex<Blitmap3Icon> {
  static constexpr int value = 2;
};
template <> struct BlitmapIndex<Blitmap4Icon> {
  static constexpr int value = 3;
};

BlitmapManager::BlitmapManager(VgaManager* vga_manager)
    : blitmap1_(vga_manager, "gfx/blitmap1", "gfx/offsets1"),
      blitmap2_(vga_manager, "gfx/blitmap2", "gfx/offsets2"),
      blitmap3_(vga_manager, "gfx/blitmap3", "gfx/offsets3"),
      blitmap4_(vga_manager, "gfx/blitmap4", "gfx/offsets4") {}

template <typename IconEnum> Blitmap<IconEnum>& BlitmapManager::get() {
  static_assert(sizeof(IconEnum) == 0,
                "get<>() not implemented for this IconEnum");
}

template <> Blitmap<Blitmap1Icon>& BlitmapManager::get<Blitmap1Icon>() {
  return blitmap1_;
}

template <> Blitmap<Blitmap2Icon>& BlitmapManager::get<Blitmap2Icon>() {
  return blitmap2_;
}

template <> Blitmap<Blitmap3Icon>& BlitmapManager::get<Blitmap3Icon>() {
  return blitmap3_;
}

template <> Blitmap<Blitmap4Icon>& BlitmapManager::get<Blitmap4Icon>() {
  return blitmap4_;
}
