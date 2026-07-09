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

#include "core/GameTypes.h"
#include "core/character/Character.h"
#include "core/character/Player.h"
#include "core/city/Property.h"
#include "core/events/LeventId.h"
#include "core/restaurant/Furniture.h"
#include "core/restaurant/PlacedFurniture.h"
#include "core/restaurant/RestaurantTypes.h"
#include "core/state/GameState.h"
#include "core/util/LegacyConstants.h"
#include "engine/FormatCompat.h"
#include "engine/Random.h"
#include "engine/Size.h"
#include "engine/audio/AudioManager.h"
#include "engine/input/Action.h"
#include "engine/input/InputManager.h"
#include "gfx/ColorIndex.h"
#include "gfx/Draw.h"
#include "resources/Bob.h"
#include "resources/BobManager.h"
#include "views/screens/StatusDisplay.h"
#include "views/screens/restaurant/RestaurantView.h"
#include "views/screens/restaurant/Walk.h"

using namespace LegacyConstants;

namespace views::screens::restaurant {

namespace {

constexpr int attack_move_speed_from_dexterity(int dexterity) {
  return (dexterity / 34) + 2;
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

constexpr int ATTACK_MOVE_RATE_THRESHOLD = 6;

// Weapon tables. All values are at original 1x scale, they get doubled at
// rendering. Values are directions in Orientation order.

constexpr std::array<int, ORIENTATION_COUNT> FLAME_REACH = {22, 23, 28, 40};

constexpr std::array<std::array<int, ORIENTATION_COUNT>, 10> FLAME_OFF_X = {{
    {16, -4, 6, 37},
    {16, -4, 7, 37},
    {15, -7, 4, 37},
    {15, -11, 4, 37},
    {12, -23, 3, 37},
    {10, -35, 1, 37},
    {9, -45, 0, 49},
    {7, -52, -3, 61},
    {6, -58, -4, 74},
    {5, -62, -1, 82},
}};
constexpr std::array<std::array<int, ORIENTATION_COUNT>, 10> FLAME_OFF_Y = {{
    {29, -6, -20, 8},
    {29, 11, -20, 9},
    {29, 12, -23, 6},
    {29, 11, -27, 6},
    {29, 8, -39, 5},
    {29, 6, -51, 3},
    {37, 5, -63, 2},
    {48, 3, -72, -1},
    {63, 2, -78, -2},
    {70, 2, -83, -2},
}};

constexpr std::array<std::array<int, ORIENTATION_COUNT>, 10> CHEM_OFF_X = {{
    {4, 18, 6, 35},
    {16, 2, 5, 35},
    {17, -1, 4, 35},
    {14, -12, 2, 35},
    {9, -15, 0, 35},
    {8, -28, -2, 37},
    {7, -31, -6, 45},
    {5, -36, -7, 55},
    {3, -36, -3, 55},
    {0, 0, 0, 0},
}};
constexpr std::array<std::array<int, ORIENTATION_COUNT>, 10> CHEM_OFF_Y = {{
    {21, 8, -18, 9},
    {21, 14, -19, 8},
    {21, 13, -21, 7},
    {21, 11, -34, 5},
    {21, 6, -35, 3},
    {29, 5, -46, 0},
    {35, 4, -52, -3},
    {39, 2, -64, -4},
    {40, 2, -70, -4},
    {0, 0, 0, 0},
}};

constexpr std::array<std::array<int, ORIENTATION_COUNT>, 3> HELD_OFF_X = {{
    {8, 3, 3, 8},   // flamethrower
    {12, 8, 4, 7},  // chemical
    {19, -1, 2, 4}, // bazooka
}};
constexpr std::array<std::array<int, ORIENTATION_COUNT>, 3> HELD_OFF_Y = {{
    {-6, 2, -12, -1},   // flamethrower
    {-5, 7, -13, 2},    // chemical
    {-14, 12, -16, -2}, // bazooka
}};

constexpr std::array<int, ORIENTATION_COUNT> BAZOOKA_MUZZLE_X = {6, -20, -10,
                                                                 7};
constexpr std::array<int, ORIENTATION_COUNT> BAZOOKA_MUZZLE_Y = {-4, -1, -28,
                                                                 -16};
constexpr std::array<int, ORIENTATION_COUNT> BAZOOKA_PROJ_OFF_X = {13, 9, 12,
                                                                   7};
constexpr std::array<int, ORIENTATION_COUNT> BAZOOKA_PROJ_OFF_Y = {0, 13, 2,
                                                                   14};

constexpr int ATTACK_SCROLL_MARGIN_TILES = 4;

// Explosion sprite Y nudge per frame
constexpr std::array<int, 11> BOMB_EXPLODE_DY = {0,   0,   0,   -1,  -8, -17,
                                                 -22, -28, -31, -31, 0};

constexpr int BOMB_BLAST = 60;
// A bomb blast runs the contact roll this many times against every fighter in
// range.
constexpr int BOMB_DAMAGE_ROLLS = 40;

// The blit offset is half the sprite size plus these margins, so the logical
// position sits at the body center and the anchor (position - offset) lands at
// the sprite top-left. The weapon offset tables are measured from that top-left
// corner, so subtracting half the sprite puts the anchor in their frame.
constexpr int ATTACKER_ANCHOR_MARGIN_X = 10;
constexpr int ATTACKER_ANCHOR_MARGIN_Y = 16;

constexpr int FLAME_FIRE_STEPS = 7;
constexpr int FLAME_FADE_LAST = 9;
// Fade frames are indexed off a base so phase 8/9 land on BOB frames 32/33.
constexpr int FLAME_FADE_FRAME_BASE = 24;
constexpr int CHEM_SPRAY_STEPS = 8;
constexpr int BAZOOKA_FLIGHT_FRAME = 8; // + dir
constexpr int BAZOOKA_BURST_FRAME = 12; // + dir
constexpr int BAZOOKA_TAIL_FRAME = 16;

constexpr int BOMB_FUSE_TICKS = 40;
constexpr int BOMB_PULSE_DIVISOR = 4;
constexpr int BOMB_EXPLOSION_FIRST_FRAME = 2;
constexpr int BOMB_EXPLOSION_LAST_FRAME = 9;

constexpr int WEAPON_CONTACT_RANGE = 12 * 2;
constexpr int PLAYER_COLLISION_HALF_X = 16;
constexpr int PLAYER_COLLISION_HALF_Y = 16;
constexpr int DEATH_SPIN_LAST = 10;
constexpr int DEATH_POLICE_CTR = 16;
constexpr int DEATH_FROZEN_CTR = 17;
constexpr int ATTACKER_CORPSE_FRAME0 = 32;
constexpr int DEFENDER_CORPSE_FRAME0 = 39;

// Ticks between police_coming and the arrest.
constexpr int POLICE_ARREST_TICKS = 100;

} // namespace

void AttackScene::begin_attack(core::item::Item weapon) {
  constexpr int tile_w = RESTAURANT_TILE_WIDTH;
  constexpr int tile_h = RESTAURANT_TILE_HEIGHT;
  const auto room_size = view_.restaurant_->size();

  attack_weapon_ = weapon;
  destroyed_furniture_value_ = Money{};
  core::character::Character* attacker_char = nullptr;
  if (auto* player = state_.active_player()) {
    player->inventory().remove(weapon, 1);
    attacker_char = &player->character();
  }

  const auto& body = context_.bob->get("bob/nightp.bob");
  const int blit_off_x = (body.width(0) / 2) + ATTACKER_ANCHOR_MARGIN_X;
  const int blit_off_y = (body.height(0) / 2) + ATTACKER_ANCHOR_MARGIN_Y;

  // Attacker spawns just inside the doorway, facing in; leaves by walking back
  // south out the entrance gap.
  const int entrance_offset = view_.restaurant_->layout()->entrance_offset();
  const int logical_x = (entrance_offset * tile_w) + (3 * tile_w / 2);
  const int spawn_y = (room_size.h * tile_h) + (tile_h / 2);
  attacker_ = AttackSprite{
      .position = {logical_x, spawn_y},
      .direction = Orientation::N,
      .move_speed = attack_move_speed_from_dexterity(
          attacker_char ? attacker_char->dexterity() : 0),
      .sprite_blit_offset_x = blit_off_x,
      .sprite_blit_offset_y = blit_off_y,
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
      const int kitchen_y =
          ((entrance_row + 1 + KITCHEN_ENTRANCE_ROWS / 2) * tile_h) +
          (tile_h / 2);
      defender_ = AttackSprite{
          .position = {tile_w, kitchen_y},
          .direction = Orientation::E,
          .move_speed =
              attack_move_speed_from_dexterity(victim->character().dexterity()),
          .sprite_blit_offset_x = blit_off_x,
          .sprite_blit_offset_y = blit_off_y,
          .is_defender = true,
          .combatant = &victim->character(),
      };
    }
  }

