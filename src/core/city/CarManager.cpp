/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/city/CarManager.h"

#include <algorithm>

#include "core/city/City.h"
#include "core/city/District.h"
#include "engine/Random.h"
#include "engine/RenderContext.h"
#include "engine/geom/Spaces.h"

static int floor_div(int value, int denom) {
  return (value >= 0) ? value / denom : -((-value + denom - 1) / denom);
}

CarManager::CarManager(BobManager* bob, const City& city)
    : city_(city), stanbobs_(bob->get("bob/stanbobs.bob")) {

  car_surfaces_.reserve(TOTAL_FRAMES);
  for (int i = 0; i < TOTAL_FRAMES; ++i) {
    car_surfaces_.emplace_back(stanbobs_.frame(i, PaletteHandle{0}));
  }
}

Car& CarManager::inject_car(const Car& c) {
  cars_.push_back(c);
  return cars_.back();
}

void CarManager::update(const WorldRect& view, int ticks) {
  for (int t = 0; t < ticks; ++t) {
    resolve_collisions();
    for (Car& c : cars_) {
      c.update(view, city_);
    }
    constexpr int margin = CAR_KEEPALIVE_MARGIN_PIXELS;
    std::erase_if(cars_, [&](const Car& c) {
      return c.position().x < view.x - margin ||
             c.position().x > view.x + view.w + margin ||
             c.position().y < view.y - margin ||
             c.position().y > view.y + view.h + margin;
    });
  }

  // Calculate newly visible regions and spawn cars there based on district
  // traffic density
  std::vector<WorldRect> newly_visible_regions;

  if (prev_viewport_.has_value()) {
    newly_visible_regions =
        calculate_newly_visible_regions(prev_viewport_.value(), view);
  }
  else {
    // First time - entire viewport is new
    newly_visible_regions.push_back(view);
  }

  // A region equal to the whole viewport means we're spawning across the
  // entire view rather than into a newly revealed strip. The original
  // halves the spawn chance in that case.
  const bool covers_full_viewport =
      newly_visible_regions.size() == 1 && newly_visible_regions[0] == view;

  // Spawn cars in newly visible regions based on district traffic density
  for (const auto& region : newly_visible_regions) {
    // Find the center tile of the region to determine district, I believe this
    // is what the original does too
    const int center_tile_x = (region.x + region.w / 2) / tile_width_pixels;
    const int center_tile_y = (region.y + region.h / 2) / tile_width_pixels;
    const int center_index =
        (center_tile_y * MAP_TILES_PER_ROW) + center_tile_x;

    // Get district and traffic density
    const auto district_index = city_.district_by_index(center_index);
    const int traffic_density =
        city_.district(district_index).traffic_density();

    spawn_cars_in_region(region, covers_full_viewport ? traffic_density / 2
                                                      : traffic_density);
  }

  prev_viewport_ = view;
}

void CarManager::render(const RenderContext& ctx, const Camera& camera) const {
  const WorldRect visible = camera.visible_world_rect();

  std::vector<const Car*> list;
  list.reserve(cars_.size());

  constexpr int sprite_margin = 32;

  for (const Car& c : cars_) {
    const WorldPoint car_world_pos = c.position();

    // Culling with margin - consider cars slightly outside visible area
    // so they don't disappear until sprite is fully off-screen
    if (car_world_pos.x >= visible.x - sprite_margin &&
        car_world_pos.x < visible.x + visible.w + sprite_margin &&
        car_world_pos.y >= visible.y - sprite_margin &&
        car_world_pos.y < visible.y + visible.h + sprite_margin) {
      list.push_back(&c);
    }
  }

  // Sort by world Y for proper depth ordering
  std::ranges::stable_sort(
      list, [](auto* a, auto* b) { return a->position().y < b->position().y; });

  for (const Car* c : list) {
    int frame = (c->type() * 4) + static_cast<int>(c->dir());

    const ViewPoint dest_pos = camera.world_to_view(c->position());

    ctx.surface->blit_masked(car_surfaces_[frame], dest_pos);
  }
}

void CarManager::resolve_collisions() {
  //  Build a list of pointers, sorted by y coord ascending
  std::vector<Car*> sorted;
  sorted.reserve(cars_.size());
  for (auto& c : cars_) {
    sorted.push_back(&c);
  }

  std::ranges::sort(
      sorted, [](Car* a, Car* b) { return a->position().y < b->position().y; });

  // Now resolve collisions in that y order,
  // each car only checks against those earlier in sorted
  constexpr int MAX_Y_DIFF = 32 + 20; // sprite height + worst 20px overlap
  for (std::size_t i = 0; i < sorted.size(); ++i) {
    Car* a = sorted[i];

    // scan backward until y difference too large
    for (std::size_t j = i; j-- > 0;) {
      Car* b = sorted[j];
      int dy = a->position().y - b->position().y;
      if (dy > MAX_Y_DIFF) {
        // any earlier cars are now too far above to collide
        break;
      }

      if (cars_would_collide(*a, *b)) {
        a->remaining_cooldown(10);
        break;
      }
    }
  }
}

