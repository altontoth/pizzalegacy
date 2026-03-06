/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/geom/Spaces.h"

[[nodiscard]] inline ViewPoint local_to_view(ViewPoint p,
                                             ViewPoint origin) noexcept {
  return ViewPoint{origin.x + p.x, origin.y + p.y};
}

[[nodiscard]] inline ViewRect local_to_view(ViewRect r,
                                            ViewPoint origin) noexcept {
  return ViewRect{origin.x + r.x, origin.y + r.y, r.w, r.h};
}

[[nodiscard]] inline ViewPoint view_to_local(ViewPoint p,
                                             ViewPoint origin) noexcept {
  return ViewPoint{p.x - origin.x, p.y - origin.y};
}

[[nodiscard]] inline ViewRect view_to_local(ViewRect r,
                                            ViewPoint origin) noexcept {
  return ViewRect{r.x - origin.x, r.y - origin.y, r.w, r.h};
}
