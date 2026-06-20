/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "resources/AssetMetadata.h"

namespace resources {

uint8_t AssetMetadata::palette() const { return dos_palette.value_or(0); }

const std::unordered_map<std::string, AssetMetadata>
    AssetMetadataRegistry::metadata_ = {
        {"gfx/k0", {4, 4, Size{96, 11}}},
        {"gfx/k1", {4, 4, Size{96, 22}}},
        {"gfx/k2", {4, 4, Size{96, 22}}},
        {"gfx/k3", {4, 4, Size{96, 22}}},
        {"gfx/k4", {4, 4, Size{96, 22}}},
        {"gfx/k5", {4, 4, Size{96, 22}}},
        {"gfx/k6", {4, 4, Size{96, 22}}},
        {"gfx/k7", {4, 4, Size{96, 22}}},
        {"gfx/k8", {4, 4, Size{96, 22}}},
        {"gfx/k9", {4, 4, Size{96, 22}}},
        {"gfx/k10", {4, 4, Size{96, 22}}},
        {"gfx/k11", {4, 4, Size{96, 22}}},
        {"gfx/k12", {4, 4, Size{96, 22}}},
        {"gfx/k13", {4, 4, Size{96, 22}}},

        {"gfx/back1", {6, 6, Size{192, 176}}},
        {"gfx/back2", {5, 5, Size{192, 176}}},
        {"gfx/back3", {15, 15, Size{192, 176}}},
        {"gfx/back4", {18, 18, Size{192, 176}}},
        {"gfx/back5", {17, 17, Size{192, 176}}},
        {"gfx/back6", {14, 14, Size{192, 176}}},
        {"gfx/back8", {46, 46, Size{192, 176}}},
        {"gfx/back9", {4, 4, Size{192, 176}}},
        {"gfx/back10", {22, 22, Size{192, 176}}},
        {"gfx/back11", {23, 23, Size{192, 176}}},
        {"gfx/back12", {24, 24, Size{192, 176}}},
        {"gfx/back13", {25, 25, Size{192, 176}}},
        {"gfx/back14", {61, 61, Size{192, 176}}},
        {"gfx/back15", {27, 27, Size{192, 176}}},
        {"gfx/back16", {28, 28, Size{192, 176}}},
        {"gfx/back17", {29, 29, Size{192, 176}}},
        {"gfx/back18", {30, 30, Size{192, 176}}},
        {"gfx/back19", {31, 31, Size{192, 176}}},
        {"gfx/back20", {32, 32, Size{192, 176}}},
        {"gfx/back21", {33, 33, Size{192, 176}}},
        {"gfx/back22", {34, 34, Size{192, 176}}},
        {"gfx/back24", {47, 47, Size{192, 176}}},
        {"gfx/back25", {46, 46, Size{192, 176}}},
        {"gfx/back26", {45, 45, Size{192, 176}}},
        {"gfx/back27", {26, 26, Size{192, 176}}},
        {"gfx/back28", {9, 9, Size{192, 176}}},
        {"gfx/back30", {21, 21, Size{192, 176}}},
        {"gfx/back31", {35, 35, Size{192, 176}}},
        {"gfx/back32", {36, 36, Size{192, 176}}},
        {"gfx/back33", {36, 36, Size{192, 176}}},
        {"gfx/back34", {36, 36, Size{192, 176}}},
        {"gfx/back35", {36, 36, Size{192, 176}}},
        {"gfx/back36", {37, 37, Size{192, 176}}},
        {"gfx/back37", {40, 40, Size{192, 176}}},
        {"gfx/back38", {44, 44, Size{192, 176}}},
        {"gfx/back39", {16, 16, Size{192, 176}}},
        {"gfx/back40", {44, 44, Size{192, 176}}},
        {"gfx/back41", {std::nullopt, std::nullopt, Size{320, 215}}},
        {"gfx/back99", {std::nullopt, std::nullopt, Size{65535, 254}}},

        {"gfx/blitmap1", {5, 5, Size{160, 114}}},
        {"gfx/blitmap2", {7, 7, Size{160, 114}}},
        {"gfx/blitmap3", {13, 13, Size{160, 114}}},
        {"gfx/blitmap4", {7, 7, Size{160, 114}}},
        {"gfx/blitmap5", {9, 9, Size{160, 114}}},
        {"gfx/blitmap6", {std::nullopt, std::nullopt, Size{88, 120}}},

        {"gfx/cyber", {66, 66, Size{320, 200}}},
        {"gfx/ende", {63, 63, Size{320, 200}}},
        {"gfx/euromap", {2, 2, Size{320, 200}}},
        {"euromap", {2, 2, std::nullopt}},
        {"euromap.f", {std::nullopt, std::nullopt, Size{320, 200}}},
        {"gfx/landsym", {1, 62, Size{256, 256}}},
        {"gfx/maestro", {41, 41, Size{320, 200}}},
        {"gfx/med1", {0, 0, Size{64, 64}}},
        {"gfx/menu", {38, 38, Size{320, 200}}},
        {"gfx/menu.f", {38, 38, Size{320, 200}}},
        {"gfx/mplogolo", {70, 70, Size{320, 200}}},
        {"gfx/titel1", {64, 64, Size{320, 200}}},
        {"gfx/titel2", {65, 65, Size{320, 200}}},
        {"gfx/jury", {12, 12, Size{320, 200}}},
        {"gfx/orden", {36, 36, Size{256, 256}}},
        {"gfx/bo", {62, 62, Size{320, 200}}},
        {"gfx/bo.f", {62, 62, Size{320, 200}}},
        {"gfx/permap", {0, 0, Size{88, 120}}},
        {"gfx/permap.i", {0, 0, Size{96, 120}}},
        {"gfx/icons", {62, 63, Size{256, 78}}},
        {"gfx/icons.96", {62, 63, Size{256, 78}}},
        {"gfx/tvmisc", {47, 47, Size{256, 256}}},

        {"gfx/zw1", {48, 48, Size{320, 200}}},
        {"gfx/zw2", {49, 49, Size{320, 200}}},
        {"gfx/zw3", {50, 50, Size{320, 200}}},
        {"gfx/zw4", {51, 51, Size{320, 200}}},
        {"gfx/zw5", {52, 52, Size{320, 200}}},
        {"gfx/zw6", {53, 53, Size{320, 200}}},
        {"gfx/zw7", {54, 54, Size{320, 200}}},
        {"gfx/zw8", {55, 55, Size{320, 200}}},
        {"gfx/zw9", {56, 56, Size{320, 200}}},
        {"gfx/zw10", {57, 57, Size{320, 200}}},
        {"gfx/zw11", {58, 58, Size{320, 200}}},

        {"gfx/zb1", {35, 35, std::nullopt}},
        {"gfx/zb2", {35, 35, std::nullopt}},
        {"gfx/zb3", {35, 35, std::nullopt}},
        {"gfx/zb4", {35, 35, std::nullopt}},
        {"gfx/zb5", {35, 35, std::nullopt}},
        {"gfx/zb6", {35, 35, std::nullopt}},
        {"gfx/zb7", {35, 35, std::nullopt}},
        {"gfx/zb8", {35, 35, std::nullopt}},
        {"gfx/zb9", {35, 35, std::nullopt}},
        {"gfx/zb10", {35, 35, std::nullopt}},
        {"gfx/zb11", {35, 35, std::nullopt}},
        {"gfx/zb12", {35, 35, std::nullopt}},
        {"gfx/zb13", {35, 35, std::nullopt}},
        {"gfx/zb14", {35, 35, std::nullopt}},
        {"gfx/zb15", {35, 35, std::nullopt}},
        {"gfx/zb16", {35, 35, std::nullopt}},
        {"gfx/zb17", {35, 35, std::nullopt}},
        {"gfx/zb18", {35, 35, std::nullopt}},
        {"gfx/zb19", {35, 35, std::nullopt}},
        {"gfx/zb20", {35, 35, std::nullopt}},

        {"gfx/spot0", {47, 47, Size{320, 200}}},
        {"gfx/spot1", {47, 47, Size{320, 200}}},
        {"gfx/spot2", {47, 47, Size{320, 200}}},
        {"gfx/spot3", {47, 47, Size{320, 200}}},
        {"gfx/spot4", {47, 47, Size{320, 200}}},
        {"gfx/spot5", {47, 47, Size{320, 200}}},
        {"gfx/spot6", {47, 47, Size{320, 200}}},
        {"gfx/spot7", {47, 47, Size{320, 200}}},
        {"gfx/spot8", {47, 47, Size{320, 200}}},

        {"gfx/plakat0", {46, 46, Size{120, 90}}},
        {"gfx/plakat1", {46, 46, Size{120, 90}}},
        {"gfx/plakat2", {46, 46, Size{120, 90}}},
        {"gfx/plakat3", {46, 46, Size{120, 90}}},
        {"gfx/plakat4", {46, 46, Size{120, 90}}},
        {"gfx/plakat5", {46, 46, Size{120, 90}}},
        {"gfx/plakat6", {46, 46, Size{120, 90}}},
        {"gfx/plakat7", {46, 46, Size{120, 90}}},
        {"gfx/plakat8", {46, 46, Size{120, 90}}},

        {"gfx/g0", {std::nullopt, std::nullopt, Size{64, 46}}},
        {"gfx/g1", {std::nullopt, std::nullopt, Size{64, 46}}},
        {"gfx/g2", {std::nullopt, std::nullopt, Size{64, 46}}},
        {"gfx/g3", {std::nullopt, std::nullopt, Size{64, 46}}},
        {"gfx/g4", {std::nullopt, std::nullopt, Size{64, 46}}},
        {"gfx/g5", {std::nullopt, std::nullopt, Size{64, 46}}},
        {"gfx/g6", {std::nullopt, std::nullopt, Size{64, 46}}},
        {"gfx/g7", {std::nullopt, std::nullopt, Size{64, 46}}},
        {"gfx/g8", {std::nullopt, std::nullopt, Size{64, 46}}},
        {"gfx/g9", {std::nullopt, std::nullopt, Size{64, 46}}},
        {"gfx/g10", {std::nullopt, std::nullopt, Size{64, 46}}},
        {"gfx/g11", {std::nullopt, std::nullopt, Size{64, 46}}},
        {"gfx/g12", {std::nullopt, std::nullopt, Size{64, 46}}},
        {"gfx/g13", {std::nullopt, std::nullopt, Size{64, 46}}},
        {"gfx/g14", {std::nullopt, std::nullopt, Size{64, 46}}},
        {"gfx/g15", {std::nullopt, std::nullopt, Size{64, 46}}},
        {"gfx/g16", {std::nullopt, std::nullopt, Size{64, 46}}},
        {"gfx/g17", {std::nullopt, std::nullopt, Size{64, 46}}},
        {"gfx/g18", {std::nullopt, std::nullopt, Size{64, 46}}},
        {"gfx/g19", {std::nullopt, std::nullopt, Size{64, 46}}},
        {"gfx/g20", {std::nullopt, std::nullopt, Size{64, 46}}},
        {"gfx/g21", {std::nullopt, std::nullopt, Size{64, 46}}},
        {"gfx/g22", {std::nullopt, std::nullopt, Size{64, 46}}},
        {"gfx/g23", {std::nullopt, std::nullopt, Size{64, 46}}},

        {"bob/stanbobs.bob", {8, 8, std::nullopt}},
        {"bob/gaeste.bob", {16, 16, std::nullopt}},
        {"bob/kus.bob", {4, 4, std::nullopt}},
        {"bob/kuebobs.bob", {4, 4, std::nullopt}},
        {"bob/lebbo.bob", {4, 4, std::nullopt}},
        {"bob/mens.bob", {4, 4, std::nullopt}},
        {"gfx/menubobs.bob", {38, 38, std::nullopt}},
        {"bob/menubobs.bob", {38, 38, std::nullopt}},
        {"bob/moebel.bob", {4, 4, std::nullopt}},
        {"bob/nightp.bob", {4, 4, std::nullopt}},
        {"bob/nightw0.bob", {4, 4, std::nullopt}},
        {"bob/nightw1.bob", {4, 4, std::nullopt}},
        {"bob/nightw2.bob", {4, 4, std::nullopt}},
        {"bob/nightw3.bob", {4, 4, std::nullopt}},
        {"bob/stabo.bob", {42, 42, std::nullopt}},
        {"bob/stas.bob", {4, 4, std::nullopt}},
        {"bob/typ1e.bob", {22, 22, std::nullopt}},
        {"bob/typ2e.bob", {22, 22, std::nullopt}},
        {"bob/typ3e.bob", {5, 5, std::nullopt}},
        {"bob/typ4e.bob", {14, 14, std::nullopt}},
        {"bob/typ5e.bob", {22, 22, std::nullopt}},
        {"bob/typ6e.bob", {22, 22, std::nullopt}},
        {"bob/typ7e.bob", {22, 22, std::nullopt}},
        {"bob/typ8e.bob", {22, 22, std::nullopt}},
        {"bob/typ9e.bob", {22, 22, std::nullopt}},
        {"bob/typ11e.bob", {22, 22, std::nullopt}},
        {"bob/typ12e.bob", {22, 22, std::nullopt}},
        {"bob/typ13e.bob", {22, 22, std::nullopt}},
        {"bob/typ14e.bob", {22, 22, std::nullopt}},
        {"bob/typ15e.bob", {22, 22, std::nullopt}},
        {"bob/typ16e.bob", {22, 22, std::nullopt}},
        {"bob/typ17e.bob", {22, 22, std::nullopt}},
        {"bob/typ18e.bob", {22, 22, std::nullopt}},
        {"bob/typ19e.bob", {22, 22, std::nullopt}},
        {"bob/typ20e.bob", {22, 22, std::nullopt}},
        {"bob/typ21e.bob", {22, 22, std::nullopt}},
        {"bob/typ22e.bob", {36, 36, std::nullopt}},
        {"bob/typ23e.bob", {22, 22, std::nullopt}},
        {"bob/typ24e.bob", {22, 22, std::nullopt}},
        {"bob/typ25e.bob", {22, 22, std::nullopt}},
        {"bob/walk1.bob", {4, 4, std::nullopt}},
        {"bob/walk2.bob", {4, 4, std::nullopt}},
        {"bob/walk3.bob", {4, 4, std::nullopt}},
        {"bob/walk4.bob", {4, 4, std::nullopt}},
        {"bob/walk5.bob", {4, 4, std::nullopt}},
        {"bob/walk6.bob", {4, 4, std::nullopt}},
        {"bob/walk7.bob", {4, 4, std::nullopt}},
        {"bob/walk8.bob", {4, 4, std::nullopt}},
        {"bob/walk9.bob", {4, 4, std::nullopt}},
        {"bob/walk11.bob", {4, 4, std::nullopt}},
        {"bob/walk12.bob", {4, 4, std::nullopt}},
        {"bob/walk14.bob", {4, 4, std::nullopt}},
        {"bob/zutaten.bob", {6, 6, std::nullopt}},
};

std::optional<AssetMetadata>
AssetMetadataRegistry::get(const std::string& filename) {
  auto it = metadata_.find(filename);
  if (it != metadata_.end()) {
    return it->second;
  }

  // Try basename fallback (handle paths like "/full/path/gfx/back1.vga")
  size_t last_slash = filename.find_last_of("/\\");
  if (last_slash != std::string::npos) {
    std::string basename = filename.substr(last_slash + 1);

    // Remove extension
    size_t dot = basename.find(".vga");
    if (dot != std::string::npos) {
      basename = basename.substr(0, dot);
    }

    // Try with gfx/ prefix for VGA files
    std::string with_gfx = "gfx/" + basename;
    it = metadata_.find(with_gfx);
    if (it != metadata_.end()) {
      return it->second;
    }

    // Try with bob/ prefix for BOB files
    std::string with_bob = "bob/" + basename;
    it = metadata_.find(with_bob);
    if (it != metadata_.end()) {
      return it->second;
    }

    // Try basename alone
    it = metadata_.find(basename);
    if (it != metadata_.end()) {
      return it->second;
    }
  }

  return std::nullopt;
}

std::optional<uint8_t>
AssetMetadataRegistry::get_palette(const std::string& filename) {
  auto metadata = get(filename);
  if (metadata.has_value()) {
    return metadata->palette();
  }
  return std::nullopt;
}

std::optional<Size>
AssetMetadataRegistry::get_size(const std::string& filename) {
  auto metadata = get(filename);
  if (metadata.has_value()) {
    return metadata->size;
  }
  return std::nullopt;
}

} // namespace resources
