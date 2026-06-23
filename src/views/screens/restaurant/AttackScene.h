/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>

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
  int move_gate{0};
  int move_speed{2};
  int sprite_blit_offset_x{0};
  int sprite_blit_offset_y{0};
  bool is_defender{false};

  int weapon_frame{0};
  bool firing{false};

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
  void tick();
  void render();

  bool active() const { return attack_active_; }
  bool has_defender() const { return defender_.has_value(); }

  // Resume time, clear status, mark the scene ended.
  void finish_scene();

private:
  void follow_attacker_camera(WorldPoint p);
  int attack_weapon_index() const;
  std::optional<WorldPoint> weapon_reach_point(const AttackSprite& s) const;
  // One PvP damage roll against target. Returns true if its energy is now
  // exhausted.
  static bool weapon_contact_roll(core::character::Character& target);
  AttackSprite* opponent_of(const AttackSprite& s);
  void advance_death(AttackSprite& s, bool advance_anim);
  void police_coming();
  void run_police_arrest();
  void end_attack(AttackEndReason reason = AttackEndReason::WalkedOut);
  void process_attack_aftermath(AttackEndReason reason);
  void update_status_display();
  PlacedFurniture* furniture_at_world(WorldPoint wp, bool& is_wall) const;
  void destroy_furniture(PlacedFurniture& pf, bool refresh_view = true);
  // Centre of the flame sprite this frame
  WorldPoint flame_probe_point(const AttackSprite& s) const;
  void fire_weapon(AttackSprite& s, bool held);
  void update_attack_sound();
  void tick_weapon(AttackSprite& s, bool advance_anim);
  void tick_flamethrower(AttackSprite& s);
  static void tick_chemicals(AttackSprite& s);
  void tick_bazooka(AttackSprite& s, bool advance_anim);
  void tick_bomb(AttackSprite& s, bool advance_anim);
  void render_weapon_effects(const AttackSprite& s);

  RestaurantView& view_;
  GameContext& context_;
  GameState& state_;

  // Flips every tick so we can advance weapon animation every other tick
  bool attack_anim_step_{false};
  std::optional<AttackSprite> attacker_;
  std::optional<AttackSprite> defender_; // only if the victim is human
  bool attack_active_{false};
  // Police escape window, 0 = none. Counts up to the arrest once police_coming
  // fires, unless the attacker escapes out the door first.
  int police_window_{0};
  // The original stops the break-in music as soon as a weapon first fires.
  bool break_in_music_stopped_{false};
  int attack_elapsed_ticks_{0};
  core::item::Item attack_weapon_{};
  Money destroyed_furniture_value_;
#ifndef NDEBUG
  // Draw the bomb blast box while a bomb is active, for tuning the radius.
  bool show_blast_debug_{false};
#endif
};

} // namespace views::screens::restaurant
