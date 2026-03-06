/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include <vector>

#include "views/ui/popups/BasePopup.h"
#include "views/ui/popups/PopupButton.h"

class Restaurant;

struct GuestInfo {
  ConsumerType demographic;
  int face_index;
  bool been_talked_to;
  bool has_pizza;

  // Pizza data
  std::string pizza_name;
  int pizza_taste{0};
  Money pizza_price;
  int pizza_quality{0};
  int district_price_level{0};
};

class GuestTalkPopup : public BasePopup {
public:
  GuestTalkPopup(const GameContext& ctx, GuestInfo info,
                 const Restaurant& restaurant);
  void add_interactions(views::ui::InteractionContext ictx,
                        ViewPoint origin) override;
  void render(Surface& surface, ViewPoint origin) override;
  void tick() override;

private:
  GuestInfo info_;
  std::string text_;

  enum class AnimState { InitialBlink, Talking, Waiting };
  AnimState anim_state_{AnimState::InitialBlink};
  int initial_blink_tick_{0}; // ticks elapsed during opening blink
  int mouth_seq_{0};          // 0-3: index into MOUTH_SEQ[]
  int mouth_tick_{0};         // ticks remaining in this mouth phase
  int mouth_state_{0};        // 0=neutral, 1=frame1, 2=frame2
  int talk_cycles_{0};        // full mouth cycles completed (target: 5)

  // Independent blink system - runs in all states
  bool eye_blink_{false};
  int blink_cooldown_{0};
  int blink_duration_{0}; // ticks remaining in current blink

  void advance_blink();

  PopupButton btn_ok_{{101, 168, 100, 24}, "OK", [this] { finished(); }};

  std::string select_dialogue(const Restaurant& restaurant,
                              const std::vector<std::string>& lines) const;
};
