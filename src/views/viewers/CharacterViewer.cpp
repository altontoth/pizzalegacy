/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/viewers/CharacterViewer.h"

#include <cstdlib>
#include "engine/FormatCompat.h"
#include "engine/input/InputManager.h"
#include "gfx/Draw.h"
#include "magic_enum/magic_enum.hpp"
#include "resources/FontManager.h"
#include "views/screens/restaurant/CharacterController.h"
#include "views/screens/restaurant/SeatedCharacterRenderer.h"

using namespace views::screens::restaurant;

/* This is just a debugging tool to help figure out some offsets for the
 * characters sitting in the restaurant, how the parts all connect, etc. */
CharacterViewer::CharacterViewer(GameContext& context, GameState& state)
    : View(context, state), listener_(context.input->register_listener(*this)),
      guest_renderer_(std::make_unique<SeatedCharacterRenderer>(context)) {
  part_visible_.fill(true);
}

bool CharacterViewer::on_action(const input::ActionEvent& ev) {
  if (!ev.state.pressed) {
    return false;
  }

  auto nudge = [&](int dx, int dy) {
    part_debug_offsets_[selected_part_].dx += dx;
    part_debug_offsets_[selected_part_].dy += dy;
    mark_dirty();
    Logger::debug("Part {} offset: ({}, {})", selected_part_,
                  part_debug_offsets_[selected_part_].dx,
                  part_debug_offsets_[selected_part_].dy);
  };

  switch (ev.action) {
  case input::Action::CycleForward:
    if (++rotation_ > 3) {
      rotation_ = 0;
    }
    Logger::info("Rotation (facing) is {}", rotation_);
    mark_dirty();
    return true;
  case input::Action::CycleBackward:
    if (--rotation_ < 0) {
      rotation_ = 3;
    }
    Logger::info("Rotation (facing) is {}", rotation_);
    mark_dirty();
    return true;
  case input::Action::CycleDown:
    if (--customer_type_ < 0) {
      customer_type_ = max_character_index_;
    }
    Logger::info("Customer type: {}", customer_type_);
    mark_dirty();
    return true;
  case input::Action::CycleUp:
    if (++customer_type_ > max_character_index_) {
      customer_type_ = 0;
    }
    Logger::info("Customer type: {}", customer_type_);
    mark_dirty();
    return true;
  case input::Action::ChangeItem:
    if (++arm_state_ > 3) {
      arm_state_ = 0;
    }
    Logger::info("Arm state: {}", arm_state_);
    mark_dirty();
    return true;
  case input::Action::ToggleExtraInfo:
    show_extra_info_ = !show_extra_info_;
    dump_offsets();
    mark_dirty();
    return true;
  case input::Action::HideItem:
    part_visible_[selected_part_] = !part_visible_[selected_part_];
    Logger::info("Part {} visible: {}", selected_part_,
                 part_visible_[selected_part_]);
    mark_dirty();
    return true;
  case input::Action::ChangeItem2:
    if (++selected_part_ >= 7) {
      selected_part_ = 0;
    }
    Logger::info("Selected part: {}", selected_part_);
    mark_dirty();
    return true;
  case input::Action::MoveItemUp:
    nudge(0, -nudge_step_);
    return true;
  case input::Action::MoveItemDown:
    nudge(0, +nudge_step_);
    return true;
  case input::Action::MoveItemLeft:
    nudge(-nudge_step_, 0);
    return true;
  case input::Action::MoveItemRight:
    nudge(+nudge_step_, 0);
    return true;
  default:
    return false;
  }
}

void CharacterViewer::tick() {}

