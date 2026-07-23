/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/SDL_include.h"

#include <unordered_map>
#include <vector>
#include "engine/LayerInfo.h"
#include "engine/SDLContext.h"
#include "engine/geom/Spaces.h"
#include "gfx/Blit.h"
#include "gfx/Surface.h"

class Renderable;
class CursorManager;

class Renderer {
public:
  enum class FadeState { NONE, FADING_OUT, FADING_IN };

  Renderer(const SDLContext& sdl, int display_height)
      : sdl_(sdl), display_height_(display_height) {}
  void add_layers(Renderable* r);
  void render();
  void clear_layers();
  void clear_layers_preserve_backbuffer();
  static SDL_Surface_unique_ptr to_sdl_surface(const Surface& surface,
                                               float fade_factor = 1.0f);
  void with_current_frame(std::function<void(SDL_Surface*)> callback) const;
  void set_display_height(int height) { display_height_ = height; }
  void set_cursor_manager(CursorManager* cursor) { cursor_ = cursor; }

  // Fade control - returns true when fade completes
  bool tick_fade();
  void start_fade_out(int duration_frames);
  void start_fade_in(int duration_frames);
  [[nodiscard]] bool is_fading() const;
  [[nodiscard]] bool fade_out_complete() const;

private:
  void update_cache(Renderable* layer, const gfx::Surface& source_surface);
  void ensure_backbuffer(int w, int h);
  // Returns true when the surface was (re)created.
  bool ensure_present_surface(int w, int h);
  // Returns the rect the cursor was drawn into.
  SDL_Rect draw_software_cursor(SDL_Surface* dst, const gfx::Surface& sprite,
                                ScreenPoint pos, bool centered) const;
  SDL_Surface* apply_crt_scaling(SDL_Surface* source) const;

  std::unordered_map<Renderable*, SDL_Surface_unique_ptr> cache_;
  SDL_Surface_unique_ptr backbuffer_;
  // Copy with the software cursor on top
  SDL_Surface_unique_ptr frame_with_cursor_;
  // Reused by apply_crt_scaling
  mutable SDL_Surface_unique_ptr crt_intermediate_;
  mutable SDL_Surface_unique_ptr crt_scaled_;
  CursorManager* cursor_{nullptr};
  // This allows us to re-present when only the cursor changed.
  const gfx::Surface* last_cursor_sprite_{nullptr};
  ScreenPoint last_cursor_pos_{};
  SDL_Rect last_cursor_rect_{};
  bool force_recompose_ = false;
  std::unordered_map<Renderable*, std::unique_ptr<Surface>> layer_surfaces_;
  std::vector<LayerInfo> layers_;
  const SDLContext& sdl_;
  int display_height_;

  FadeState fade_state_{FadeState::NONE};
  float fade_factor_{1.0f}; // 1.0 = normal, 0.0 = black
  int fade_duration_frames_{0};
  int fade_elapsed_frames_{0};
};
