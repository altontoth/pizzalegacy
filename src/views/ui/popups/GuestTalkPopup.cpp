/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/popups/GuestTalkPopup.h"

#include <algorithm>

#include "core/Demographics.h"
#include "core/restaurant/Restaurant.h"
#include "engine/Random.h"
#include "engine/geom/Spaces.h"
#include "gfx/Draw.h"
#include "resources/FontTypes.h"
#include "resources/util/FileUtils.h"

namespace {

// Source rects within gfx/g%d.vga
// The VGA sheet is 128x92 pixels (64x46 at 1x):
//   Left 76x76:  base face (eyes open, mouth neutral)
//   Right 52x76: talking mouth frames (frame1 top, frame2 bottom)
//   Bottom row:  eye blink frames packed left to right,
//                but I think only one is used and they're all the same

ViewRect face_main(0 * 2, 0 * 2, 38 * 2, 38 * 2);       // full base face
ViewRect mouth_frame_1(38 * 2, 0 * 2, 26 * 2, 19 * 2);  // open mouth
ViewRect mouth_frame_2(38 * 2, 19 * 2, 26 * 2, 19 * 2); // wider open
// Ignore the other two eye frames that are unused
ViewRect eyes_frame_1(0 * 2, 38 * 2, 21 * 2, 8 * 2); // blink

constexpr ViewPoint face_pos{16, 16};
constexpr PxOffset eyes_o{9 * 2, 10 * 2};
constexpr PxOffset mouth_o{6 * 2, 19 * 2};

// Mouth animation sequence: {frame_state, tick_duration}
// frame 0 = neutral, 1 = frame1 (open), 2 = frame2 (wider)
constexpr std::pair<int, int> MOUTH_SEQ[4] = {
    {1, 3}, // open for 3 ticks
    {2, 4}, // wider open for 4 ticks
    {1, 3}, // open for 3 ticks
    {0, 7}, // neutral for 7 ticks
};

constexpr int TALK_CYCLES = 5;
constexpr int INITIAL_BLINK_TICKS = 5;
constexpr int BLINK_DURATION = 5;
constexpr int BLINK_MIN_INTERVAL = 60;
constexpr int BLINK_RANDOM_RANGE = 90; // adds 0-89 ticks

std::vector<std::string> load_gtalk(bool formal) {
  auto lines = fileutils::load_text_lines(formal ? "t/gtalk1.e" : "t/gtalk0.e");
  lines.insert(lines.begin(), ""); // index 0 unused - lines are 1-based
  return lines;
}

} // namespace

GuestTalkPopup::GuestTalkPopup(const GameContext& ctx, GuestInfo info,
                               const Restaurant* restaurant)
    : BasePopup(ctx), info_(info) {
  // Demographic groups 0-2 -> casual (GTALK0), 3-5 -> formal (GTALK1)
  const bool formal = (static_cast<int>(info_.demographic) / 3) != 0;
  const auto lines = load_gtalk(formal);
  text_ = select_dialogue(restaurant, lines);

  // Prime the mouth state machine (initial blink happens first)
  mouth_tick_ = MOUTH_SEQ[0].second;
  mouth_state_ = 0;
  // Start with eyes closed for the initial blink
  eye_blink_ = true;
}

std::string
GuestTalkPopup::select_dialogue(const Restaurant* restaurant,
                                const std::vector<std::string>& lines) const {

  auto line = [&](int n) -> std::string {
    if (n >= 1 && n < static_cast<int>(lines.size())) {
      return lines[n];
    }
    return "unknown string";
  };

  // picks between the two lines within each rating tier
  const int variation = engine::random::rand_max_n(2);

  // Repeat click
  if (info_.been_talked_to) {
    return line(57); // "What's wrong now?" / "What do you want now?"
  }

  // 10% flat refusal
  if (engine::random::rand_percentage_chance(10)) {
    return line(56); // "Get lost, old man..."
  }

  // No pizza yet (or empty menu)
  if (!info_.has_pizza || restaurant->count_menu_items() == 0) {

    if (engine::random::coin_flip()) {
      // Branch A: furniture / decor comment
      const auto demo = static_cast<ConsumerType>(info_.demographic);
      const int score = restaurant->economy().furniture_appeal()[demo];
      int n;
      if (score < 5) {
        n = 37; // "You should knock the whole lot down!"
      }
      else {
        n = std::clamp(36 - (score / 16), 31, 36);
      }
      return line(n);
    }

    // Branch B: music or atmosphere

    // 20% trigger: music comment (if jukebox is set)
    const int music = restaurant->jukebox_music(); // 0=off, 1-6=demographic+1
    if (music != 0) {
      const int preferred = music - 1;
      const int diff =
          std::abs(preferred - static_cast<int>(info_.demographic));
      if (diff == 0) {
        return line(44);
      } // "The music is absolutely great!"
      if (diff > 1) {
        return line(45);
      } // "What's that horrible racket..."
      // diff == 1: adjacent demographic, fall through to atmosphere
    }

    // Atmosphere fallback: map total_popularity (roughly -100 to +200) to
    // lines 35-42. Shift by 100 to make the neutral restaurant land in the
    // middle of that range.
    const int pop = std::clamp(
        static_cast<int>(restaurant->economy().total_popularity()) + 100, 0,
        245);
    return line(std::clamp((pop / 35) + 35, 35, 42));
  }

  const std::string prefix =
      compat::format("I am eating {}. ", info_.pizza_name);

  auto pizza_line = [&](int n) { return prefix + line(n); };

  switch (engine::random::rand_max_n(3)) {
  case 0: {
    // Taste
    const int rating = std::min(info_.pizza_taste / 19, 4);
    printf("rating %d, piz taste %d\n", rating, info_.pizza_taste);
    return pizza_line(std::clamp(9 + variation - (rating * 2), 1, 10));
  }
  case 1: {
    // Price
    const int threshold = 900 - (info_.district_price_level * 6);
    const int excess = static_cast<int>(
        std::max(int64_t{0}, info_.pizza_price.cents - threshold));
    const int base_line = std::min(excess / 300, 3) + 11;
    printf("Thres %d, excess %d, base %d, dis price level %d\n", threshold,
           excess, base_line, info_.district_price_level);
    return pizza_line(base_line + (variation * 4));
  }
  default: {
    // Quality
    const int diff = info_.pizza_quality - 25;
    const int rating = (diff < 0) ? 0 : std::min(diff / 14, 5);
    printf("Diff %d, rating %d, piz qual %d\n", diff, rating,
           info_.pizza_quality);
    return pizza_line(std::clamp(19 + variation + ((5 - rating) * 2), 19, 30));
  }
  }
}

