/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/restaurant/CharacterRenderer.h"

#include "resources/FontManager.h"

namespace views::screens::restaurant {

CharacterRenderer::CharacterRenderer(GameContext& ctx)
    : guests_bob_(ctx.bob->get("bob/gaeste.bob")) {
  load_seeds();
}

int CharacterRenderer::base_index_for(Part p) {
  switch (p) {
  case Part::Torso:
    return 0x30;
  case Part::Head:
    return 0x00;
  case Part::Legs:
    return 0x60;
  case Part::Arm1:
    return 0x90;
  case Part::Arm2:
    return 0xC0;
  case Part::Arm3:
    return 0xF0;
  case Part::Arm4:
    return 0x120;
  default:
    return 0x00;
  }
}

Surface CharacterRenderer::frame_for(Part p, const CharacterPose& pose) const {
  const int r = static_cast<int>(pose.facing) & 3;
  const int c = static_cast<int>(pose.customer_type) * 4;
  const int idx = base_index_for(p) + r + c;
  return guests_bob_.frame(idx);
}

ViewPoint CharacterRenderer::apply_anchor(ViewPoint p, Size s,
                                          const AnchorRule& ar) {
  switch (ar.ref) {
  case AnchorRef::TR:
    p.x -= (s.w - 1);
    break;
  case AnchorRef::BL:
    p.y -= (s.h - 1);
    break;
  case AnchorRef::BR:
    p.x -= (s.w - 1);
    p.y -= (s.h - 1);
    break;
  case AnchorRef::Center:
    p.x -= s.w / 2;
    p.y -= s.h / 2;
    break;
  case AnchorRef::Custom:
    p.x -= ar.ox;
    p.y -= ar.oy;
    break;
  case AnchorRef::TL:
  default:
    break;
  }
  return p;
}

ViewPoint CharacterRenderer::pos_for(Part part, ViewPoint torso_anchor,
                                     const CharacterPose& pose,
                                     const Surface& img) const {
  const int r = static_cast<int>(pose.facing) & 3;
  const auto& off = part_offsets_[r][(size_t)part];
  const auto& ar = part_anchor_[r][(size_t)part];
  ViewPoint p = torso_anchor + off;
  return apply_anchor(p, img.size(), ar);
}

void CharacterRenderer::render_parts(const RenderContext& ctx,
                                     ViewPoint torso_anchor_on_screen,
                                     const CharacterPose& pose,
                                     std::span<const Part> draw_order) const {
  for (Part part : draw_order) {
    // Only draw the arm matching arm_state (0..3 select Arm1..Arm4)
    if (part == Part::Arm1 || part == Part::Arm2 || part == Part::Arm3 ||
        part == Part::Arm4) {
      const int which = pose.arm_state;
      const Part active = static_cast<Part>((int)Part::Arm1 + which);
      if (part != active) {
        continue;
      }
    }
    const Surface img = frame_for(part, pose);
    const ViewPoint dst = pos_for(part, torso_anchor_on_screen, pose, img);
    ctx.surface->blit_masked(img, dst);
  }
}

void CharacterRenderer::render_back_half(const RenderContext& ctx,
                                         ViewPoint torso_anchor,
                                         const CharacterPose& pose) const {
  const Part order[] = {Part::Legs};
  render_parts(ctx, torso_anchor, pose, order);
}

void CharacterRenderer::render_front_half(const RenderContext& ctx,
                                          ViewPoint torso_anchor,
                                          const CharacterPose& pose) const {
  const Part order[] = {Part::Torso, Part::Head, Part::Arm1,
                        Part::Arm2,  Part::Arm3, Part::Arm4};
  render_parts(ctx, torso_anchor, pose, order);
}

void CharacterRenderer::load_seeds() {
  std::array<int8_t, 4> x_torso = {-12, -22, -18, -2};
  std::array<int8_t, 4> y_torso = {0, -14, -16, -20};
  std::array<int8_t, 4> x_legs = {-12, -42, -12, 16};
  std::array<int8_t, 4> y_legs = {18, -14, -32, -12};
  std::array<int8_t, 4> x_arm1 = {-18, -4, 14, 0};
  std::array<int8_t, 4> y_arm1 = {2, -20, -16, 12};
  std::array<int8_t, 4> x_arm2 = {-20, -2, 14, 0};
  std::array<int8_t, 4> y_arm2 = {0, -20, -30, 12};
  std::array<int8_t, 4> x_arm3 = {-4, -2, 20, 0};
  std::array<int8_t, 4> y_arm3 = {0, -20, -24, 4};
  std::array<int8_t, 4> x_arm4 = {0, -2, 24, 2};
  std::array<int8_t, 4> y_arm4 = {2, -22, -18, 0};

  for (int r = 0; r < 4; ++r) {
    part_offsets_[r][(int)Part::Head] = {0, 0};
    part_offsets_[r][(int)Part::Torso] = {x_torso[r], y_torso[r]};
    part_offsets_[r][(int)Part::Legs] = {x_legs[r], y_legs[r]};
    part_offsets_[r][(int)Part::Arm1] = {x_arm1[r], y_arm1[r]};
    part_offsets_[r][(int)Part::Arm2] = {x_arm2[r], y_arm2[r]};
    part_offsets_[r][(int)Part::Arm3] = {x_arm3[r], y_arm3[r]};
    part_offsets_[r][(int)Part::Arm4] = {x_arm4[r], y_arm4[r]};

    part_anchor_[r][(int)Part::Torso] = {AnchorRef::TL};
    part_anchor_[r][(int)Part::Legs] = {AnchorRef::TL};
    part_anchor_[r][(int)Part::Head] = {AnchorRef::Center};
    // arms: keep shoulder fixed relative to facing
    part_anchor_[r][(int)Part::Arm1] = (r == (int)Facing::Left)
                                           ? AnchorRule{AnchorRef::TR}
                                           : AnchorRule{AnchorRef::TL};
    part_anchor_[r][(int)Part::Arm2] = (r == (int)Facing::Left)
                                           ? AnchorRule{AnchorRef::TR}
                                           : AnchorRule{AnchorRef::TL};
    part_anchor_[r][(int)Part::Arm3] = (r == (int)Facing::Right)
                                           ? AnchorRule{AnchorRef::TL}
                                           : AnchorRule{AnchorRef::TR};
    part_anchor_[r][(int)Part::Arm4] = (r == (int)Facing::Right)
                                           ? AnchorRule{AnchorRef::TL}
                                           : AnchorRule{AnchorRef::TR};
  }
}
} // namespace views::screens::restaurant
