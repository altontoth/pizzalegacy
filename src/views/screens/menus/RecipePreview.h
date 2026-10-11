/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <filesystem>
#include <memory>
#include "core/food/Pizza.h"
#include "engine/RenderContext.h"
#include "views/base/BobRenderer.h"
#include "views/base/View.h"
#include "views/screens/pizza/HalfSizePizza.h"

// A recipe file's pizza at half size, drawn as its own layer so we don't have
// to worry about mismatched palettes.
class RecipePreview : public View {
public:
  static constexpr int size = pizza_design::half_size_pizza;

  RecipePreview(GameContext& context, GameState& state);

  void show_file(const std::filesystem::path& path);

  void tick() override {}
  void render(const RenderContext& ctx) override;

private:
  std::unique_ptr<core::food::Pizza> pizza_;
  BobRenderer renderer_;
};
