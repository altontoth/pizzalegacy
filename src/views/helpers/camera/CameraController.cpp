/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/helpers/camera/CameraController.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>

#include "engine/geom/Camera.h"
#include "engine/geom/Spaces.h"
#include "engine/input/InputManager.h"

constexpr float my_abs(float value) noexcept {
  return value < 0.f ? -value : value;
}

// Move `cur` toward `target` by `step` per tick.
constexpr float CameraController::step_towards(float cur, float target,
                                               float step) noexcept {
  const float d = target - cur;
  if (my_abs(d) <= step) {
    return target;
  }
  return cur + (d > 0.f ? step : -step);
}

// Snap a scroll position (in world px) toward the nearest tile edge.
constexpr float CameraController::tile_align_target(float pos, int view_len,
                                                    int world_len, int tile,
                                                    int last_dir) noexcept {
  if (tile <= 0) {
    // no snapping
    const int max_extent = std::max(0, world_len - view_len);
    return std::clamp(pos, 0.f, static_cast<float>(max_extent));
  }

  const int hi_i = std::max(0, world_len - view_len);
  const auto hi = static_cast<float>(hi_i);
  const auto t = static_cast<float>(tile);

  float target;
  if (last_dir > 0) {
    target = std::ceil(pos / t) * t;
  }
  else if (last_dir < 0) {
    target = std::floor(pos / t) * t;
  }
  else {
    target = std::round(pos / t) * t;
  }

  return std::clamp(target, 0.f, hi);
}

CameraController::CameraController(input::InputManager* input,
                                   Camera& camera) noexcept
    : input_(input), camera_(camera),
      listener_(input_->register_listener(*this)) {
  precise_x_ = static_cast<float>(camera_.scroll().x);
  precise_y_ = static_cast<float>(camera_.scroll().y);
}

void CameraController::tile_grid(int tile_w, int tile_h) noexcept {
  tile_w_ = tile_w;
  tile_h_ = tile_h;
}

void CameraController::substeps_per_tile(int substeps) noexcept {
  substeps_per_tile_ = std::max(1, substeps);
}

void CameraController::speed_multipliers(float normal_mult,
                                         float fast_mult) noexcept {
  normal_mult_ = std::max(0.f, normal_mult);
  fast_mult_ = std::max(normal_mult_, fast_mult);
}

void CameraController::min_step_px(float min_px) noexcept {
  min_step_px_ = std::max(0.f, min_px);
}

void CameraController::edge_active(ScrollEdge e, bool on) noexcept {
  switch (e) {
  case ScrollEdge::Left:
    edge_left_ = on;
    break;
  case ScrollEdge::Right:
    edge_right_ = on;
    break;
  case ScrollEdge::Top:
    edge_top_ = on;
    break;
  case ScrollEdge::Bottom:
    edge_bottom_ = on;
    break;
  }
}

void CameraController::clear_edges() noexcept {
  edge_left_ = edge_right_ = edge_top_ = edge_bottom_ = false;
}

void CameraController::reset() noexcept {
  precise_x_ = static_cast<float>(camera_.scroll().x);
  precise_y_ = static_cast<float>(camera_.scroll().y);
  aligning_ = false;
  last_dir_x_ = last_dir_y_ = 0;
  last_step_px_x_ = last_step_px_y_ = 0.f;
  had_user_input_ = false;
}

