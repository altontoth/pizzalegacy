/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <optional>
#include <random>
#include <vector>
#include "core/city/Car.h"
#include "engine/geom/Camera.h"
#include "engine/geom/Spaces.h"
#include "resources/BobManager.h"

class City;
struct RenderContext;

constexpr int MAX_CAR_TYPES = 4;
constexpr int FRAMES_PER_CAR = 4;
constexpr int TOTAL_FRAMES = MAX_CAR_TYPES * FRAMES_PER_CAR;

class CarManager {
public:
  CarManager(BobManager* bob, const City& city, std::function<int(int)> rng);

  void update(const WorldRect& view, int ticks = 1);
  void render(const RenderContext& ctx, const Camera& camera) const;

  Car& inject_car(const Car& c);
  std::size_t active_count() const { return cars_.size(); }

private:
  void resolve_collisions();
  void spawn_cars_in_region(const WorldRect& region, int traffic_density);
  std::vector<WorldRect>
  calculate_newly_visible_regions(const WorldRect& prev_view,
                                  const WorldRect& curr_view) const;
  [[nodiscard]] bool cars_would_collide(const Car& a, const Car& b) noexcept;

  const City& city_;
  std::vector<Car> cars_;
  std::function<int(int)> rng_;
  Bob& stanbobs_;
  std::mt19937 shuffle_rng_;
  std::vector<Surface> car_surfaces_;
  std::optional<WorldRect> prev_viewport_;
};
