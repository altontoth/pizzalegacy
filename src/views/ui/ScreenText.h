/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string_view>

#include "engine/Geometry.h"

class FontManager;
namespace gfx {
class Surface;
}

namespace views::ui {

// Render text inset from the surface edges by a small margin.
void draw_screen_text(FontManager& font, gfx::Surface& surface,
                      std::string_view text, HAlign ax, VAlign ay);

} // namespace views::ui
