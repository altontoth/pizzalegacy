/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include "core/util/GameContext.h"
#include "engine/RenderContext.h"
#include "views/screens/restaurant/CharacterController.h"

namespace views::screens::restaurant {

enum class AnchorRef { TL, TR, BL, BR, Center, Custom };
struct AnchorRule {
  AnchorRef ref{AnchorRef::TL};
  int ox{0}, oy{0};
};

class SeatedCharacterRenderer {
public:
  explicit SeatedCharacterRenderer(GameContext& ctx);

  // Draw a subset of parts, in the given order (for layering with furniture).
  void render_parts(const RenderContext& ctx, ViewPoint torso_anchor_on_screen,
                    const struct CharacterPose& pose,
                    std::span<const Part> draw_order) const;

  // Convenience methods with lists of body parts already:
  void render_lower_body(const RenderContext& ctx, ViewPoint torso_anchor,
                         const CharacterPose& pose) const;
  void render_upper_body(const RenderContext& ctx, ViewPoint torso_anchor,
                         const CharacterPose& pose) const;

private:
  std::array<std::array<PxOffset, (size_t)Part::Count>, 4> part_offsets_{};
  std::array<std::array<AnchorRule, (size_t)Part::Count>, 4> part_anchor_{};

  Bob& guests_bob_;

  static int base_index_for(Part p);
  Surface frame_for(Part p, const CharacterPose& pose) const; // uses Bob::frame
  ViewPoint pos_for(Part p, ViewPoint torso_anchor, const CharacterPose& pose,
                    const Surface& img) const;
  static ViewPoint apply_anchor(ViewPoint p, Size s, const AnchorRule& ar);

  void load_seeds();
};

} // namespace views::screens::restaurant