bool CarManager::cars_would_collide(const Car& a, const Car& b) noexcept {
  if (&a == &b) {
    return false;
  }

  const int ax = a.position().x;
  const int ay = a.position().y;
  const int bx = b.position().x;
  const int by = b.position().y;

  const int dx = bx - ax; // +X -> right
  const int dy = by - ay; // +Y -> down

  const auto same_lane_y = [&] { return ay == by; };
  const auto same_lane_x = [&] { return ax == bx; };

  switch (a.dir()) {
  case Dir::E:
    if (b.dir() == Dir::E) {
      return same_lane_y() && (dx > 0) && (dx < 22);
    }

    if (b.dir() == Dir::N) {
      const int lane = bx - ax;
      return (lane > 20 && lane < 30) && (std::abs(by - ay) <= 20);
    }
    // East vs South has no collision in original
    break;

  case Dir::W:
    if (b.dir() == Dir::W) {
      return same_lane_y() && (dx < 0) && (-dx < 22);
    }

    if (b.dir() == Dir::N) {
      const int lane = bx - ax;
      return (lane > -20 && lane < -10) && (std::abs(by - ay) < 12);
    }
    // West vs South has no collision in original
    break;

  case Dir::N:
    if (b.dir() == Dir::N) {
      return same_lane_x() && (dy < 0) && (-dy < 22);
    }

    if (b.dir() == Dir::E || b.dir() == Dir::W) {
      const int lane = by - ay;
      return (lane > -20 && lane < -10) && (std::abs(bx - ax) < 20);
    }
    break;

  case Dir::S:
    if (b.dir() == Dir::S) {
      return same_lane_x() && (dy > 0) && (dy < 22);
    }

    if (b.dir() == Dir::E || b.dir() == Dir::W) {
      const int lane = by - ay; // row - column
      return (lane > 20 && lane < 28) && (std::abs(bx - ax) < 20);
    }
    break;
  }

  return false;
}

void CarManager::spawn_cars_in_region(const WorldRect& region,
                                      int traffic_density) {
  // Convert region to tile rectangle
  const int tile_left = std::max(0, floor_div(region.x, tile_width_pixels));
  const int tile_top = std::max(0, floor_div(region.y, tile_width_pixels));
  const int tile_right = std::min((region.x + region.w - 1) / tile_width_pixels,
                                  MAP_TILES_PER_ROW - 1);
  const int tile_bottom = std::min(
      (region.y + region.h - 1) / tile_width_pixels, MAP_TILES_PER_COLUMN - 1);

  // Each road tile independently rolls to spawn a car, same as the original:
  // traffic_density is the percentage chance per tile.
  for (int ty = tile_top; ty <= tile_bottom; ++ty) {
    for (int tx = tile_left; tx <= tile_right; ++tx) {
      int idx = (ty * MAP_TILES_PER_ROW) + tx;
      if (!Car::spawn_mask(city_.type(idx))) {
        continue;
      }
      if (engine::random::presentation::rand_percentage_chance(
              traffic_density)) {
        int type = engine::random::presentation::rand_max_n(MAX_CAR_TYPES);
        cars_.push_back(Car::spawn(tx, ty, type, city_));
      }
    }
  }
}

std::vector<WorldRect>
CarManager::calculate_newly_visible_regions(const WorldRect& prev_view,
                                            const WorldRect& curr_view) const {
  std::vector<WorldRect> regions;

  if (prev_view == curr_view) {
    return regions;
  }

  if (!curr_view.intersects(prev_view)) {
    // No overlap - entire current view is new
    regions.push_back(curr_view);
    return regions;
  }

  // Left strip (scrolled left)
  if (curr_view.x < prev_view.x) {
    regions.emplace_back(curr_view.x, curr_view.y, prev_view.x - curr_view.x,
                         curr_view.h);
  }

  // Right strip (scrolled right)
  if (curr_view.right() > prev_view.right()) {
    regions.emplace_back(prev_view.right(), curr_view.y,
                         curr_view.right() - prev_view.right(), curr_view.h);
  }

  // Top and bottom strips only need the x-range the left/right strips
  // skipped: where prev_view and curr_view overlap horizontally.
  const WorldRect overlap = intersect(prev_view, curr_view);

  // Top strip (scrolled up)
  if (curr_view.y < prev_view.y) {
    regions.emplace_back(overlap.x, curr_view.y, overlap.w,
                         prev_view.y - curr_view.y);
  }

  // Bottom strip (scrolled down)
  if (curr_view.bottom() > prev_view.bottom()) {
    regions.emplace_back(overlap.x, prev_view.bottom(), overlap.w,
                         curr_view.bottom() - prev_view.bottom());
  }

  return regions;
}
