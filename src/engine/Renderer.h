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
#include "gfx/Blit.h"
#include "gfx/Surface.h"

class Renderable;

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

  // Fade control - returns true when fade completes
  bool tick_fade();
  void start_fade_out(int duration_frames);
  void start_fade_in(int duration_frames);
  [[nodiscard]] bool is_fading() const;
  [[nodiscard]] bool fade_out_complete() const;

private:
  void update_cache(Renderable* layer, const gfx::Surface& source_surface);
  void ensure_backbuffer(int w, int h);
  SDL_Surface_unique_ptr apply_crt_scaling(SDL_Surface* source);

  std::unordered_map<Renderable*, SDL_Surface_unique_ptr> cache_;
  SDL_Surface_unique_ptr backbuffer_;
  SDL_Surface_unique_ptr intermediate_buffer_;
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
