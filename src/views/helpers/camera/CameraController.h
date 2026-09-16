/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>

#include "engine/DebugLabel.h"
#include "engine/geom/Camera.h"
#include "engine/geom/Spaces.h"
#include "engine/input/InputManager.h"

enum class ScrollEdge { Left, Right, Top, Bottom };

/*
 * CameraController
 *
 * If you have a view of something that is bigger than the 'screen' (viewport),
 * you need to be able to scroll to see all content. This class handles that.
 * Create it by specifing an x and y position (of the current viewport), a width
 * and height of the viewport, and a max_x and max_y, specifying the size of the
 * view that can be scrolled, and finally how much you want to scroll by. For
 * example, if you have something 500 x 400 with a view that is only 100 x 80
 * and they are 10 x 8 tiles (so you don't want to show half a tile), you call
 * CameraController(0,0,100,80,500,400,10,8).
 */

class CameraController : public input::InputListener,
                         public debug::AutoLabel<CameraController> {
public:
  // Construct with an input manager and the camera to drive.
  // The camera remains the single source of truth for geometry/state.
  CameraController(input::InputManager* input, Camera& camera) noexcept;

  bool tick();

  // Define tile/grid size in *world pixels* for snapping and per-tile stepping.
  // Use tile_w/tile_h > 0 to enable snapping per axis; set to 0 to disable.
  void tile_grid(int tile_w, int tile_h) noexcept;

  // Set sub-steps per tile while moving (higher => slower/smoother).
  // e.g., 8 means base step =~ tile/8 per tick.
  void substeps_per_tile(int substeps) noexcept;

  // Set speed multipliers (world px per tick scaling).
  // 'normal' is the baseline; holding Shift applies 'fast' multiplier.
  void speed_multipliers(float normal_mult, float fast_mult) noexcept;

  // Minimal step size (px/tick) to avoid getting stuck with tiny axis input.
  void min_step_px(float min_px) noexcept;

  void edge_active(ScrollEdge e, bool on) noexcept;
  void clear_edges() noexcept;

  // Scroll the camera by (dx, dy) screen pixels, scaled by the camera zoom
  // factor. Syncs internal accumulators so the next tick() doesn't fight it.
  // inverted=true means drag-the-map style: drag right scrolls left.
  // inverted=false means swipe style: drag right scrolls right. Returns true if
  // the camera scroll position actually changed
  [[nodiscard]] bool apply_drag_delta(int dx, int dy, bool inverted) noexcept;

  // Glide the camera to `target` over `ticks` ticks, snapped to the tile grid.
  // Scroll input cancels it.
  void glide_to(WorldPoint target, int ticks) noexcept;

  // Reset controller internal state (after view/world resize, map swap, etc.).
  // Does not change camera; it re-syncs to camera.scroll().
  void reset() noexcept;

  bool on_text(const input::TextEvent& /*ev*/) override { return false; }
  bool on_action(const input::ActionEvent& /*ev*/) override { return false; }

private:
  static float step_towards(float cur, float target, float step) noexcept;
  static float tile_align_target(float pos, int view_len, int world_len,
                                 int tile, int last_dir) noexcept;

  // tick() sub-routines: active motion and idle alignment
  void tick_active(float dx, float dy, float x_vel, float y_vel, float max_xf,
                   float max_yf, WorldPoint& scroll);
  void tick_glide(WorldPoint& scroll);
  void tick_idle(float base_step_x, float base_step_y, float max_xf,
                 float max_yf, WorldPoint& scroll);

  input::InputManager* input_{nullptr};
  Camera& camera_;

  // Edge scroll inputs
  bool edge_left_{false};
  bool edge_right_{false};
  bool edge_top_{false};
  bool edge_bottom_{false};

  // Controller config (world-space)
  int tile_w_{0};
  int tile_h_{0};
  int substeps_per_tile_{4};
  float normal_mult_{1.5f};
  float fast_mult_{3.0f};
  float min_step_px_{1.0f};

  // Keep higher-precision accumulators in world px (floats).
  float precise_x_{0.0f};
  float precise_y_{0.0f};

  // Last input direction per axis: {-1, 0, +1}; used by align rounding policy.
  int last_dir_x_{0};
  int last_dir_y_{0};

  // Auto-align state after user stops
  bool aligning_{false};
  float align_target_x_{0.0f};
  float align_target_y_{0.0f};

  // Stop threshold for alignment (in world px)
  static constexpr float align_epsilon_{0.25f};

  // Carry over last per-tick step so align continues seamlessly.
  float last_step_px_x_{0.0f};
  float last_step_px_y_{0.0f};

  // Detect boundaries between "had input this frame" and "idle"
  bool had_user_input_{false};

  struct Glide {
    float from_x;
    float from_y;
    float to_x;
    float to_y;
    int tick;
    int ticks;
  };
  std::optional<Glide> glide_;

  input::ListenerToken listener_;
};
