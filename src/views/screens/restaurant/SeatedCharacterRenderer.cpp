/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/restaurant/SeatedCharacterRenderer.h"
#include "resources/FontManager.h"

using namespace views::screens::restaurant;

SeatedCharacterRenderer::SeatedCharacterRenderer(GameContext& ctx)
    : guests_bob_(ctx.bob->get("bob/gaeste.bob")) {
  load_seeds();
}

int SeatedCharacterRenderer::base_index_for(Part p) {
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

Surface SeatedCharacterRenderer::frame_for(Part p,
                                           const CharacterPose& pose) const {
  const int r = static_cast<int>(pose.facing) & 3;
  const int c = pose.guest_type * 4;
  const int idx = base_index_for(p) + r + c;
  return guests_bob_.frame(idx);
}

ViewPoint SeatedCharacterRenderer::apply_anchor(ViewPoint p, Size s,
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

ViewPoint SeatedCharacterRenderer::pos_for(Part part, ViewPoint torso_anchor,
                                           const CharacterPose& pose,
                                           const Surface& img) const {
  const int r = static_cast<int>(pose.facing) & 3;
  const auto& off = part_offsets_[r][(size_t)part];
  const auto& ar = part_anchor_[r][(size_t)part];
  ViewPoint p = torso_anchor + off;
  return apply_anchor(p, img.size(), ar);
}

void SeatedCharacterRenderer::render_parts(
    Surface& surface, ViewPoint torso_anchor_on_screen,
    const CharacterPose& pose, std::span<const Part> draw_order) const {
  for (Part part : draw_order) {
    // Only draw the arm matching arm_state (0..3 select Arm1..Arm4)
    if (part == Part::Arm1 || part == Part::Arm2 || part == Part::Arm3 ||
        part == Part::Arm4) {
      const int which = pose.arm_state; // 0..3
      const Part active = static_cast<Part>((int)Part::Arm1 + which);
      if (part != active) {
        continue;
      }
    }
    const Surface img = frame_for(part, pose);
    const ViewPoint dst = pos_for(part, torso_anchor_on_screen, pose, img);
    surface.blit_masked(img, dst);
  }
}

void SeatedCharacterRenderer::render_lower_body(
    Surface& surface, ViewPoint torso_anchor, const CharacterPose& pose) const {
  const Part order[] = {Part::Legs};
  render_parts(surface, torso_anchor, pose, order);
}

void SeatedCharacterRenderer::render_upper_body(
    Surface& surface, ViewPoint torso_anchor, const CharacterPose& pose) const {
  const Part order[] = {Part::Torso, Part::Head, Part::Arm1,
                        Part::Arm2,  Part::Arm3, Part::Arm4};
  render_parts(surface, torso_anchor, pose, order);
}

void SeatedCharacterRenderer::load_seeds() {
  // Offsets from original at 1x, doubled to 2x for our rendering NOTE: In the
  // original, all parts offset from the furniture base position. In our code,
  // the "anchor" passed in is already the furniture position, so we apply each
  // part's offset from that common base.

  // Head offsets
  std::array<int8_t, 4> x_head = {2, 6, 2, -26};
  std::array<int8_t, 4> y_head = {-30, 2, 10, -2};

  // Torso offsets
  std::array<int8_t, 4> x_torso = {0, 2, -8, -14};
  std::array<int8_t, 4> y_torso = {-16, 0, 0, -8};

  // Legs offsets
  std::array<int8_t, 4> x_legs = {0, -16, 0, 0};
  std::array<int8_t, 4> y_legs = {0, 0, -20, 0};

  // Arm offsets
  std::array<int8_t, 4> x_arm1 = {-6, 0, 20, -22};
  std::array<int8_t, 4> y_arm1 = {-16, -6, 2, 20};
  // Arm2
  std::array<int8_t, 4> x_arm2 = {-8, -20, 20, -22};
  std::array<int8_t, 4> y_arm2 = {-16, -8, -16, 20};
  // Arm3
  std::array<int8_t, 4> x_arm3 = {-8, -16, 12, -22};
  std::array<int8_t, 4> y_arm3 = {-16, -8, -12, 16};
  // Arm4
  std::array<int8_t, 4> x_arm4 = {-10, -12, 6, -22};
  std::array<int8_t, 4> y_arm4 = {-16, -10, 0, 12};

  for (int r = 0; r < 4; ++r) {
    part_offsets_[r][(int)Part::Head] = {x_head[r], y_head[r]};
    part_offsets_[r][(int)Part::Torso] = {x_torso[r], y_torso[r]};
    part_offsets_[r][(int)Part::Legs] = {x_legs[r], y_legs[r]};
    part_offsets_[r][(int)Part::Arm1] = {x_arm1[r], y_arm1[r]};
    part_offsets_[r][(int)Part::Arm2] = {x_arm2[r], y_arm2[r]};
    part_offsets_[r][(int)Part::Arm3] = {x_arm3[r], y_arm3[r]};
    part_offsets_[r][(int)Part::Arm4] = {x_arm4[r], y_arm4[r]};

    // Use top-left positioning for all parts
    part_anchor_[r][(int)Part::Torso] = {AnchorRef::TL};
    part_anchor_[r][(int)Part::Legs] = {AnchorRef::TL};
    part_anchor_[r][(int)Part::Head] = {AnchorRef::TL};
    part_anchor_[r][(int)Part::Arm1] = {AnchorRef::TL};
    part_anchor_[r][(int)Part::Arm2] = {AnchorRef::TL};
    part_anchor_[r][(int)Part::Arm3] = {AnchorRef::TL};
    part_anchor_[r][(int)Part::Arm4] = {AnchorRef::TL};
  }
}