  view_.walk_scene_.clear_walker();
  attack_active_ = true;
  break_in_music_stopped_ = false;
  state_.pause_time_advancement();
  context_.audio->play_music(audio::MusicId::BREAK_IN_MUSIC_1,
                             audio::LoopMode::Loop);
  view_.view_mode_ = RestaurantViewMode::OPPONENT_ATTACK;
  update_status_display();
  view_.set_up_buttons();
  view_.mark_dirty();
}

void AttackScene::tick() {
  if (!attacker_) {
    return;
  }

  // Half-rate gate for weapon animation
  attack_anim_step_ = !attack_anim_step_;
  const bool advance_anim = attack_anim_step_;

  // Returns true if the attacker walked out
  const auto step_sprite = [&](AttackSprite& s, input::Action move_act,
                               input::Action fire_act) -> bool {
    // Holding the fire key keeps firing
    const bool fire_held = context_.input->is_pressed(fire_act);
    fire_weapon(s, fire_held);
    tick_weapon(s, advance_anim);

    s.move_gate += s.move_speed;
    if (s.move_gate < ATTACK_MOVE_RATE_THRESHOLD) {
      return false;
    }
    s.move_gate -= ATTACK_MOVE_RATE_THRESHOLD;

    // tick_weapon above can invalidate the cached grid, so get the current
    // reference.
    const auto& [blocked, grid_w, grid_h] = view_.navigation_grid();

    const auto [ax, ay] = context_.input->axis(move_act);
    const auto dir = orientation_from_axis(ax, ay);
    if (!dir) {
      return false; // dead zone
    }
    s.direction = *dir;

    const int nx = (s.position.x / WALK_CELL_SIZE) + walk_dx(*dir);
    const int ny = (s.position.y / WALK_CELL_SIZE) + walk_dy(*dir);

    // The attacker leaves by walking south off the bottom edge of the grid.
    if (!s.is_defender && *dir == Orientation::S && ny >= grid_h) {
      end_attack();
      state_.ui().pop();
      return true;
    }

    if (nx < 0 || nx >= grid_w || ny < 0 || ny >= grid_h ||
        blocked[(ny * grid_w) + nx]) {
      return false;
    }

    // CD between players. A step shifts one axis by one cell,
    // which is smaller than the collision box, so this can never reach an
    // overlapping state.
    if (auto* opp = opponent_of(s); opp && opp->death_ctr == 0) {
      const WorldPoint cand{s.position.x + (walk_dx(*dir) * WALK_CELL_SIZE),
                            s.position.y + (walk_dy(*dir) * WALK_CELL_SIZE)};
      if (std::abs(cand.x - opp->position.x) < PLAYER_COLLISION_HALF_X &&
          std::abs(cand.y - opp->position.y) < PLAYER_COLLISION_HALF_Y) {
        return false;
      }
    }

    step_one_cell(s.position, s.walk_cycle, *dir);
    return false;
  };

  if (attacker_->death_ctr > 0) {
    advance_death(*attacker_, advance_anim);
  }
  else if (step_sprite(*attacker_, input::Action::MoveVec2,
                       input::Action::Fire)) {
    return; // attacker walked out
  }
  follow_attacker_camera(attacker_->position);

  if (defender_) {
    if (defender_->death_ctr > 0) {
      advance_death(*defender_, advance_anim);
    }
    else {
      step_sprite(*defender_, input::Action::P2_MoveVec2,
                  input::Action::P2_Fire);
    }
  }

  if (defender_ && attacker_->death_ctr == 0 && defender_->death_ctr == 0) {
    const auto strikes = [&](const AttackSprite& shooter,
                             const AttackSprite& victim) -> bool {
      const auto reach = weapon_reach_point(shooter);
      if (!reach) {
        return false;
      }
      const WorldPoint body =
          attack_weapon_ == core::item::Item::Bazooka
              ? victim.position
              : WorldPoint{victim.position.x - victim.sprite_blit_offset_x,
                           victim.position.y - victim.sprite_blit_offset_y};
      return std::abs(reach->x - body.x) < WEAPON_CONTACT_RANGE &&
             std::abs(reach->y - body.y) < WEAPON_CONTACT_RANGE;
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

  // Solo burglary timer.
  if (!defender_ && police_window_ == 0) {
    ++attack_elapsed_ticks_;
    const int unk_9E6A7 = 0; // original only ever reads this, always 0
    const int threshold = 250 - ((unk_9E6A7 / 34) + 2);
    if (attack_elapsed_ticks_ > threshold &&
        engine::random::rand_percentage_chance(4) &&
        engine::random::rand_percentage_chance(4)) {
      police_coming();
    }
  }

  if (police_window_ > 0) {
    if (++police_window_ > POLICE_ARREST_TICKS) {
      run_police_arrest(); // pops this view
      return;
    }
  }

  update_attack_sound();
  view_.mark_dirty();
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
  // NIGHTP frames: 0-11 attacker, 12-23 defender (dir*3 + walk_cycle); 24-27 /
  // 28-31 are the per-direction head frames.
  const auto& body = context_.bob->get("bob/nightp.bob");

  static constexpr std::array<int, ORIENTATION_COUNT> attacker_head_dx = {
      7, 11, 8, 12};
  static constexpr std::array<int, ORIENTATION_COUNT> attacker_head_dy = {
      -3, -3, -5, 0};
  static constexpr std::array<int, ORIENTATION_COUNT> defender_head_dx = {
      13, 12, 12, 13};
  static constexpr std::array<int, ORIENTATION_COUNT> defender_head_dy = {
      -7, -8, -7, -8};

  const auto draw = [&](const AttackSprite& s) {
    const int dir = static_cast<int>(s.direction);
    const ViewPoint blit_pos{s.position.x - s.sprite_blit_offset_x,
                             s.position.y - s.sprite_blit_offset_y};

    if (s.death_ctr > DEATH_SPIN_LAST) {
      const int corpse0 =
          s.is_defender ? DEFENDER_CORPSE_FRAME0 : ATTACKER_CORPSE_FRAME0;
      view_.background_.blit_masked(
          body.frame(corpse0 + (s.death_ctr - (DEATH_SPIN_LAST + 1))),
          blit_pos);
      return;
    }

    const int body_base = s.is_defender ? 12 : 0;
    const int frame = body_base + walk_frame_for(s.direction, s.walk_cycle);
    view_.background_.blit_masked(body.frame(frame), blit_pos);

    const int widx = attack_weapon_index();
    if (widx >= 0 && widx <= 2) {
      const auto& weapon_bob =
          context_.bob->get(compat::format("bob/nightw{}.bob", widx));
      view_.background_.blit_masked(
          weapon_bob.frame(dir),
          ViewPoint{blit_pos.x + (HELD_OFF_X[widx][dir] * 2),
                    blit_pos.y + (HELD_OFF_Y[widx][dir] * 2)});
    }

    // The bomb renders under the fighters (floor pass below); everything else
    // renders over the body here.
    if (attack_weapon_ != core::item::Item::Bomb) {
      render_weapon_effects(s);
    }

    // Head last, over the held weapon and firing sprite.
    const int head_base = s.is_defender ? 28 : 24;
    const int head_dx =
        (s.is_defender ? defender_head_dx : attacker_head_dx)[dir];
    const int head_dy =
        (s.is_defender ? defender_head_dy : attacker_head_dy)[dir];
    view_.background_.blit_masked(
        body.frame(head_base + dir),
        ViewPoint{blit_pos.x + (head_dx * 2), blit_pos.y + (head_dy * 2)});
  };

  // render bombs before any fighter body so the bodies always draw on top.
  if (attack_weapon_ == core::item::Item::Bomb) {
    if (attacker_) {
      render_weapon_effects(*attacker_);
    }
    if (defender_) {
      render_weapon_effects(*defender_);
    }
  }

  if (attacker_) {
    draw(*attacker_);
  }
  if (defender_) {
    draw(*defender_);
  }
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
  const WorldPoint anchor{s.position.x - s.sprite_blit_offset_x,
                          s.position.y - s.sprite_blit_offset_y};
  const int dir = static_cast<int>(s.fire_dir);
  switch (attack_weapon_) {
  case Item::FlameThrower:
  case Item::Chemicals: {
    const int reach = FLAME_REACH[std::min(s.weapon_frame, 3)] * 2;
    return WorldPoint{anchor.x + (WALK_DX[dir] * reach),
                      anchor.y + (WALK_DY[dir] * reach)};
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

void AttackScene::advance_death(AttackSprite& s, bool advance_anim) {
  if (!advance_anim) {
    return;
  }
  if (s.death_ctr == DEATH_POLICE_CTR) {
    police_coming();
    s.death_ctr = DEATH_FROZEN_CTR;
    return;
  }

  // Spin +90 each tick
  s.direction = static_cast<Orientation>((static_cast<int>(s.direction) + 1) %
                                         ORIENTATION_COUNT);

  if (s.death_ctr >= DEATH_FROZEN_CTR) {
    return;
  }
  // Hold the last spin frame until the rotation lands facing S, so the collapse
  // always begins from the same pose.
  if (s.death_ctr == DEATH_SPIN_LAST && s.direction != Orientation::S) {
    return;
  }
  ++s.death_ctr;
}

void AttackScene::police_coming() {
  // Starts the 100-tick escape window.
  if (police_window_ > 0) {
    return; // already counting down
  }
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

  state_.active_player(caught);
  state_.ui().pop();

  // Delivered after the police conversation resolves, by which point this view
  // is gone.
  state_.initiate_arrest(reason, [st_ptr = &state_, attacker_id]() {
    constexpr auto milestone =
        core::events::LeventId::CarriedOutFirstAttackOnCompetitorsRestaurant;
    if (st_ptr->message_manager().was_scheduled(milestone, attacker_id)) {
      return;
    }
    const core::PlayerId prev =
        st_ptr->active_player() ? st_ptr->active_player()->id() : attacker_id;
    st_ptr->active_player(attacker_id);
    st_ptr->deliver_message(milestone);
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
  state_.resume_time_advancement();
  if (auto* sd = context_.status_panel) {
    sd->clear_attack_status();
  }
}

void AttackScene::process_attack_aftermath(AttackEndReason reason) {
  // On arrest we need to show this fullscreen update after the arrest
  if (reason != AttackEndReason::Arrested) {
    if (auto* attacker_player = state_.active_player()) {
      state_.message_manager().schedule_message(
          core::events::LeventId::CarriedOutFirstAttackOnCompetitorsRestaurant,
          attacker_player->id(), state_.time().current_date());
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

  if (!victim->is_human_controlled()) {
    // Computer victim: apply the closure and AI profit hit immediately.
    const int closure_days = (dollars > 0) ? static_cast<int>(dollars / 2000)
                                           : (contamination_pct / 20);
    if (closure_days > 0) {
      view_.restaurant_->force_close(closure_days);
    }

    const int profit_penalty = static_cast<int>(5 + (dollars / 1000) + 1);
    view_.restaurant_->ai_adjust_profit_days(-profit_penalty);
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

  sd->set_attack_contamination(
      attack_weapon_ == core::item::Item::Chemicals
          ? std::optional<int>(view_.restaurant_->contamination())
          : std::nullopt);
}

PlacedFurniture* AttackScene::furniture_at_world(WorldPoint wp,
                                                 bool& is_wall) const {
  is_wall = false;
  const auto room_size = view_.restaurant_->size();
  constexpr int tw = RESTAURANT_TILE_WIDTH;
  constexpr int th = RESTAURANT_TILE_HEIGHT;
  const WorldRect interior{tw, th, room_size.w * tw, room_size.h * th};
  if (!interior.contains(wp)) {
    is_wall = true;
    return nullptr;
  }
  for (const auto& up : view_.current_furniture_list()) {
    const PlacedFurniture* pf = up.get();
    if (pf->is_ashes()) {
      continue; // ashes is shoot-through
    }
    const Size sz = view_.bob_renderer_for(*pf).size(*pf);
    const WorldRect r{pf->origin().x, pf->origin().y, sz.w, sz.h};
    if (r.contains(wp)) {
      return view_.restaurant_->layout()->find_by_id(pf->id(),
                                                     view_.kitchen_view_);
    }
  }
  return nullptr;
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

WorldPoint AttackScene::flame_probe_point(const AttackSprite& s) const {
  const WorldPoint anchor{s.position.x - s.sprite_blit_offset_x,
                          s.position.y - s.sprite_blit_offset_y};
  const int dir = static_cast<int>(s.fire_dir);
  // Probe the center of the flame BOB frame so the hit lines up with the
  // visible nozzle flame (same anchor + offset render_weapon_effects blits at).
  const int widx = attack_weapon_index();
  const auto& w = context_.bob->get(compat::format("bob/nightw{}.bob", widx));
  const int wf = s.weapon_frame;
  const int frame = (wf <= 7) ? ((wf * 4) + dir) : (FLAME_FADE_FRAME_BASE + wf);
  const WorldPoint tl{anchor.x + (FLAME_OFF_X[wf][dir] * 2),
                      anchor.y + (FLAME_OFF_Y[wf][dir] * 2)};
  const Size fsz = w.size(static_cast<std::size_t>(frame));
  return WorldPoint{tl.x + (fsz.w / 2), tl.y + (fsz.h / 2)};
}

void AttackScene::fire_weapon(AttackSprite& s, bool held) {
  using core::item::Item;

  const WorldPoint anchor{s.position.x - s.sprite_blit_offset_x,
                          s.position.y - s.sprite_blit_offset_y};

  switch (attack_weapon_) {
  case Item::FlameThrower: {
    s.firing = held;
    if (!held) {
      return;
    }
    s.fire_dir = s.direction;
    break;
  }
  case Item::Chemicals: {
    s.firing = held;
    if (!held) {
      return;
    }
    s.fire_dir = s.direction;
    // Every fire tick raises contamination
    view_.restaurant_->add_contamination(1);
    break;
  }
  case Item::Bazooka: {
    // Auto-fire when fire is held, but only when no shot is in
    // flight/exploding. TODO: Figure out how the original decided the fire
    // rate, I think it can have more than one projectile active
    if (!held || s.projectile_active || s.weapon_frame != 0) {
      break;
    }
    s.fire_dir = s.direction;
    const int dir = static_cast<int>(s.fire_dir);
    const WorldPoint launch{
        anchor.x + ((BAZOOKA_MUZZLE_X[dir] + BAZOOKA_PROJ_OFF_X[dir]) * 2),
        anchor.y + ((BAZOOKA_MUZZLE_Y[dir] + BAZOOKA_PROJ_OFF_Y[dir]) * 2)};
    const AttackSprite* opp = opponent_of(s);
    const WorldPoint opp_body = opp ? opp->position : WorldPoint{};
    // Trace the shot forward until it hits the opponent, furniture, or a wall.
    WorldPoint p = launch;
    WorldPoint impact = launch;
    for (int i = 0; i < 64; ++i) {
      p.x += WALK_DX[dir] * WALK_CELL_SIZE;
      p.y += WALK_DY[dir] * WALK_CELL_SIZE;
      impact = p;
      if (opp && opp->death_ctr == 0 &&
          std::abs(p.x - opp_body.x) < WEAPON_CONTACT_RANGE &&
          std::abs(p.y - opp_body.y) < WEAPON_CONTACT_RANGE) {
        break;
      }
      bool wall = false;
      if (furniture_at_world(p, wall) != nullptr || wall) {
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
    s.bomb_pos = anchor;
    s.bomb_fuse = BOMB_FUSE_TICKS;
    break;
  }
  default:
    break;
  }
}

void AttackScene::tick_weapon(AttackSprite& s, bool advance_anim) {
  using core::item::Item;

  switch (attack_weapon_) {
  case Item::FlameThrower:
    tick_flamethrower(s);
    break;
  case Item::Chemicals:
    tick_chemicals(s);
    break;
  case Item::Bazooka:
    tick_bazooka(s, advance_anim);
    break;
  case Item::Bomb:
    tick_bomb(s, advance_anim);
    break;
  default:
    break;
  }
}

void AttackScene::tick_flamethrower(AttackSprite& s) {
  // Hold loops frames 1..7; on release play fade frames 8,9 then snap to 0.
  if (s.firing) {
    s.weapon_frame = (s.weapon_frame == 0 || s.weapon_frame >= FLAME_FIRE_STEPS)
                         ? 1
                         : s.weapon_frame + 1;
    // Damage furniture under the flame, now that weapon_frame is current.
    bool wall = false;
    if (auto* f = furniture_at_world(flame_probe_point(s), wall)) {
      f->damage(2);
      if (f->durability() <= 0) {
        destroy_furniture(*f);
      }
    }
  }
  else if (s.weapon_frame != 0) {
    s.weapon_frame =
        (s.weapon_frame >= FLAME_FADE_LAST) ? 0 : s.weapon_frame + 1;
  }
}

void AttackScene::tick_chemicals(AttackSprite& s) {
  if (!s.firing) {
    s.weapon_frame = 0;
  }
  else if (s.weapon_frame == 0) {
    s.weapon_frame = 1;
  }
  else {
    s.weapon_frame = (s.weapon_frame % CHEM_SPRAY_STEPS) + 1;
  }
}

void AttackScene::tick_bazooka(AttackSprite& s, bool advance_anim) {
  if (s.projectile_active && s.weapon_frame == 0) {
    const int dir = static_cast<int>(s.fire_dir);
    ++s.projectile_ctr;
    const int step = WALK_CELL_SIZE * s.projectile_ctr;
    const int dx = WALK_DX[dir];
    const int dy = WALK_DY[dir];
    s.projectile_pos.x += dx * step;
    s.projectile_pos.y += dy * step;

    const int distance_past_impact =
        ((s.projectile_pos.x - s.projectile_impact.x) * dx) +
        ((s.projectile_pos.y - s.projectile_impact.y) * dy);
    if (distance_past_impact >= 0) {
      s.projectile_pos = s.projectile_impact;
      s.weapon_frame = 1; // begin explosion
      bool wall = false;
      if (auto* f = furniture_at_world(s.projectile_impact, wall)) {
        destroy_furniture(*f);
      }
    }
    view_.mark_dirty();
  }
  else if (s.weapon_frame >= 1 && advance_anim) {
    // Explosion frames 1..6 (gated)
    ++s.weapon_frame;
    if (s.weapon_frame > 6) {
      s.weapon_frame = 0;
      s.projectile_active = false;
    }
    view_.mark_dirty();
  }
}

void AttackScene::tick_bomb(AttackSprite& s, bool advance_anim) {
  if (!s.bomb_active) {
    return;
  }
  if (s.bomb_fuse > 0) {
    --s.bomb_fuse;
    if (s.bomb_fuse == 0) {
      s.weapon_frame = BOMB_EXPLOSION_FIRST_FRAME; // fuse done: explode
    }
    view_.mark_dirty();
    return;
  }
  // Advance the explosion at the half rate (unlike the fuse)
  if (!advance_anim) {
    return;
  }
  if (s.weapon_frame == BOMB_EXPLOSION_FIRST_FRAME) {
    // Destroy every non-ashes piece whose center is within the blast box.
    const int dbl_blast = BOMB_BLAST * 2;
    WorldPoint blast_center = s.bomb_pos;
    const int widx = attack_weapon_index();
    if (widx >= 0 && widx <= 3) {
      const auto& w =
          context_.bob->get(compat::format("bob/nightw{}.bob", widx));
      const Size placed = w.frame(0).size();
      blast_center.x += placed.w / 2;
      blast_center.y += placed.h / 2;
    }
    bool any_destroyed = false;
    for (const auto& up : view_.current_furniture_list()) {
      PlacedFurniture* pf = up.get();
      if (pf->is_ashes()) {
        continue;
      }
      const Size sz = view_.bob_renderer_for(*pf).size(*pf);
      const WorldPoint c{pf->origin().x + (sz.w / 2),
                         pf->origin().y + (sz.h / 2)};
      if (std::abs(c.x - blast_center.x) < dbl_blast &&
          std::abs(c.y - blast_center.y) < dbl_blast) {
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
    // Reduce energy for any fighter caught in the blast
    const auto wound_if_in_blast = [&](AttackSprite* fighter) {
      if (!fighter || !fighter->combatant) {
        return;
      }
      if (std::abs(fighter->position.x - blast_center.x) < dbl_blast &&
          std::abs(fighter->position.y - blast_center.y) < dbl_blast) {
        for (int i = 0; i < BOMB_DAMAGE_ROLLS; ++i) {
          // Bombs can't kill anyone, only reduce energy
          (void)weapon_contact_roll(*fighter->combatant);
        }
      }
    };
    wound_if_in_blast(attacker_ ? &*attacker_ : nullptr);
    wound_if_in_blast(defender_ ? &*defender_ : nullptr);
  }
  ++s.weapon_frame;
  if (s.weapon_frame > BOMB_EXPLOSION_LAST_FRAME) {
    s.weapon_frame = 0;
    s.bomb_active = false;
  }
  view_.mark_dirty();
}

void AttackScene::render_weapon_effects(const AttackSprite& s) {
  using core::item::Item;

  const int widx = attack_weapon_index();
  if (widx < 0 || widx > 3) {
    return;
  }
  const auto& w = context_.bob->get(compat::format("bob/nightw{}.bob", widx));
  const int dir = static_cast<int>(s.fire_dir);
  const WorldPoint anchor{s.position.x - s.sprite_blit_offset_x,
                          s.position.y - s.sprite_blit_offset_y};

  switch (attack_weapon_) {
  case Item::FlameThrower: {
    // Render whenever the flame is active (not just firing) so the fade frames
    // plays after release
    if (s.weapon_frame == 0) {
      break;
    }
    // wf is the flame animation phase (see tick_flamethrower): 1..7 = fire
    // loop, 8..9 = fade-out. Fire uses 4 frames per phase (one per direction)
    // -> BOB frames 4..31; fade uses one non-directional frame per phase ->
    // 32, 33.
    const int wf = s.weapon_frame;
    const int frame =
        (wf <= 7) ? ((wf * 4) + dir) : (FLAME_FADE_FRAME_BASE + wf);
    view_.background_.blit_masked(
        w.frame(frame), ViewPoint{anchor.x + (FLAME_OFF_X[wf][dir] * 2),
                                  anchor.y + (FLAME_OFF_Y[wf][dir] * 2)});
    break;
  }
  case Item::Chemicals: {
    if (!s.firing) {
      break;
    }
    const int wf = s.weapon_frame;
    view_.background_.blit_masked(
        w.frame((wf * 4) + dir),
        ViewPoint{anchor.x + (CHEM_OFF_X[wf][dir] * 2),
                  anchor.y + (CHEM_OFF_Y[wf][dir] * 2)});
    break;
  }
  case Item::Bazooka: {
    if (s.projectile_active && s.weapon_frame == 0) {
      // In-flight grenade (frames 8-11, per dir)
      view_.background_.blit_masked(
          w.frame(BAZOOKA_FLIGHT_FRAME + dir),
          ViewPoint{s.projectile_pos.x, s.projectile_pos.y});
    }
    else if (s.weapon_frame >= 1) {
      // Explosion: wf 1 the directional burst (12-15); wf 2..6 the tail
      // (16-20).
      const int frame = (s.weapon_frame == 1)
                            ? (BAZOOKA_BURST_FRAME + dir)
                            : (BAZOOKA_TAIL_FRAME + (s.weapon_frame - 2));
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
    const Size placed = w.frame(0).size();
    const WorldPoint center{s.bomb_pos.x + (placed.w / 2),
                            s.bomb_pos.y + (placed.h / 2)};
    if (s.bomb_fuse > 0) {
      // Pulse between frame 0 and 1
      const int pulse = (s.bomb_fuse / BOMB_PULSE_DIVISOR) & 1;
      const auto& bf = w.frame(pulse);
      const Size bs = bf.size();
      int x = center.x - (bs.w / 2);
      int y = center.y - (bs.h / 2);
      view_.background_.blit_masked(bf, ViewPoint{x, y});
    }
    else {
      // Explosion frames 2..9. The legacy rise (BOMB_EXPLODE_DY) is a top-left
      // compensation, so in centered mode it is dropped.
      const auto& ef = w.frame(s.weapon_frame);
      const Size es = ef.size();
      int x = center.x - (es.w / 2);
      int y = center.y - (es.h / 2);
      view_.background_.blit_masked(ef, ViewPoint{x, y});
    }
#ifndef NDEBUG
    if (show_blast_debug_) {
      // Show detonation check: the box is the destruction area (a piece is hit
      // when its center is inside), each piece's center is a marker (red = in
      // range, green = safe).
      static constexpr gfx::ColorIndex BOX_COLOR = 68;
      static constexpr gfx::ColorIndex IN_COLOR = 68;
      static constexpr gfx::ColorIndex OUT_COLOR = 80;
      static constexpr int MARK = 4;
      // Must equal tick_bomb calculation, double blast
      const int half = BOMB_BLAST * 2;
      gfx::draw::box(
          view_.background_, ViewPoint{center.x - half, center.y - half},
          ViewPoint{center.x + half - 1, center.y + half - 1}, BOX_COLOR, 2);
      for (const auto& up : view_.current_furniture_list()) {
        const PlacedFurniture* pf = up.get();
        if (pf->is_ashes()) {
          continue;
        }
        const Size sz = view_.bob_renderer_for(*pf).size(*pf);
        const WorldPoint c{pf->origin().x + (sz.w / 2),
                           pf->origin().y + (sz.h / 2)};
        const bool inside =
            std::abs(c.x - center.x) < half && std::abs(c.y - center.y) < half;
        gfx::draw::box(view_.background_, ViewPoint{c.x - MARK, c.y - MARK},
                       ViewPoint{c.x + MARK, c.y + MARK},
                       inside ? IN_COLOR : OUT_COLOR, 1);
      }
    }
#endif
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
