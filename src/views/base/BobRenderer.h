/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <string>
#include <unordered_map>

#include "engine/BobRenderable.h"
#include "engine/geom/Camera.h"
#include "resources/Bob.h"
#include "resources/BobLoader.h"
#include "resources/ResourceManager.h"

using BobManager = ResourceManager<Bob, BobLoader>;

class BobRenderer {
public:
  BobRenderer(const std::string& graphics_file, BobManager* bm,
              std::optional<PaletteHandle> palette = std::nullopt);
  Surface& image(const BobRenderable& renderable) const;
  void render(const BobRenderable& renderable, Surface& screen_surface,
              const Camera& cam) const;
  Size size(const BobRenderable& renderable) const;

private:
  std::string graphics_file_;
  std::optional<PaletteHandle> palette_;
  mutable std::unordered_map<int, Surface> images_;
  Bob& bob_;
};
