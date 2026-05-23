/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/Renderer.h"

#include <cstring>
#include <span>
#include "engine/DisplayConstants.h"
#include "engine/Fatal.h"
#include "engine/Renderable.h"
#include "engine/SDL_include.h"
#include "engine/adapters/SDLRectAdapter.h"
#include "gfx/Surface.h"

using gfx::Surface;

void Renderer::add_layers(Renderable* r) {
  auto layers = r->get_layers();
  for (const auto& layer : layers) {
    layers_.push_back(layer);
  }
}

void Renderer::render() {
  // For now all layers have to be full screen size
  ensure_backbuffer(DisplayConstants::SCREEN_WIDTH,
                    DisplayConstants::SCREEN_HEIGHT);

  // During fade, clear cache so all layers regenerate with current fade_factor
  if (is_fading()) {
    cache_.clear();
    force_recompose_ = true;
  }

  // propagate force-dirty from layers
  for (auto& layer : layers_) {
    force_recompose_ |= layer.view->needs_force_recompose();
  }

  bool below_changed = force_recompose_;
  bool frame_changed = force_recompose_;

  for (auto& layer : layers_) {
    Renderable* view = layer.view;
    ViewRect r = layer.area;
    auto& label = layer.label;
    auto width = r.w;
    auto height = r.h;

    auto& layer_surface = layer_surfaces_[view];
    if (!layer_surface || layer_surface->width() != width ||
        layer_surface->height() != height) {
      layer_surface = std::make_unique<Surface>(width, height);
      view->mark_dirty();
    }

    SDL_Rect dest_rect = SDLAdapter::to_sdl_rect(r);

    bool rerender = view->is_dirty();
    bool recomposite = rerender || below_changed;

    if (rerender) {
      if (!layer.preserve_background) {
        layer_surface->fill(0);
        layer_surface->clear_palette_state();
      }

      RenderContext ctx{layer_surface.get()};
      view->render(ctx);
      update_cache(view, *layer_surface);
      view->clean();
    }
    if (recomposite) {
      auto it = cache_.find(view);
      if (it == cache_.end()) {
        if (!layer_surface) {
          layer_surface = std::make_unique<Surface>(width, height);
        }
        if (!layer.preserve_background) {
          layer_surface->fill(0);
          layer_surface->clear_palette_state();
        }
        RenderContext ctx{layer_surface.get()};
        view->render(ctx);

        update_cache(view, *layer_surface);
        it = cache_.find(view);
      }

      SDL_BlitSurface(it->second.get(), nullptr, backbuffer_.get(), &dest_rect);

      frame_changed = true;
      below_changed |= rerender;
    }
  }

  force_recompose_ = false;
  if (!frame_changed) {
    return;
  }

  SDL_Surface* final_surface = backbuffer_.get();
  SDL_Surface_unique_ptr scaled_surface;

  if (display_height_ == DisplayConstants::CRT_DISPLAY_HEIGHT) {
    scaled_surface = apply_crt_scaling(backbuffer_.get());
    final_surface = scaled_surface.get();
  }

  SDL_UpdateTexture(sdl_.texture, nullptr, final_surface->pixels,
                    final_surface->pitch);
  SDL_RenderClear(sdl_.renderer);
  SDL_RenderCopy(sdl_.renderer, sdl_.texture, nullptr, nullptr);
  SDL_RenderPresent(sdl_.renderer);
}

void Renderer::clear_layers() {
  layers_.clear();
  cache_.clear();
  force_recompose_ = true;
  if (backbuffer_) {
    SDL_FillRect(backbuffer_.get(), nullptr,
                 SDL_MapRGB(backbuffer_->format, 0, 0, 0));
  }
}

void Renderer::clear_layers_preserve_backbuffer() {
  layers_.clear();
  cache_.clear();
  force_recompose_ = true;
}

void Renderer::update_cache(Renderable* layer, const Surface& surface) {
  cache_[layer] = to_sdl_surface(surface, fade_factor_);
}

void Renderer::ensure_backbuffer(int w, int h) {
  if (backbuffer_ && backbuffer_->w == w && backbuffer_->h == h) {
    return;
  }

  backbuffer_.reset(
      SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA8888));

  if (!backbuffer_) {
    fatal(SDL_GetError());
  }
}

SDL_Surface_unique_ptr Renderer::to_sdl_surface(const Surface& src,
                                                float fade_factor) {
  const int w = src.width();
  const int h = src.height();

  SDL_Surface* index8 =
      SDL_CreateRGBSurfaceWithFormat(0, w, h, 8, SDL_PIXELFORMAT_INDEX8);
  if (!index8) {
    fatal(SDL_GetError());
  }

  // Build palette with fade applied (1.0 = normal, 0.0 = black)
  SDL_Color sdlPal[256];
  const auto pal = src.colors();
  for (size_t i = 0; i < pal.size(); ++i) {
    sdlPal[i] = {static_cast<uint8_t>(pal[i].r * fade_factor),
                 static_cast<uint8_t>(pal[i].g * fade_factor),
                 static_cast<uint8_t>(pal[i].b * fade_factor),
                 SDL_ALPHA_OPAQUE};
  }

  SDL_SetPaletteColors(index8->format->palette, sdlPal, 0, pal.size());
  SDL_SetColorKey(index8, SDL_TRUE, 0);
  SDL_SetSurfaceRLE(index8, 1);

  SDL_LockSurface(index8);
  uint8_t* dst = static_cast<uint8_t*>(index8->pixels);
  const uint8_t* raw_src = src.pixels().data();

  for (int y = 0; y < h; ++y) {
    std::memcpy(dst + y * index8->pitch, raw_src + y * w, w);
  }
  SDL_UnlockSurface(index8);

  // SDL does the palette to RGBA expansion
  SDL_Surface* rgba =
      SDL_ConvertSurfaceFormat(index8, SDL_PIXELFORMAT_RGBA8888, 0);
  SDL_FreeSurface(index8);

  if (!rgba) {
    fatal(SDL_GetError());
  }
  return SDL_Surface_unique_ptr(rgba);
}