void CharacterViewer::render(const RenderContext& ctx) {
  using namespace gfx::draw;
  using namespace views::screens::restaurant;

  ctx.surface->fill(14);
  ctx.surface->palette(PaletteHandle{palette_});

  // Anchor point on screen (furniture center position)
  const ViewPoint base_anchor{200, 150};

  // Create a pose for the guest
  CharacterPose pose;
  pose.facing = static_cast<Facing>(rotation_ & 3);
  pose.customer_type = static_cast<ConsumerType>(customer_type_);
  pose.arm_state = arm_state_;
  pose.eating = true;

  // Draw crosshair at base anchor if enabled
  if (show_crosshairs_) {
    auto cross = [&](ViewPoint p, uint8_t color) {
      line(*ctx.surface, {p.x - 4, p.y}, {p.x + 4, p.y}, color, 1);
      line(*ctx.surface, {p.x, p.y - 4}, {p.x, p.y + 4}, color, 1);
    };
    cross(base_anchor, 20); // Bright crosshair at base anchor point
  }

  // Render parts individually with per-part debug offsets
  // Render order: Legs (back), then Torso/Head/Arms (front)
  using Part = views::screens::restaurant::Part;

  // Back half: Legs
  if (part_visible_[2]) { // Legs
    ViewPoint anchor = base_anchor + part_debug_offsets_[2];
    const Part order[] = {Part::Legs};
    guest_renderer_->render_parts(ctx, anchor, pose, order);
  }

  // Front half: Torso, Head, Arms
  auto render_part = [&](int part_idx, Part part) {
    if (part_visible_[part_idx]) {
      ViewPoint anchor = base_anchor + part_debug_offsets_[part_idx];
      const Part order[] = {part};
      guest_renderer_->render_parts(ctx, anchor, pose, order);
    }
  };

  render_part(0, Part::Torso);
  render_part(1, Part::Head);

  // Only render the active arm based on arm_state
  if (arm_state_ == 0) {
    render_part(3, Part::Arm1);
  }
  if (arm_state_ == 1) {
    render_part(4, Part::Arm2);
  }
  if (arm_state_ == 2) {
    render_part(5, Part::Arm3);
  }
  if (arm_state_ == 3) {
    render_part(6, Part::Arm4);
  }

  // Part names for display
  const char* part_names[] = {"Torso", "Head", "Legs", "Arm1",
                              "Arm2",  "Arm3", "Arm4"};

  // Labels
  std::string info = compat::format(
      "Facing: {} ({})  Customer: {}  Arm: {}  Part: {} [{}, {}] {}",
      magic_enum::enum_name(pose.facing), rotation_, customer_type_, arm_state_,
      part_names[selected_part_], part_debug_offsets_[selected_part_].dx,
      part_debug_offsets_[selected_part_].dy,
      part_visible_[selected_part_] ? "VIS" : "HIDDEN");

  context_.font->render_line_onto(*ctx.surface, info, ViewPoint{10, 10},
                                  FontFace::CSTANDAR);

  if (show_extra_info_) {
    std::string extra = compat::format(
        "Anchor at ({}, {})\n"
        "Arrow keys: cycle facing/customer | Tab: cycle arm | Space: cycle "
        "part\n"
        "WASD: nudge selected part | H: hide/show part | I: dump offsets",
        base_anchor.x, base_anchor.y);
    context_.font->render_line_onto(*ctx.surface, extra, ViewPoint{10, 30},
                                    FontFace::CSTANDAR);
  }
}

void CharacterViewer::dump_offsets() const {
  const char* part_names[] = {"Torso", "Head", "Legs", "Arm1",
                              "Arm2",  "Arm3", "Arm4"};

  Logger::info("=== CharacterViewer Current State ===");
  Logger::info("Facing: {} ({})",
               magic_enum::enum_name(static_cast<Facing>(rotation_ & 3)),
               rotation_);
  Logger::info("Customer type: {}", customer_type_);
  Logger::info("Arm state: {}", arm_state_);
  Logger::info("Selected part: {} ({})", selected_part_,
               part_names[selected_part_]);
  Logger::info("");
  Logger::info("Per-part debug offsets:");
  for (int i = 0; i < 7; ++i) {
    Logger::info("  {}: ({:3}, {:3}) {}", part_names[i],
                 part_debug_offsets_[i].dx, part_debug_offsets_[i].dy,
                 part_visible_[i] ? "" : "[HIDDEN]");
  }
  Logger::info("====================================");
}