void GuestTalkPopup::add_interactions(views::ui::InteractionContext ictx,
                                      ViewPoint /* origin */) {
  // Click anywhere dismisses
  ViewRect click_area{0, 0, DisplayConstants::SCREEN_WIDTH,
                      DisplayConstants::SCREEN_HEIGHT};
  ictx.layers.create_frame_item(
      click_area, "gtalk_popup", [this](views::ui::InteractionEvent ie) {
        if (ie.button != views::ui::MouseButton::Left ||
            ie.type != views::ui::InteractionEvent::Type::ButtonDown) {
          return;
        }
        finished();
      });
}

void GuestTalkPopup::render(Surface& surface, ViewPoint origin) {
  const Size box_size{302, 202};
  Surface s(box_size, PaletteHandle{4}, 15);
  const ViewRect outer{ViewPoint{0, 0}, box_size};
  const int c1 = 17;
  const int c2 = 16;
  const int c3 = 18;
  gfx::draw::box(s, outer.inset(0, 0), c1, 2);
  gfx::draw::box(s, outer.inset(2, 2), c2, 2);
  gfx::draw::box(s, outer.inset(4, 4), c1, 2);
  gfx::draw::box(s, outer.inset(6, 6), c3, 2);

  // Face portrait, get the one for this demographic
  const auto face_key = compat::format("gfx/g{}", info_.face_index);
  const auto& guest_pic = vga_->get(face_key);

  // Base face
  // Black background behind the face:
  s.fill_rect({face_pos, face_main.size()}, 240);
  s.blit(guest_pic, face_main, face_pos);

  // Eye overlay (blink animation):
  if (eye_blink_) {
    s.blit(guest_pic, eyes_frame_1, face_pos + eyes_o);
  }

  // Mouth overlay (talking animation):
  if (mouth_state_ == 1) {
    s.blit(guest_pic, mouth_frame_1, face_pos + mouth_o);
  }
  else if (mouth_state_ == 2) {
    s.blit(guest_pic, mouth_frame_2, face_pos + mouth_o);
  }

  Surface text(Size{180, 160}, PaletteHandle{4});
  text.fill(15);
  font_->render_text_onto(text, text_, ViewPoint{0, 0}, FontFace::PIZ_NUMB,
                          240);
  s.blit_raw(text, ViewPoint{100, 28});
  surface.palette({4});
  surface.blit_raw(s, origin);
}

void GuestTalkPopup::advance_blink() {
  if (blink_duration_ > 0) {
    // Currently blinking - count down
    --blink_duration_;
    eye_blink_ = true;
    if (blink_duration_ <= 0) {
      eye_blink_ = false;
      blink_cooldown_ =
          BLINK_MIN_INTERVAL + engine::random::rand_max_n(BLINK_RANDOM_RANGE);
    }
  }
  else if (blink_cooldown_ > 0) {
    --blink_cooldown_;
  }
  else {
    // Start a new blink
    blink_duration_ = BLINK_DURATION;
    eye_blink_ = true;
  }
}

void GuestTalkPopup::tick() {
  bool dirty = false;
  bool prev_blink = eye_blink_;

  switch (anim_state_) {

  case AnimState::InitialBlink:
    // Hold eyes closed for the opening blink, then start talking
    ++initial_blink_tick_;
    if (initial_blink_tick_ > INITIAL_BLINK_TICKS) {
      eye_blink_ = false;
      anim_state_ = AnimState::Talking;
      mouth_seq_ = 0;
      mouth_tick_ = MOUTH_SEQ[0].second;
      mouth_state_ = MOUTH_SEQ[0].first;
      // Schedule first independent blink after a short delay
      blink_cooldown_ =
          BLINK_MIN_INTERVAL + engine::random::rand_max_n(BLINK_RANDOM_RANGE);
    }
    dirty = true;
    break;

  case AnimState::Talking:
    advance_blink();
    if (--mouth_tick_ <= 0) {
      ++mouth_seq_;
      if (mouth_seq_ >= 4) {
        mouth_seq_ = 0;
        if (++talk_cycles_ >= TALK_CYCLES) {
          mouth_state_ = 0;
          anim_state_ = AnimState::Waiting;
          break;
        }
      }
      mouth_state_ = MOUTH_SEQ[mouth_seq_].first;
      mouth_tick_ = MOUTH_SEQ[mouth_seq_].second;
    }
    dirty = true;
    break;

  case AnimState::Waiting:
    advance_blink();
    break;
  }

  if (dirty || eye_blink_ != prev_blink) {
    mark_dirty();
  }
}
