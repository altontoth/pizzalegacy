/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/restaurant/AttackScene.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <utility>

#include "core/GameTypes.h"
#include "core/character/Character.h"
#include "core/character/InsurancePlayerState.h"
#include "core/character/Player.h"
#include "core/city/Property.h"
#include "core/events/MilestoneId.h"
#include "core/finance/Ledger.h"
#include "core/finance/Money.h"
#include "core/finance/PlayerFinancials.h"
#include "core/restaurant/Furniture.h"
#include "core/restaurant/PlacedFurniture.h"
#include "core/restaurant/RestaurantLayout.h"
#include "core/restaurant/RestaurantTypes.h"
#include "core/state/GameOptions.h"
#include "core/state/GameState.h"
#include "core/util/LegacyConstants.h"
#include "engine/FormatCompat.h"
#include "engine/Random.h"
#include "engine/Size.h"
#include "engine/audio/AudioManager.h"
#include "engine/input/Action.h"
#include "engine/input/InputManager.h"
#include "resources/Bob.h"
#include "resources/BobManager.h"
#include "views/screens/StatusDisplay.h"
#include "views/screens/restaurant/RestaurantView.h"
#include "views/screens/restaurant/Walk.h"

using namespace LegacyConstants;

namespace views::screens::restaurant {

namespace {

constexpr int attack_move_speed_from_dexterity(int dexterity) {
  return ((dexterity / 34) + 2) * 2;
}

std::optional<Orientation> orientation_from_axis(float ax, float ay) {
  constexpr float DIRECTION_COMMIT_THRESHOLD = 0.5f;
  if (std::abs(ax) < DIRECTION_COMMIT_THRESHOLD &&
      std::abs(ay) < DIRECTION_COMMIT_THRESHOLD) {
    return std::nullopt;
  }
  if (std::abs(ax) > std::abs(ay)) {
    return (ax > 0) ? Orientation::E : Orientation::W;
  }
  return (ay > 0) ? Orientation::S : Orientation::N;
}

// Weapon tables, at our 2x scale. Values are directions in Orientation order.

namespace spray {

// How far ahead of the fighter's center a flame or spray reaches, per frame.
constexpr std::array<int, 10> REACH = {0,  44,  46,  56,  80,
                                       96, 124, 140, 150, 160};

constexpr std::array<std::array<int, ORIENTATION_COUNT>, 10> FLAME_OFF_X = {{
    {32, -8, 12, 74},
    {32, -8, 14, 74},
    {30, -14, 8, 74},
    {30, -22, 8, 74},
    {24, -46, 6, 74},
    {20, -70, 2, 74},
    {18, -90, 0, 98},
    {14, -104, -6, 122},
    {12, -116, -8, 148},
    {10, -124, -2, 164},
}};
constexpr std::array<std::array<int, ORIENTATION_COUNT>, 10> FLAME_OFF_Y = {{
    {58, -12, -40, 16},
    {58, 22, -40, 18},
    {58, 24, -46, 12},
    {58, 22, -54, 12},
    {58, 16, -78, 10},
    {58, 12, -102, 6},
    {74, 10, -126, 4},
    {96, 6, -144, -2},
    {126, 4, -156, -4},
    {140, 4, -166, -4},
}};

constexpr std::array<std::array<int, ORIENTATION_COUNT>, 10> CHEM_OFF_X = {{
    {8, 36, 12, 70},
    {32, 4, 10, 70},
    {34, -2, 8, 70},
    {28, -24, 4, 70},
    {18, -30, 0, 70},
    {16, -56, -4, 74},
    {14, -62, -12, 90},
    {10, -72, -14, 110},
    {6, -72, -6, 110},
    {0, 0, 0, 0},
}};
constexpr std::array<std::array<int, ORIENTATION_COUNT>, 10> CHEM_OFF_Y = {{
    {42, 16, -36, 18},
    {42, 28, -38, 16},
    {42, 26, -42, 14},
    {42, 22, -68, 10},
    {42, 12, -70, 6},
    {58, 10, -92, 0},
    {70, 8, -104, -6},
    {78, 4, -128, -8},
    {80, 4, -140, -8},
    {0, 0, 0, 0},
}};

// A held flame ramps up to this frame and then alternates with the one below.
constexpr int FLAME_SUSTAIN_FRAME = 5;
// A released flame burns out through the frames after the sustain.
constexpr int FLAME_BURNOUT_LAST = 9;
constexpr int FLAME_DIRECTIONAL_LAST = 7;
constexpr int FLAME_FADE_FRAME_BASE = 24;
constexpr int CHEM_STEPS = 8;

} // namespace spray

namespace held_weapon {

using Offsets = std::array<std::array<int, ORIENTATION_COUNT>, 3>;
// Attacker then defender; the defender holds the chemicals differently.
constexpr std::array<Offsets, 2> OFF_X = {{
    {{
        {16, 6, 6, 16},  // flamethrower
        {24, 16, 8, 14}, // chemical
        {38, -2, 4, 8},  // bazooka
    }},
    {{
        {16, 6, 6, 16},
        {24, 12, 8, 14},
        {38, -2, 4, 8},
    }},
}};
constexpr std::array<Offsets, 2> OFF_Y = {{
    {{
        {-12, 4, -24, -2},  // flamethrower
        {-10, 14, -26, 4},  // chemical
        {-28, 24, -32, -4}, // bazooka
    }},
    {{
        {-12, 4, -24, -2},
        {-10, 10, -26, 4},
        {-28, 24, -32, -4},
    }},
}};

} // namespace held_weapon

namespace bazooka {

// From the fighter's center.
constexpr std::array<int, ORIENTATION_COUNT> MUZZLE_X = {12, -40, -20, 14};
constexpr std::array<int, ORIENTATION_COUNT> MUZZLE_Y = {-8, -2, -56, -32};
// A shot is traced this far at a time, and its flight speeds up by this much
// every update.
constexpr int STEP = 12;
// Trace a shot for at most this many steps.
constexpr int TRACE_STEPS = 64;
constexpr int SHOT_FRAME = 8; // + dir
// The shot with its rocket lit, drawn on odd steps of the flight.
constexpr int SHOT_LIT_FRAME = 12; // + dir
constexpr int BURST_FRAME = 16;    // + dir
// Explosion frames after the burst are the frame counter plus this.
constexpr int TAIL_FRAME_OFFSET = 18;
constexpr int DAMAGE_FRAME = 4;
constexpr int LAST_FRAME = 6;

} // namespace bazooka

namespace bomb {

// Where each explosion frame sits, from the point the bomb was dropped.
constexpr std::array<int, 10> EXPLODE_DX = {0,   0,   0,   0,   8,
                                            -10, -28, -34, -44, -42};
constexpr std::array<int, 10> EXPLODE_DY = {0,   0,   0,   -2,  -16,
                                            -34, -44, -56, -62, -62};
// How far from where a bomb was dropped its blast reaches, on both axes.
constexpr int BLAST = 120;
// A blast runs the contact roll this many times against every fighter in range.
constexpr int DAMAGE_ROLLS = 40;
constexpr int FUSE = 20;
// The fuse burns down one on every fourth update.
constexpr int FUSE_UPDATES = 4;
constexpr int EXPLOSION_FIRST_FRAME = 1;
constexpr int EXPLOSION_LAST_FRAME = 8;

} // namespace bomb

namespace fighter {

constexpr int WEAPON_CONTACT_RANGE = 24;
// Half the size of the box around a fighter that the other cannot walk into.
constexpr int BLOCK_HALF = 32;
// The fighters block each other at a point this far ahead of the one moving.
constexpr int BLOCK_LOOKAHEAD = 10;
// A walker tests two points this far ahead, one either side of its line.
constexpr int OBSTACLE_CHECK_AHEAD = 12;
constexpr int OBSTACLE_CHECK_SIDE = 6;

} // namespace fighter

// In the original's restaurant coordinates.
namespace room {

// Where the fighters spawn, the attacker at the door it leaves by.
constexpr int ATTACKER_SPAWN_X_OFFSET = 60;
constexpr int ATTACKER_SPAWN_Y = 500;
constexpr int DEFENDER_SPAWN_X = 64;
constexpr int DEFENDER_SPAWN_ROW_BASE = 23;
constexpr int DEFENDER_SPAWN_Y_OFFSET = 60;
// The walls a walker stops at.
constexpr int WALK_LEFT = 52;
constexpr int WALK_RIGHT_MARGIN = 44;
constexpr int WALK_FRONT = 530;
constexpr int WALK_BACK_BASE = 540;
// The walls a weapon hits.
constexpr int HIT_LEFT = 46;
constexpr int HIT_RIGHT_MARGIN = 48;
constexpr int HIT_BACK_BASE = 516;
constexpr int HIT_FRONT = 516;
// How far from the door's center the attacker can still leave through it.
constexpr int DOOR_HALF_WIDTH = 32;

} // namespace room

// NIGHTP frames: 0-11 attacker, 12-23 defender (dir*3 + walk_cycle); 24-27 /
// 28-31 are the per-direction head frames.
namespace sprite {

constexpr int ATTACKER_BODY_FRAME0 = 0;
constexpr int DEFENDER_BODY_FRAME0 = 12;
constexpr int ATTACKER_HEAD_FRAME0 = 24;
constexpr int DEFENDER_HEAD_FRAME0 = 28;
constexpr int ATTACKER_CORPSE_FRAME0 = 32;
constexpr int DEFENDER_CORPSE_FRAME0 = 39;
constexpr std::array<int, ORIENTATION_COUNT> ATTACKER_HEAD_DX = {14, 22, 16,
                                                                 24};
constexpr std::array<int, ORIENTATION_COUNT> ATTACKER_HEAD_DY = {-6, -6, -10,
                                                                 0};
constexpr std::array<int, ORIENTATION_COUNT> DEFENDER_HEAD_DX = {26, 24, 24,
                                                                 26};
constexpr std::array<int, ORIENTATION_COUNT> DEFENDER_HEAD_DY = {-14, -16, -14,
                                                                 -16};

} // namespace sprite

namespace death {

constexpr int SPIN_LAST = 10;
constexpr int POLICE_CTR = 16;
constexpr int FROZEN_CTR = 17;

} // namespace death

constexpr int ATTACK_SCROLL_MARGIN_TILES = 4;

// Updates between police_coming and the arrest.
constexpr int POLICE_ARREST_UPDATES = 100;
// Updates before an attack with no defender can set off the alarm.
constexpr int BURGLARY_ALARM_UPDATES = 250;

constexpr uint32_t MICROSECONDS_PER_SECOND = 1000000;

// Half a sprite dimension, rounded down at the original's scale.
int half_size(int doubled_size) { return (doubled_size / 4) * 2; }

bool in_blast(WorldPoint p, WorldPoint bomb_pos) {
  return std::abs(p.x - bomb_pos.x) < bomb::BLAST &&
         std::abs(p.y - bomb_pos.y) < bomb::BLAST;
}

} // namespace

void AttackScene::begin_attack(core::item::Item weapon) {
  const auto room_size = view_.restaurant_->size();

  attack_weapon_ = weapon;
  destroyed_furniture_value_ = Money{};
  view_.restaurant_->reset_contamination();
  core::character::Character* attacker_char = nullptr;
  if (auto* player = state_.active_player()) {
    player->use_weapon(state_.time().current_date(), weapon);
    attacker_char = &player->character();
  }

  // Attacker spawns just inside the doorway, facing in; leaves by walking back
  // south out the entrance gap.
  attacker_ = AttackSprite{
      .position = door_point(),
      .direction = Orientation::N,
      .move_speed = attack_move_speed_from_dexterity(
          attacker_char ? attacker_char->dexterity() : 0),
      .is_defender = false,
      .combatant = attacker_char,
  };

  // Defender (if human) spawns at the kitchen entrance
  defender_.reset();
  if (auto owner = view_.restaurant_->property().owner_or_tenant()) {
    auto* victim = state_.player(*owner);
    if (victim && victim->is_human_controlled()) {
      const int entrance_row =
          view_.restaurant_->layout()->kitchen_entry_offset();
      const int legacy_y =
          ((entrance_row + room::DEFENDER_SPAWN_ROW_BASE - room_size.h) *
           RESTAURANT_TILE_HEIGHT) +
          room::DEFENDER_SPAWN_Y_OFFSET;
      defender_ = AttackSprite{
          .position = restaurant_point_from_legacy(room::DEFENDER_SPAWN_X,
                                                   legacy_y, room_size.h),
          .direction = Orientation::E,
          .move_speed =
              attack_move_speed_from_dexterity(victim->character().dexterity()),
          .is_defender = true,
          .combatant = &victim->character(),
      };
    }
  }

  attack_active_ = true;
  break_in_music_stopped_ = false;
  context_.audio->play_music(audio::MusicId::BREAK_IN_MUSIC_1,
                             audio::LoopMode::Loop);
  view_.view_mode_ = RestaurantViewMode::OPPONENT_ATTACK;
  update_status_display();
  view_.set_up_buttons();
  view_.mark_dirty();
}

void AttackScene::advance(uint32_t dt_ms) {
  const uint32_t update_us =
      MICROSECONDS_PER_SECOND /
      static_cast<uint32_t>(state_.options().attack_scene_hz);
  update_accumulator_us_ += dt_ms * 1000;
  while (update_accumulator_us_ >= update_us) {
    update_accumulator_us_ -= update_us;
    if (!update()) {
      return;
    }
  }
}

bool AttackScene::update() {
  if (!attacker_) {
    return false;
  }
  ++update_count_;

  const auto walk = [](AttackSprite& s, Orientation dir) {
    step_walker(s.position, s.walk_cycle, dir, s.move_speed);
  };

  // Returns true if the attacker walked out
  const auto update_sprite = [&](AttackSprite& s, input::Action move_act,
                                 input::Action fire_act) -> bool {
    if (walking_out_ && !s.is_defender) {
      stop_firing(s);
      tick_weapon(s);
      s.direction = Orientation::S;
      walk(s, Orientation::S);
      if (body_top_left(s).y >= view_.background_height_) {
        end_attack();
        state_.ui().pop();
        return true;
      }
      return false;
    }

    const auto [ax, ay] = context_.input->axis(move_act);
    const auto dir = orientation_from_axis(ax, ay);

    if (dir) {
      stop_firing(s);
    }
    else {
      fire_weapon(s, context_.input->is_pressed(fire_act));
    }
    tick_weapon(s);

    if (!dir) {
      return false; // dead zone
    }
    // Pressing a new direction first only turns the fighter.
    if (*dir != s.direction) {
      s.direction = *dir;
      return false;
    }

    if (auto* opp = opponent_of(s); opp && opp->death_ctr == 0) {
      const WorldPoint ahead =
          point_ahead(s.position, *dir, fighter::BLOCK_LOOKAHEAD);
      if (std::abs(ahead.x - opp->position.x) < fighter::BLOCK_HALF &&
          std::abs(ahead.y - opp->position.y) < fighter::BLOCK_HALF) {
        return false;
      }
    }

    switch (obstacle_ahead(s, *dir)) {
    case Obstacle::None:
      break;
    case Obstacle::FrontWall:
      // Only the attacker leaves, and only through the door.
      if (s.is_defender ||
          std::abs(s.position.x - door_point().x) >= room::DOOR_HALF_WIDTH) {
        return false;
      }
      walking_out_ = true;
      break;
    case Obstacle::Furniture:
    case Obstacle::OtherWall:
      return false;
    }
    walk(s, *dir);
    return false;
  };

  if (attacker_->death_ctr > 0) {
    advance_death(*attacker_);
  }
  else if (update_sprite(*attacker_, input::Action::MoveVec2,
                         input::Action::Fire)) {
    return false; // attacker walked out
  }
  follow_attacker_camera(attacker_->position);

  if (defender_) {
    if (defender_->death_ctr > 0) {
      advance_death(*defender_);
    }
    else {
      update_sprite(*defender_, input::Action::P2_MoveVec2,
                    input::Action::P2_Fire);
    }
  }

  if (defender_ && !walking_out_ && attacker_->death_ctr == 0 &&
      defender_->death_ctr == 0) {
    const auto strikes = [&](const AttackSprite& shooter,
                             const AttackSprite& victim) -> bool {
      const auto reach = weapon_reach_point(shooter);
      if (!reach) {
        return false;
      }
      return std::abs(reach->x - victim.position.x) <
                 fighter::WEAPON_CONTACT_RANGE &&
             std::abs(reach->y - victim.position.y) <
                 fighter::WEAPON_CONTACT_RANGE;
    };
    if (strikes(*attacker_, *defender_) && defender_->combatant &&
        weapon_contact_roll(*defender_->combatant)) {
      defender_->death_ctr = 1;
    }
    if (strikes(*defender_, *attacker_) && attacker_->combatant &&
        weapon_contact_roll(*attacker_->combatant)) {
      attacker_->death_ctr = 1;
    }
  }
  update_status_display();

  // As soon as the attacker starts walking out, the escape has succeeded.
  if (police_window_ > 0 && !walking_out_) {
    if (++police_window_ > POLICE_ARREST_UPDATES) {
      run_police_arrest(); // pops this view
      return false;
    }
  }

  if (!defender_ && !walking_out_ && state_.options().police) {
    if (update_count_ > BURGLARY_ALARM_UPDATES &&
        engine::random::rand_percentage_chance(4) &&
        engine::random::rand_percentage_chance(4)) {
      police_coming();
    }
  }

  update_attack_sound();
  view_.mark_dirty();
  return true;
}

void AttackScene::follow_attacker_camera(WorldPoint p) {
  const WorldRect vis = view_.camera_.visible_world_rect();
  const int mx = ATTACK_SCROLL_MARGIN_TILES * RESTAURANT_TILE_WIDTH;
  const int my = ATTACK_SCROLL_MARGIN_TILES * RESTAURANT_TILE_HEIGHT;
  WorldPoint sc = view_.camera_.scroll();
  if (p.x - vis.x < mx) {
    sc.x = p.x - mx;
  }
  else if ((vis.x + vis.w) - p.x < mx) {
    sc.x = (p.x + mx) - vis.w;
  }
  if (p.y - vis.y < my) {
    sc.y = p.y - my;
  }
  else if ((vis.y + vis.h) - p.y < my) {
    sc.y = (p.y + my) - vis.h;
  }
  view_.camera_.scroll(sc);
}

void AttackScene::render() {
  if (!attack_active_ || view_.kitchen_view_) {
    return;
  }
  const auto& body = context_.bob->get("bob/nightp.bob");

  const auto for_each_fighter = [&](auto&& draw) {
    if (defender_) {
      draw(*defender_);
    }
    if (attacker_) {
      draw(*attacker_);
    }
  };

  // The bomb lies under everything.
  if (attack_weapon_ == core::item::Item::Bomb) {
    for_each_fighter([&](const AttackSprite& s) { render_weapon_effects(s); });
  }

  for_each_fighter([&](const AttackSprite& s) {
    const WorldPoint tl = body_top_left(s);
    view_.background_.blit_masked(
        body.frame(static_cast<std::size_t>(body_frame(s))),
        ViewPoint{tl.x, tl.y});
  });

  const int widx = attack_weapon_index();
  if (widx >= 0 && widx <= 2) {
    const auto& weapon_bob =
        context_.bob->get(compat::format("bob/nightw{}.bob", widx));
    for_each_fighter([&](const AttackSprite& s) {
      if (s.death_ctr != 0) {
        return;
      }
      const int dir = static_cast<int>(s.direction);
      const auto& off_x = held_weapon::OFF_X[s.is_defender ? 1 : 0][widx];
      const auto& off_y = held_weapon::OFF_Y[s.is_defender ? 1 : 0][widx];
      const WorldPoint tl = body_top_left(s);
      view_.background_.blit_masked(
          weapon_bob.frame(static_cast<std::size_t>(dir)),
          ViewPoint{tl.x + off_x[dir], tl.y + off_y[dir]});
    });
  }

  // The firing sprite goes under the head.
  for_each_fighter([&](const AttackSprite& s) {
    if (attack_weapon_ != core::item::Item::Bomb) {
      render_weapon_effects(s);
    }
    if (s.death_ctr > death::SPIN_LAST) {
      return;
    }
    const int dir = static_cast<int>(s.direction);
    const int head_base = s.is_defender ? sprite::DEFENDER_HEAD_FRAME0
                                        : sprite::ATTACKER_HEAD_FRAME0;
    const int head_dx = (s.is_defender ? sprite::DEFENDER_HEAD_DX
                                       : sprite::ATTACKER_HEAD_DX)[dir];
    const int head_dy = (s.is_defender ? sprite::DEFENDER_HEAD_DY
                                       : sprite::ATTACKER_HEAD_DY)[dir];
    const WorldPoint tl = body_top_left(s);
    view_.background_.blit_masked(
        body.frame(static_cast<std::size_t>(head_base + dir)),
        ViewPoint{tl.x + head_dx, tl.y + head_dy});
  });
}

int AttackScene::body_frame(const AttackSprite& s) const {
  if (s.death_ctr > death::SPIN_LAST) {
    const int corpse0 = s.is_defender ? sprite::DEFENDER_CORPSE_FRAME0
                                      : sprite::ATTACKER_CORPSE_FRAME0;
    return corpse0 + (s.death_ctr - (death::SPIN_LAST + 1));
  }
  const int body_base = s.is_defender ? sprite::DEFENDER_BODY_FRAME0
                                      : sprite::ATTACKER_BODY_FRAME0;
  return body_base + walk_frame_for(s.direction, s.walk_cycle);
}

WorldPoint AttackScene::body_top_left(const AttackSprite& s) const {
  return body_top_left(s, body_frame(s));
}

WorldPoint AttackScene::body_top_left(const AttackSprite& s, int frame) const {
  const auto& body = context_.bob->get("bob/nightp.bob");
  const auto index = static_cast<std::size_t>(frame);
  return WorldPoint{s.position.x - half_size(body.width(index)),
                    s.position.y - half_size(body.height(index))};
}

int AttackScene::attack_weapon_index() const {
  return static_cast<int>(attack_weapon_) -
         static_cast<int>(core::item::Item::FlameThrower);
}

std::optional<WorldPoint>
AttackScene::weapon_reach_point(const AttackSprite& s) const {
  using core::item::Item;
  // Not firing right now:
  if (s.weapon_frame == 0) {
    return std::nullopt;
  }
  switch (attack_weapon_) {
  case Item::FlameThrower:
  case Item::Chemicals: {
    const int reach = spray::REACH[static_cast<std::size_t>(s.weapon_frame)];
    return point_ahead(s.position, s.fire_dir, reach);
  }
  case Item::Bazooka:
    return s.projectile_active ? s.projectile_pos : s.projectile_impact;
  case Item::Bomb:
  default:
    // Bomb doesn't kill
    return std::nullopt;
  }
}

bool AttackScene::weapon_contact_roll(core::character::Character& target) {
  if (target.energy() == 0) {
    return true;
  }
  // Survival chance is (100 - age)%. A failed roll (probability age%) kills
  // outright with no energy lost; a survived roll costs 1 energy.
  if (!engine::random::rand_percentage_chance(100 - target.age())) {
    return true;
  }
  target.energy(static_cast<uint8_t>(target.energy() - 1));
  return target.energy() == 0;
}

AttackSprite* AttackScene::opponent_of(const AttackSprite& s) {
  if (s.is_defender) {
    return attacker_ ? &*attacker_ : nullptr;
  }
  return defender_ ? &*defender_ : nullptr;
}

WorldPoint AttackScene::door_point() const {
  const int door = view_.restaurant_->layout()->entrance_offset();
  return restaurant_point_from_legacy(
      (door * RESTAURANT_TILE_WIDTH) + room::ATTACKER_SPAWN_X_OFFSET,
      room::ATTACKER_SPAWN_Y, view_.restaurant_->size().h);
}

std::pair<WorldPoint, WorldPoint> AttackScene::room_corners(int left,
                                                            int right_margin,
                                                            int back_base,
                                                            int front) const {
  const auto room_size = view_.restaurant_->size();
  return {restaurant_point_from_legacy(
              left, back_base - (room_size.h * RESTAURANT_TILE_HEIGHT),
              room_size.h),
          restaurant_point_from_legacy((room_size.w * RESTAURANT_TILE_WIDTH) +
                                           right_margin,
                                       front, room_size.h)};
}

PlacedFurniture* AttackScene::furniture_at(WorldPoint p) const {
  // The first piece in layout order wins, ashes included.
  for (const auto& pf : view_.restaurant_->layout()->placed_furniture()) {
    const Size sz = view_.bob_renderer_for(*pf).size(*pf);
    const WorldPoint o = pf->origin();
    if (p.x >= o.x && p.x <= o.x + sz.w && p.y >= o.y && p.y <= o.y + sz.h) {
      return pf.get();
    }
  }
  return nullptr;
}

AttackScene::Obstacle AttackScene::obstacle_at(WorldPoint p) const {
  const auto [top_left, bottom_right] =
      room_corners(room::WALK_LEFT, room::WALK_RIGHT_MARGIN,
                   room::WALK_BACK_BASE, room::WALK_FRONT);
  if (p.x < top_left.x || p.x > bottom_right.x || p.y < top_left.y) {
    return Obstacle::OtherWall;
  }
  if (p.y > bottom_right.y) {
    return Obstacle::FrontWall;
  }
  // Destroyed furniture still blocks.
  if (furniture_at(p)) {
    return Obstacle::Furniture;
  }
  return Obstacle::None;
}

AttackScene::Obstacle AttackScene::obstacle_ahead(const AttackSprite& s,
                                                  Orientation dir) const {
  const int side_x = walk_dy(dir) * fighter::OBSTACLE_CHECK_SIDE;
  const int side_y = walk_dx(dir) * fighter::OBSTACLE_CHECK_SIDE;
  const WorldPoint ahead =
      point_ahead(s.position, dir, fighter::OBSTACLE_CHECK_AHEAD);
  const Obstacle left =
      obstacle_at(WorldPoint{ahead.x - side_x, ahead.y - side_y});
  const Obstacle right =
      obstacle_at(WorldPoint{ahead.x + side_x, ahead.y + side_y});
  return std::min(left, right);
}

bool AttackScene::odd_update() const { return (update_count_ & 1) != 0; }

void AttackScene::advance_death(AttackSprite& s) {
  if (s.death_ctr == death::POLICE_CTR) {
    if (state_.options().police) {
      police_coming();
    }
    s.death_ctr = death::FROZEN_CTR;
    return;
  }

  // Spin a quarter turn clockwise
  s.direction = static_cast<Orientation>((static_cast<int>(s.direction) + 1) %
                                         ORIENTATION_COUNT);

  if (s.death_ctr >= death::FROZEN_CTR) {
    return;
  }
  // Hold the last spin frame until the rotation lands facing S, so the collapse
  // always begins from the same pose.
  if (s.death_ctr == death::SPIN_LAST && s.direction != Orientation::S) {
    return;
  }
  ++s.death_ctr;
}

void AttackScene::police_coming() {
  // Starts the escape window, or restarts it if the police are already coming.
  police_window_ = 1;
  // So our siren doesn't play over the music:
  context_.audio->stop_music();
  if (auto* sd = context_.status_panel) {
    sd->set_police_coming();
  }
}

void AttackScene::run_police_arrest() {
  ArrestReason reason = ArrestReason::Burglary;
  core::PlayerId attacker_id{};
  if (auto* ap = state_.active_player()) {
    attacker_id = ap->id();
  }
  core::PlayerId caught = attacker_id;
  if (defender_) {
    reason = ArrestReason::SuspectedHomicide;
    const bool attacker_survived = attacker_ && attacker_->death_ctr == 0;
    if (!attacker_survived) {
      if (auto owner = view_.restaurant_->property().owner_or_tenant()) {
        caught = *owner;
      }
    }
  }

  end_attack(AttackEndReason::Arrested);
  view_.status_controller_.show_city_default(
      {.state = &state_, .city = state_.active_city()});

  state_.active_player(caught);
  state_.ui().pop();

  // Delivered after the police conversation resolves, by which point this view
  // is gone.
  state_.initiate_arrest(reason, [st_ptr = &state_, attacker_id]() {
    constexpr auto milestone =
        core::events::MilestoneId::CarriedOutFirstAttackOnCompetitorsRestaurant;
    const core::PlayerId prev =
        st_ptr->active_player() ? st_ptr->active_player()->id() : attacker_id;
    st_ptr->active_player(attacker_id);
    st_ptr->deliver_milestone(milestone);
    st_ptr->active_player(prev);
  });
}

void AttackScene::end_attack(AttackEndReason reason) {
  process_attack_aftermath(reason);
  finish_scene();
}

void AttackScene::finish_scene() {
  attack_active_ = false;
  context_.audio->stop_current_indexed_sound();
  context_.audio->stop_music();
  if (auto* sd = context_.status_panel) {
    sd->clear_attack_status();
  }
}

void AttackScene::process_attack_aftermath(AttackEndReason reason) {
  // On arrest we need to show this fullscreen update after the arrest
  if (reason != AttackEndReason::Arrested) {
    if (auto* attacker_player = state_.active_player()) {
      state_.record_milestone(core::events::MilestoneId::
                                  CarriedOutFirstAttackOnCompetitorsRestaurant,
                              attacker_player->id());
    }
  }

  // Underworld score increase
  if (auto* attacker_player = state_.active_player()) {
    const int weapon_idx = attack_weapon_index();
    state_.adjust_underworld_score(*attacker_player, (weapon_idx + 1) * 10);
  }

  const auto owner = view_.restaurant_->property().owner_or_tenant();
  auto* victim = state_.player(*owner);
  const int64_t dollars = destroyed_furniture_value_.whole();
  const int contamination_pct = view_.restaurant_->contamination();

  const auto date = state_.time().current_date();
  if (destroyed_furniture_value_ > 0_usd) {
    victim->financials().transfer(date, Account::Furniture,
                                  Account::ProfitAndLoss,
                                  destroyed_furniture_value_,
                                  view_.restaurant_->id(), balance_sheet_only);
  }

  if (!victim->is_human_controlled()) {
    // Computer victim: apply the closure and AI profit hit immediately.
    const int closure_days = (dollars > 0) ? static_cast<int>(dollars / 2000)
                                           : (contamination_pct / 20);
    if (closure_days > 0) {
      view_.restaurant_->force_close(closure_days);
    }

    const int profit_penalty = static_cast<int>(5 + (dollars / 1000) + 1);
    view_.restaurant_->ai_adjust_days_banked(-profit_penalty);
    // Only the owner investing raises this again, so the restaurant serves
    // nobody until then.
    view_.restaurant_->ai_customers_per_day(0);
  }
  else if (destroyed_furniture_value_ > 0_usd) {
    victim->make_insurance_claim(InsuranceType::FIRE);
  }
}

void AttackScene::update_status_display() {
  auto* sd = context_.status_panel;
  if (!sd) {
    return;
  }
  if (defender_ && attacker_ && attacker_->combatant && defender_->combatant) {
    sd->set_attack_status(attacker_->combatant->energy(),
                          defender_->combatant->energy());
  }
  else {
    sd->set_attack_status(std::nullopt, std::nullopt);
  }

  sd->set_attack_damage(destroyed_furniture_value_);
  sd->set_attack_contamination(
      attack_weapon_ == core::item::Item::Chemicals
          ? std::optional<int>(view_.restaurant_->contamination())
          : std::nullopt);
}

PlacedFurniture* AttackScene::furniture_at_world(WorldPoint wp,
                                                 bool& is_wall) const {
  const auto [top_left, bottom_right] =
      room_corners(room::HIT_LEFT, room::HIT_RIGHT_MARGIN, room::HIT_BACK_BASE,
                   room::HIT_FRONT);
  is_wall = wp.x < top_left.x || wp.x > bottom_right.x || wp.y < top_left.y ||
            wp.y > bottom_right.y;
  if (is_wall) {
    return nullptr;
  }
  return furniture_at(wp);
}

void AttackScene::destroy_furniture(PlacedFurniture& pf, bool refresh_view) {
  if (pf.is_ashes()) {
    return;
  }
  destroyed_furniture_value_ += pf.furniture()->price();
  pf.change_to_ashes();
  if (refresh_view) {
    view_.invalidate_nav_grid();
    view_.mark_dirty();
  }
}

void AttackScene::fire_weapon(AttackSprite& s, bool held) {
  using core::item::Item;

  switch (attack_weapon_) {
  case Item::FlameThrower:
  case Item::Chemicals:
    s.firing = held;
    break;
  case Item::Bazooka: {
    // Holding fire fires again once the last shot has exploded.
    if (!held || s.projectile_active || s.weapon_frame != 0) {
      break;
    }
    s.fire_dir = s.direction;
    const int dir = static_cast<int>(s.fire_dir);
    const WorldPoint launch{s.position.x + bazooka::MUZZLE_X[dir],
                            s.position.y + bazooka::MUZZLE_Y[dir]};
    const AttackSprite* opp = opponent_of(s);
    const WorldPoint opp_body = opp ? opp->position : WorldPoint{};
    // Trace the shot forward until it hits the opponent, furniture, or a wall.
    WorldPoint p = launch;
    WorldPoint impact = launch;
    for (int i = 0; i < bazooka::TRACE_STEPS; ++i) {
      p = point_ahead(p, s.fire_dir, bazooka::STEP);
      impact = p;
      if (opp && opp->death_ctr == 0 &&
          std::abs(p.x - opp_body.x) < fighter::WEAPON_CONTACT_RANGE &&
          std::abs(p.y - opp_body.y) < fighter::WEAPON_CONTACT_RANGE) {
        break;
      }
      // The shot flies over ashes.
      bool wall = false;
      const auto* f = furniture_at_world(p, wall);
      if (wall || (f && !f->is_ashes())) {
        break;
      }
    }
    s.projectile_active = true;
    s.projectile_pos = launch;
    s.projectile_impact = impact;
    s.projectile_ctr = 0;
    break;
  }
  case Item::Bomb: {
    // Auto-fire, paced by bomb_active
    if (!held || s.bomb_active || s.weapon_frame != 0) {
      break;
    }
    s.bomb_active = true;
    s.bomb_pos = s.position;
    s.bomb_fuse = bomb::FUSE;
    break;
  }
  default:
    break;
  }
}

void AttackScene::stop_firing(AttackSprite& s) {
  using core::item::Item;

  s.firing = false;
  if (attack_weapon_ == Item::FlameThrower ||
      attack_weapon_ == Item::Chemicals) {
    s.weapon_frame = 0;
  }
}

void AttackScene::tick_weapon(AttackSprite& s) {
  using core::item::Item;

  switch (attack_weapon_) {
  case Item::FlameThrower:
    tick_flamethrower(s);
    break;
  case Item::Chemicals:
    tick_chemicals(s);
    break;
  case Item::Bazooka:
    tick_bazooka(s);
    break;
  case Item::Bomb:
    tick_bomb(s);
    break;
  default:
    break;
  }
}

void AttackScene::tick_flamethrower(AttackSprite& s) {
  if (s.firing) {
    if (s.weapon_frame <= spray::FLAME_SUSTAIN_FRAME) {
      s.fire_dir = s.direction;
    }
    if (s.weapon_frame == spray::FLAME_SUSTAIN_FRAME) {
      --s.weapon_frame;
    }
    else if (odd_update()) {
      ++s.weapon_frame;
    }
  }
  else if (s.weapon_frame >= spray::FLAME_BURNOUT_LAST) {
    s.weapon_frame = 0;
  }
  else if (s.weapon_frame >= spray::FLAME_SUSTAIN_FRAME) {
    if (odd_update()) {
      ++s.weapon_frame;
    }
  }
  else if (s.weapon_frame > 0) {
    --s.weapon_frame;
  }
  if (s.weapon_frame > spray::FLAME_BURNOUT_LAST) {
    s.weapon_frame = 0;
  }

  if (s.weapon_frame == 0) {
    return;
  }
  // Damage the furniture at the tip of the flame, now that weapon_frame is
  // current. Ashes in front of a piece shield it.
  bool wall = false;
  if (auto* f = furniture_at_world(*weapon_reach_point(s), wall)) {
    f->damage(2);
    if (f->durability() <= 0) {
      destroy_furniture(*f);
    }
  }
}

void AttackScene::tick_chemicals(AttackSprite& s) {
  if (s.firing) {
    if (s.weapon_frame == 0) {
      s.fire_dir = s.direction;
      // The cloud stays where it started.
      s.spray_anchor = body_top_left(s, static_cast<int>(s.fire_dir));
    }
    if (s.weapon_frame >= spray::CHEM_STEPS) {
      s.weapon_frame = 1;
    }
    else if (odd_update()) {
      ++s.weapon_frame;
    }
  }
  // If you release fire the spray animation finishes.
  else if (s.weapon_frame >= spray::CHEM_STEPS) {
    s.weapon_frame = 0;
  }
  else if (s.weapon_frame > 0 && odd_update()) {
    ++s.weapon_frame;
  }

  // One update in five of spraying raises contamination.
  if (s.weapon_frame != 0 && view_.restaurant_->contamination() < 100 &&
      engine::random::rand_percentage_chance(20)) {
    view_.restaurant_->add_contamination(1);
  }
}

void AttackScene::tick_bazooka(AttackSprite& s) {
  if (s.projectile_active && s.weapon_frame == 0) {
    ++s.projectile_ctr;
    s.projectile_pos = point_ahead(s.projectile_pos, s.fire_dir,
                                   bazooka::STEP * s.projectile_ctr);
    const int dx = walk_dx(s.fire_dir);
    const int dy = walk_dy(s.fire_dir);

    const int distance_past_impact =
        ((s.projectile_pos.x - s.projectile_impact.x) * dx) +
        ((s.projectile_pos.y - s.projectile_impact.y) * dy);
    if (distance_past_impact >= 0) {
      s.projectile_pos = s.projectile_impact;
      s.weapon_frame = 1; // begin explosion
    }
    view_.mark_dirty();
  }
  else if (s.weapon_frame >= 1 && odd_update()) {
    ++s.weapon_frame;
    if (s.weapon_frame == bazooka::DAMAGE_FRAME) {
      bool wall = false;
      if (auto* f = furniture_at_world(s.projectile_impact, wall)) {
        destroy_furniture(*f);
      }
    }
    if (s.weapon_frame > bazooka::LAST_FRAME) {
      s.weapon_frame = 0;
      s.projectile_active = false;
    }
    view_.mark_dirty();
  }
}

void AttackScene::tick_bomb(AttackSprite& s) {
  if (!s.bomb_active) {
    return;
  }
  if (s.bomb_fuse > 0) {
    if (update_count_ % bomb::FUSE_UPDATES == 0) {
      --s.bomb_fuse;
      view_.mark_dirty();
    }
    if (s.bomb_fuse > 0) {
      return;
    }
    s.weapon_frame = bomb::EXPLOSION_FIRST_FRAME;
  }
  if (odd_update()) {
    return;
  }
  ++s.weapon_frame;
  if (s.weapon_frame > bomb::EXPLOSION_LAST_FRAME) {
    // The blast does its damage once the explosion has played out.
    detonate_bomb(s);
    s.weapon_frame = 0;
    s.bomb_active = false;
  }
  view_.mark_dirty();
}

void AttackScene::detonate_bomb(const AttackSprite& s) {
  // Destroy all non-ashes pieces whose top-left corner is within the blast.
  bool any_destroyed = false;
  for (const auto& up : view_.current_furniture_list()) {
    PlacedFurniture* pf = up.get();
    if (pf->is_ashes()) {
      continue;
    }
    if (in_blast(pf->origin(), s.bomb_pos)) {
      if (auto* m = view_.restaurant_->layout()->find_by_id(
              pf->id(), view_.kitchen_view_)) {
        destroy_furniture(*m, /*refresh_view=*/false);
        any_destroyed = true;
      }
    }
  }
  if (any_destroyed) {
    view_.invalidate_nav_grid();
  }
  const auto reduce_energy_if_in_blast = [&](AttackSprite* fighter) {
    if (!fighter || !fighter->combatant) {
      return;
    }
    if (in_blast(fighter->position, s.bomb_pos)) {
      for (int i = 0; i < bomb::DAMAGE_ROLLS; ++i) {
        // Bombs can't kill anyone, only reduce energy
        (void)weapon_contact_roll(*fighter->combatant);
      }
    }
  };
  reduce_energy_if_in_blast(attacker_ ? &*attacker_ : nullptr);
  reduce_energy_if_in_blast(defender_ ? &*defender_ : nullptr);
}

void AttackScene::render_weapon_effects(const AttackSprite& s) {
  using core::item::Item;

  const int widx = attack_weapon_index();
  if (widx < 0 || widx > 3) {
    return;
  }
  const auto& w = context_.bob->get(compat::format("bob/nightw{}.bob", widx));
  const int dir = static_cast<int>(s.fire_dir);
  const WorldPoint anchor = body_top_left(s);

  switch (attack_weapon_) {
  case Item::FlameThrower: {
    // Render whenever the flame is active (not just firing) so the fade frames
    // plays after release
    if (s.weapon_frame == 0) {
      break;
    }
    // Phases 1 to 7 have a frame per direction, BOB frames 4 to 31; phases 8
    // and 9 have one frame each, 32 and 33.
    const int wf = s.weapon_frame;
    const int frame = (wf <= spray::FLAME_DIRECTIONAL_LAST)
                          ? ((wf * 4) + dir)
                          : (spray::FLAME_FADE_FRAME_BASE + wf);
    view_.background_.blit_masked(
        w.frame(frame), ViewPoint{anchor.x + spray::FLAME_OFF_X[wf][dir],
                                  anchor.y + spray::FLAME_OFF_Y[wf][dir]});
    break;
  }
  case Item::Chemicals: {
    if (s.weapon_frame == 0) {
      break;
    }
    const int wf = s.weapon_frame;
    view_.background_.blit_masked(
        w.frame((wf * 4) + dir),
        ViewPoint{s.spray_anchor.x + spray::CHEM_OFF_X[wf][dir],
                  s.spray_anchor.y + spray::CHEM_OFF_Y[wf][dir]});
    break;
  }
  case Item::Bazooka: {
    if (s.projectile_active && s.weapon_frame == 0) {
      const int shot_frame = (s.projectile_ctr & 1) ? bazooka::SHOT_LIT_FRAME
                                                    : bazooka::SHOT_FRAME;
      view_.background_.blit_masked(
          w.frame(shot_frame + dir),
          ViewPoint{s.projectile_pos.x, s.projectile_pos.y});
    }
    else if (s.weapon_frame >= 1) {
      const int frame = (s.weapon_frame == 1)
                            ? (bazooka::BURST_FRAME + dir)
                            : (s.weapon_frame + bazooka::TAIL_FRAME_OFFSET);
      view_.background_.blit_masked(
          w.frame(frame),
          ViewPoint{s.projectile_impact.x, s.projectile_impact.y});
    }
    break;
  }
  case Item::Bomb: {
    if (!s.bomb_active) {
      break;
    }
    const WorldPoint center = s.bomb_pos;
    if (s.bomb_fuse > 0) {
      // Pulse between frame 0 and 1
      const int pulse = s.bomb_fuse & 1;
      view_.background_.blit_masked(w.frame(pulse),
                                    ViewPoint{center.x, center.y});
    }
    else {
      const int wf = s.weapon_frame;
      view_.background_.blit_masked(w.frame(wf),
                                    ViewPoint{center.x + bomb::EXPLODE_DX[wf],
                                              center.y + bomb::EXPLODE_DY[wf]});
    }
    break;
  }
  default:
    break;
  }
}

void AttackScene::update_attack_sound() {
  using audio::LoopMode;
  using audio::SoundId;
  using core::item::Item;

  const auto explosion_active = [&](const AttackSprite& s) {
    if (attack_weapon_ == Item::Bazooka) {
      return s.weapon_frame >= 1;
    }
    if (attack_weapon_ == Item::Bomb) {
      return s.bomb_active && s.bomb_fuse == 0;
    }
    return false;
  };
  // weapon_frame != 0 keeps the flame sounding through its fade frames.
  const auto flame_active = [&](const AttackSprite& s) {
    return attack_weapon_ == Item::FlameThrower && s.weapon_frame != 0;
  };
  // The launch fires once; the gate makes the rest of the flight silent.
  const auto bazooka_flight = [&](const AttackSprite& s) {
    return attack_weapon_ == Item::Bazooka && s.projectile_active &&
           s.weapon_frame == 0;
  };

  const auto any = [&](auto&& pred) {
    return (attacker_ && pred(*attacker_)) || (defender_ && pred(*defender_));
  };

  auto* a = context_.audio;

  if (!break_in_music_stopped_ &&
      (any(explosion_active) || any(flame_active) || any(bazooka_flight))) {
    a->stop_music();
    break_in_music_stopped_ = true;
  }

  if (police_window_ > 0) {
    a->play_sound_if_changed(SoundId::POLICE_SIREN, LoopMode::Loop);
  }
  else if (any(explosion_active)) {
    a->play_sound_if_changed(SoundId::WEAPON_4);
  }
  else if (any(flame_active)) {
    a->play_sound_if_changed(SoundId::WEAPON_1, LoopMode::Loop);
  }
  else if (any(bazooka_flight)) {
    a->play_sound_if_changed(SoundId::WEAPON_3);
  }
  else {
    a->stop_current_indexed_sound();
  }
}

} // namespace views::screens::restaurant
