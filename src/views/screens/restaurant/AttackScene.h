/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <optional>
#include <utility>

#include "core/finance/Money.h"
#include "core/item/Item.h"
#include "engine/geom/Spaces.h"
#include "views/screens/restaurant/Walk.h"

namespace core::character {
class Character;
}
class GameContext;
class GameState;
class PlacedFurniture;
class RestaurantView;

namespace views::screens::restaurant {

struct AttackSprite {
  WorldPoint position;
  Orientation direction{Orientation::N};
  // Direction they're facing at the moment of firing
  Orientation fire_dir{Orientation::N};
  int walk_cycle{0};
  // Pixels moved per update.
  int move_speed{4};
  bool is_defender{false};

  int weapon_frame{0};
  bool firing{false};
  WorldPoint spray_anchor;

  // Bazooka
  bool projectile_active{false};
  WorldPoint projectile_pos;
  WorldPoint projectile_impact;
  int projectile_ctr{0};

  bool bomb_active{false};
  WorldPoint bomb_pos;
  int bomb_fuse{0};

  // Backing character for PvP damage
  core::character::Character* combatant{nullptr};
  // Death sequence: 0 = alive and  1..10 is the spin, then 11..17 is the
  // collapse. Police is summoned at 16; done at 17.
  int death_ctr{0};
};

enum class AttackEndReason { WalkedOut, Arrested };

class AttackScene {
public:
  AttackScene(RestaurantView& view, GameContext& context, GameState& state)
      : view_(view), context_(context), state_(state) {}

  void begin_attack(core::item::Item weapon);
  void advance(uint32_t dt_ms);
  void render();

  bool active() const { return attack_active_; }
  bool has_defender() const { return defender_.has_value(); }

  // Resume time, clear status, mark the scene ended.
  void finish_scene();

private:
  // One pass of the original's attack loop. Returns false once the scene ends.
  bool update();
  void follow_attacker_camera(WorldPoint p);
  int attack_weapon_index() const;
  std::optional<WorldPoint> weapon_reach_point(const AttackSprite& s) const;
  // One PvP damage roll against target. Returns true if its energy is now
  // exhausted.
  static bool weapon_contact_roll(core::character::Character& target);
  AttackSprite* opponent_of(const AttackSprite& s);
  // When the two points ahead find different things, the one listed first wins.
  enum class Obstacle { Furniture, FrontWall, OtherWall, None };
  WorldPoint door_point() const;
  // The top-left and bottom-right corners of a set of walls given in the
  // original's restaurant coordinates.
  std::pair<WorldPoint, WorldPoint>
  room_corners(int left, int right_margin, int back_base, int front) const;
  PlacedFurniture* furniture_at(WorldPoint p) const;
  Obstacle obstacle_at(WorldPoint p) const;
  // What s would run into walking in dir.
  Obstacle obstacle_ahead(const AttackSprite& s, Orientation dir) const;
  // Most weapon animations advance only every other update.
  bool odd_update() const;
  void advance_death(AttackSprite& s);
  void police_coming();
  void run_police_arrest();
  void end_attack(AttackEndReason reason = AttackEndReason::WalkedOut);
  void process_attack_aftermath(AttackEndReason reason);
  void update_status_display();
  PlacedFurniture* furniture_at_world(WorldPoint wp, bool& is_wall) const;
  void destroy_furniture(PlacedFurniture& pf, bool refresh_view = true);
  void fire_weapon(AttackSprite& s, bool held);
  void stop_firing(AttackSprite& s);
  void update_attack_sound();
  void tick_weapon(AttackSprite& s);
  void tick_flamethrower(AttackSprite& s);
  void tick_chemicals(AttackSprite& s);
  void tick_bazooka(AttackSprite& s);
  void tick_bomb(AttackSprite& s);
  void detonate_bomb(const AttackSprite& s);
  void render_weapon_effects(const AttackSprite& s);
  int body_frame(const AttackSprite& s) const;
  // The body sprite is centered on the fighter's position; weapons and heads
  // are drawn relative to its top-left corner.
  WorldPoint body_top_left(const AttackSprite& s) const;
  WorldPoint body_top_left(const AttackSprite& s, int frame) const;

  RestaurantView& view_;
  GameContext& context_;
  GameState& state_;

  int update_count_{0};
  std::optional<AttackSprite> attacker_;
  std::optional<AttackSprite> defender_; // only if the victim is human
  bool attack_active_{false};
  // Set once the attacker steps out through the door; the scene then walks
  // them out of the room.
  bool walking_out_{false};
  // Police escape window, 0 = none. Counts up to the arrest once police_coming
  // fires, unless the attacker escapes out the door first.
  int police_window_{0};
  // The original stops the break-in music as soon as a weapon first fires.
  bool break_in_music_stopped_{false};
  uint32_t update_accumulator_us_{0};
  core::item::Item attack_weapon_{};
  Money destroyed_furniture_value_;
};

} // namespace views::screens::restaurant