void Renderer::with_current_frame(
    std::function<void(SDL_Surface*)> callback) const {
  if (display_height_ == DisplayConstants::CRT_DISPLAY_HEIGHT) {
    auto scaled = apply_crt_scaling(backbuffer_.get());
    callback(scaled.get());
  }
  else {
    callback(backbuffer_.get());
  }
}

SDL_Surface_unique_ptr Renderer::apply_crt_scaling(SDL_Surface* source) const {
  // 640x400 -> 640x1200 (nearest) -> 640x480 (bilinear)
  // Thanks to https://www.youtube.com/watch?v=YvckyWxHAIw (DOS Gaming Aspect
  // Ratio - 320x200) which talks about why this is a good way to do it
  const int INTERMEDIATE_HEIGHT = DisplayConstants::LOGICAL_HEIGHT * 3;
  const int source_width = source->w;
  const int source_height = source->h;

  // Stage 1: nearest-neighbor 3x vertical upscale
  SDL_Surface* intermediate = SDL_CreateRGBSurfaceWithFormat(
      0, source_width, INTERMEDIATE_HEIGHT, 32, SDL_PIXELFORMAT_RGBA8888);
  if (!intermediate) {
    fatal(SDL_GetError());
  }

  SDL_LockSurface(source);
  SDL_LockSurface(intermediate);

  auto* src_pixels = static_cast<uint32_t*>(source->pixels);
  auto* int_pixels = static_cast<uint32_t*>(intermediate->pixels);

  for (int y = 0; y < source_height; ++y) {
    for (int dy = 0; dy < 3; ++dy) {
      int dest_y = (y * 3) + dy;
      for (int x = 0; x < source_width; ++x) {
        int_pixels[(dest_y * source_width) + x] =
            src_pixels[(y * source_width) + x];
      }
    }
  }

  SDL_UnlockSurface(intermediate);
  SDL_UnlockSurface(source);

  // Stage 2: bilinear downscale to display height
  SDL_Surface* final = SDL_CreateRGBSurfaceWithFormat(
      0, source_width, display_height_, 32, SDL_PIXELFORMAT_RGBA8888);
  if (!final) {
    SDL_FreeSurface(intermediate);
    fatal(SDL_GetError());
  }

  SDL_Rect int_rect = {0, 0, source_width, INTERMEDIATE_HEIGHT};
  SDL_Rect final_rect = {0, 0, source_width, display_height_};
  SDL_SoftStretchLinear(intermediate, &int_rect, final, &final_rect);

  SDL_FreeSurface(intermediate);

  return SDL_Surface_unique_ptr(final);
}

bool Renderer::tick_fade() {
  if (fade_state_ == FadeState::NONE) {
    return false;
  }

  fade_elapsed_frames_++;

  if (fade_elapsed_frames_ >= fade_duration_frames_) {
    if (fade_state_ == FadeState::FADING_OUT) {
      fade_factor_ = 0.0f;
    }
    else if (fade_state_ == FadeState::FADING_IN) {
      fade_factor_ = 1.0f;
      fade_state_ = FadeState::NONE;
    }
    force_recompose_ = true;
    return true;
  }

  float progress = static_cast<float>(fade_elapsed_frames_) /
                   static_cast<float>(fade_duration_frames_);

  if (fade_state_ == FadeState::FADING_OUT) {
    fade_factor_ = 1.0f - progress;
  }
  else if (fade_state_ == FadeState::FADING_IN) {
    fade_factor_ = progress;
  }

  force_recompose_ = true;
  return false;
}

void Renderer::start_fade_out(int duration_frames) {
  fade_state_ = FadeState::FADING_OUT;
  fade_duration_frames_ = duration_frames;
  fade_elapsed_frames_ = 0;
  fade_factor_ = 1.0f;
  force_recompose_ = true;
}

void Renderer::start_fade_in(int duration_frames) {
  fade_state_ = FadeState::FADING_IN;
  fade_duration_frames_ = duration_frames;
  fade_elapsed_frames_ = 0;
  fade_factor_ = 0.0f;
  force_recompose_ = true;
}

bool Renderer::is_fading() const { return fade_state_ != FadeState::NONE; }

bool Renderer::fade_out_complete() const {
  return fade_state_ == FadeState::FADING_OUT && fade_factor_ <= 0.0f;
}