bool CameraController::tick() {
  // Get current world sizes and scroll
  const Size view = camera_.view_size();
  const Size world = camera_.world_size();
  WorldPoint scroll = camera_.scroll();

  const float max_xf = static_cast<float>(std::max(0, world.w - view.w));
  const float max_yf = static_cast<float>(std::max(0, world.h - view.h));

  // Input axes
  const auto [ax, ay] = input_->axis(input::Action::ScrollVec2);
  float x_vel = ax;
  float y_vel = ay;

  if (edge_left_) {
    x_vel -= 1.0f;
  }
  if (edge_right_) {
    x_vel += 1.0f;
  }
  if (edge_top_) {
    y_vel -= 1.0f;
  }
  if (edge_bottom_) {
    y_vel += 1.0f;
  }

  // Speed multiplier
  const bool fast = input_->modifier_down(input::Mod::Shift);
  const float mult = fast ? fast_mult_ : normal_mult_;

  // Step base (tile/substeps) in world px
  const float base_step_x =
      std::max(min_step_px_, static_cast<float>(tile_w_) /
                                 static_cast<float>(substeps_per_tile_));
  const float base_step_y =
      std::max(min_step_px_, static_cast<float>(tile_h_) /
                                 static_cast<float>(substeps_per_tile_));

  const float dx = x_vel * base_step_x * mult;
  const float dy = y_vel * base_step_y * mult;

  const bool has_user_input = (dx != 0.0f) || (dy != 0.0f);

  const int old_x = scroll.x;
  const int old_y = scroll.y;

  if (has_user_input) {
    // user-driven motion cancels align
    aligning_ = false;
    precise_x_ = std::clamp(precise_x_ + dx, 0.0f, max_xf);
    precise_y_ = std::clamp(precise_y_ + dy, 0.0f, max_yf);

    scroll.x = static_cast<int>(std::lround(precise_x_));
    scroll.y = static_cast<int>(std::lround(precise_y_));
    camera_.scroll(scroll);

    if (x_vel != 0.0f) {
      last_dir_x_ = (x_vel > 0.0f) ? 1 : -1;
    }
    if (y_vel != 0.0f) {
      last_dir_y_ = (y_vel > 0.0f) ? 1 : -1;
    }

    last_step_px_x_ =
        std::abs(dx) > 0.f ? std::abs(dx) : base_step_x * std::abs(x_vel);
    last_step_px_y_ =
        std::abs(dy) > 0.f ? std::abs(dy) : base_step_y * std::abs(y_vel);

    had_user_input_ = true;
  }
  else {
    // idle: possibly start or continue alignment
    if (!aligning_) {
      if (had_user_input_) {
        align_target_x_ = tile_align_target(precise_x_, view.w, world.w,
                                            tile_w_, last_dir_x_);
        align_target_y_ = tile_align_target(precise_y_, view.h, world.h,
                                            tile_h_, last_dir_y_);

        const bool need_x =
            std::abs(align_target_x_ - precise_x_) > align_epsilon_;
        const bool need_y =
            std::abs(align_target_y_ - precise_y_) > align_epsilon_;
        aligning_ = (need_x || need_y);

        if (need_x && last_step_px_x_ <= 0.f) {
          last_step_px_x_ = base_step_x;
        }
        if (need_y && last_step_px_y_ <= 0.f) {
          last_step_px_y_ = base_step_y;
        }
      }
    }

    if (aligning_) {
      precise_x_ =
          std::clamp(step_towards(precise_x_, align_target_x_, last_step_px_x_),
                     0.0f, max_xf);
      precise_y_ =
          std::clamp(step_towards(precise_y_, align_target_y_, last_step_px_y_),
                     0.0f, max_yf);

      scroll.x = static_cast<int>(std::lround(precise_x_));
      scroll.y = static_cast<int>(std::lround(precise_y_));
      camera_.scroll(scroll);

      const bool done_x =
          std::abs(align_target_x_ - precise_x_) <= align_epsilon_;
      const bool done_y =
          std::abs(align_target_y_ - precise_y_) <= align_epsilon_;
      if (done_x && done_y) {
        // finish on exact grid to avoid drift
        const int snapped_x = static_cast<int>(align_target_x_);
        const int snapped_y = static_cast<int>(align_target_y_);
        precise_x_ = static_cast<float>(snapped_x);
        precise_y_ = static_cast<float>(snapped_y);
        scroll.x = snapped_x;
        scroll.y = snapped_y;
        camera_.scroll(scroll);
        aligning_ = false;
      }
    }

    had_user_input_ = false;
  }

  return (scroll.x != old_x) || (scroll.y != old_y);
}
