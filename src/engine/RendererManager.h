/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/SDL_include.h"

class RendererManager {

public:
  static RendererManager& getInstance() {
    static RendererManager instance;
    return instance;
  }

  void setRenderer(SDL_Renderer* renderer) { renderer_ = renderer; }

  SDL_Renderer* getRenderer() const { return renderer_; }

private:
  SDL_Renderer* renderer_ = nullptr;
  RendererManager() {}

  // Prevent copying
  RendererManager(const RendererManager&) = delete;
  RendererManager& operator=(const RendererManager&) = delete;
};
