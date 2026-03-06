/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cstdint>
#include "gfx/Surface.h"
#include "resources/FontTypes.h"
using gfx::Surface;

class FontManager;

namespace views::ui::components {

enum class ButtonState : uint8_t { Normal, Pressed };

struct BeveledButtonStyle {
  uint8_t light_color = 0x0F;
  uint8_t dark_color = 0x0A;
  uint8_t text_color = 240;
  FontFace font_face = FontFace::CSTANDAR;
};

void draw_beveled_button(FontManager* font, const gfx::Surface& permap,
                         gfx::Surface& dst, const ViewRect& box,
                         std::string_view label, ButtonState state,
                         const BeveledButtonStyle& style = {});

} // namespace views::ui::components
