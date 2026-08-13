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

  // Spawn cars in newly visible regions based on district traffic density
  for (const auto& region : newly_visible_regions) {
    // Find the center tile of the region to determine district, I believe this
    // is what the original does too
    const int center_tile_x = (region.x + region.w / 2) / tile_width_pixels;
    const int center_tile_y = (region.y + region.h / 2) / tile_width_pixels;
    const int center_index =
        (center_tile_y * MAP_TILES_PER_ROW) + center_tile_x;

    // Get district and traffic density
    const auto district_id = city_.district_by_index(center_index);
    const int traffic_density = city_.district(district_id).traffic_density();

    spawn_cars_in_region(region, traffic_density);
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
  struct SpawnSite {
    int tx;
    int ty;
    // allowed exits (E=1,W=2,S=4,N=8):
    uint8_t lane_mask;
  };

  // Calculate target number of cars based on traffic density and region size
  // traffic_density is 0-100, where 50 is average
  // Formula: scale density so that:
  //   - density=0:   0 cars
  //   - density=50:  ~1 car per 3-4 road tiles
  //   - density=100: ~2 cars per 3-4 road tiles
  const int region_tiles = ((region.w / tile_width_pixels) + 1) *
                           ((region.h / tile_width_pixels) + 1);
  // TODO: For now assume roughly 30% of tiles are roads , but we could just
  // calculate this?
  const int estimated_road_tiles = region_tiles * 3 / 10;
  const int target_cars =
      std::max(0, (estimated_road_tiles * traffic_density) / 150);

  // Convert region to tile rectangle
  const int tile_left = std::max(0, floor_div(region.x, tile_width_pixels));
  const int tile_top = std::max(0, floor_div(region.y, tile_width_pixels));
  const int tile_right = std::min((region.x + region.w - 1) / tile_width_pixels,
                                  MAP_TILES_PER_ROW - 1);
  const int tile_bottom = std::min(
      (region.y + region.h - 1) / tile_width_pixels, MAP_TILES_PER_COLUMN - 1);

  // Gather candidate road tiles in region
  std::vector<SpawnSite> sites;
  for (int ty = tile_top; ty <= tile_bottom; ++ty) {
    for (int tx = tile_left; tx <= tile_right; ++tx) {
      int idx = (ty * MAP_TILES_PER_ROW) + tx;
      uint8_t lanes = Car::spawn_mask(city_.type(idx));
      if (lanes) {
        sites.push_back({tx, ty, lanes});
      }
    }
  }

  if (sites.empty()) {
    return;
  }

  // Shuffle & spawn up to target_cars
  engine::random::presentation::shuffle(sites.begin(), sites.end());

  const std::size_t spawn_count =
      std::min(static_cast<std::size_t>(target_cars), sites.size());
  for (std::size_t i = 0; i < spawn_count; ++i) {
    const auto& s = sites[i];
    int type = engine::random::presentation::rand_max_n(MAX_CAR_TYPES);
    cars_.push_back(Car::spawn(s.tx, s.ty, type, city_));
  }
}

std::vector<WorldRect>
CarManager::calculate_newly_visible_regions(const WorldRect& prev_view,
                                            const WorldRect& curr_view) const {
  std::vector<WorldRect> regions;

  // Check if viewports are identical
  if (prev_view.x == curr_view.x && prev_view.y == curr_view.y &&
      prev_view.w == curr_view.w && prev_view.h == curr_view.h) {
    return regions; // No change, return empty
  }

  // Calculate bounds
  const int prev_left = prev_view.x;
  const int prev_right = prev_view.x + prev_view.w;
  const int prev_top = prev_view.y;
  const int prev_bottom = prev_view.y + prev_view.h;

  const int curr_left = curr_view.x;
  const int curr_right = curr_view.x + curr_view.w;
  const int curr_top = curr_view.y;
  const int curr_bottom = curr_view.y + curr_view.h;

  // Check for completely non-overlapping viewports (jumped to different
  // location)
  if (curr_right <= prev_left || curr_left >= prev_right ||
      curr_bottom <= prev_top || curr_top >= prev_bottom) {
    // No overlap - entire current view is new
    regions.push_back(curr_view);
    return regions;
  }

  // Calculate newly visible strips
  // Left strip (scrolled left)
  if (curr_left < prev_left) {
    regions.emplace_back(curr_left, curr_top, prev_left - curr_left,
                         curr_view.h);
  }

  // Right strip (scrolled right)
  if (curr_right > prev_right) {
    regions.emplace_back(prev_right, curr_top, curr_right - prev_right,
                         curr_view.h);
  }

  // Top strip (scrolled up)
  // Only the part not already covered by left/right strips
  if (curr_top < prev_top) {
    const int strip_left = std::max(curr_left, prev_left);
    const int strip_right = std::min(curr_right, prev_right);
    if (strip_right > strip_left) {
      regions.emplace_back(strip_left, curr_top, strip_right - strip_left,
                           prev_top - curr_top);
    }
  }

  // Bottom strip (scrolled down)
  // Only the part not already covered by left/right strips
  if (curr_bottom > prev_bottom) {
    const int strip_left = std::max(curr_left, prev_left);
    const int strip_right = std::min(curr_right, prev_right);
    if (strip_right > strip_left) {
      regions.emplace_back(strip_left, prev_bottom, strip_right - strip_left,
                           curr_bottom - prev_bottom);
    }
  }

  return regions;
}
