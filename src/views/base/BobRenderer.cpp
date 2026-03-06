/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "views/base/BobRenderer.h"

#include <vector>

#include "engine/Fatal.h"
#include "engine/geom/Spaces.h"
#include "resources/ResourceManager.h"

using BobManager = ResourceManager<Bob, BobLoader>;

BobRenderer::BobRenderer(const std::string& graphics_file, BobManager* bm,
                         std::optional<PaletteHandle> palette)
    : graphics_file_(graphics_file), palette_(palette),
      bob_(bm->get(graphics_file)) {}

Surface& BobRenderer::image(const BobRenderable& renderable) const {
  std::size_t bob_state = renderable.bob_state();
  const auto& bobs = renderable.bobs();

  if (bob_state >= bobs.size()) {
    fatal("Invalid render state");
  }
  int frame_index = bobs[bob_state];

  // Check if we already cached a Surface for this frame
  auto it = images_.find(frame_index);
  if (it == images_.end()) {
    auto bob_surface = bob_.frame(frame_index, palette_);
    auto result = images_.emplace(frame_index, std::move(bob_surface));
    return result.first->second;
  }
  return it->second;
}

void BobRenderer::render(const BobRenderable& renderable,
                         Surface& screen_surface, const Camera& cam) const {
  auto& bob_surface = image(renderable);

  int w = bob_surface.width();
  int h = bob_surface.height();

  // ViewPoint dst = renderable.origin() - origin;
  ViewPoint dst = cam.world_to_view(renderable.origin());

  if (renderable.anchor() == Anchor::Center) {
    dst.x -= (w / 2);
    dst.y -= (h / 2);
  }

  if ((dst.x + w) <= 0 || (dst.y + h) <= 0 ||
      (dst.x >= screen_surface.width()) || (dst.y >= screen_surface.height())) {
    return;
  }

  screen_surface.blit_masked(bob_surface, dst);
}

Size BobRenderer::size(const BobRenderable& renderable) const {
  std::size_t bob_state = renderable.bob_state();
  const auto& bobs = renderable.bobs();
  return bob_.size(bobs[bob_state]);
}
