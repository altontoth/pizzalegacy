/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/ui/PointerTracker.h"

#include <algorithm>

#include "engine/DisplayConstants.h"
#include "engine/ui/InteractionManager.h"

namespace views::ui {

void PointerTracker::on_motion(const SDL_MouseMotionEvent& motion) {
  position_ = ScreenPoint{
      std::clamp(static_cast<int>(motion.x), 0,
                 DisplayConstants::SCREEN_WIDTH - 1),
      std::clamp(static_cast<int>(motion.y), 0, display_height_ - 1)};
}

void PointerTracker::on_relative_motion(const SDL_MouseMotionEvent& motion) {
  position_.x += static_cast<int>(motion.xrel);
  position_.y += static_cast<int>(motion.yrel);
}

void PointerTracker::update_interaction() {
  interaction_.update_mouse(position_);
}

} // namespace views::ui
