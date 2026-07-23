/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <vector>
#include "engine/LayerInfo.h"
#include "engine/RenderContext.h"

class Renderable {
public:
  virtual void render(const RenderContext& ctx) = 0;
  virtual bool is_dirty() const = 0;
  virtual void clean() = 0;
  virtual void mark_dirty() = 0;
  virtual bool needs_force_recompose() const = 0;
  virtual void tick() = 0;
  virtual std::vector<LayerInfo> get_layers() = 0;
  virtual bool preserve_background() const { return false; }
  // Composite at full brightness during palette fades.
  virtual bool ignore_fade() const { return false; }
  virtual ~Renderable() = default;
};
