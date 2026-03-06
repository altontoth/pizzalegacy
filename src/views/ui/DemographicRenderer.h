/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/Demographics.h"
#include "gfx/Surface.h"
#include "resources/Blitmap.h"
#include "resources/BlitmapIcons.h"
#include "resources/FontManager.h"

#include <string>

namespace views::ui {

// Renders demographic popularity bars and icons.
// This is used by both the Menu & Customers and Furniture & Customers screens
// to display how popular a restaurant's menu or furniture is with the different
// demographic groups
void render_demographics(gfx::Surface& surface, FontManager& font,
                         const Blitmap<Blitmap2Icon>& blitmap,
                         const Popularity& popularity,
                         const std::string& title);

} // namespace views::ui
