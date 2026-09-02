/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "core/finance/Money.h"
#include "engine/Math.h"
#include "gfx/RawRenderable.h"

// DOS version dimensions (most common)
#define PICTURE_WIDTH 52
#define PICTURE_HEIGHT 59
#define NUMBER_OF_CHARACTERS 100

namespace core::character {

enum class JobRole {
  CHEF,
  WAITER,
  MANAGER,
  NONE,
};

constexpr std::string_view to_string(core::character::JobRole job) noexcept {
  switch (job) {
  case core::character::JobRole::CHEF:
    return "COOK";
  case core::character::JobRole::WAITER:
    return "WAITER";
  case core::character::JobRole::MANAGER:
    return "MANAGER";
  case core::character::JobRole::NONE:
    return "NONE";
  }
  return "unknown";
}

struct CharacterData : public RawRenderable {
  uint8_t id;
  // Public data members
  std::string first_name;
  std::string last_name;
  uint8_t age;
  uint8_t popularity;
  uint8_t health;
  uint8_t energy;
  uint8_t intelligence;
  uint8_t charisma;
  uint8_t dexterity;
  uint8_t personality;
  uint8_t courage;
  uint8_t experience;
  uint8_t trustworthiness;
  uint8_t honesty; // Hidden stat for theft prevention (character file +0x5D)
  uint8_t cooking;
  uint8_t serving;
  uint8_t management;
  uint8_t motivation;
  int pizzas_per_day;
  Money starting_money{0};
  uint8_t gender_flag{
      0}; // 0=male, 1=female (for pregnancy) - character file +0x2B
  virtual ~CharacterData() = default;
  [[nodiscard]] int picture_width() const override {
    // Use actual dimensions if set, otherwise fall back to defaults
    return picture_width_ > 0 ? picture_width_ : PICTURE_WIDTH;
  };
  [[nodiscard]] int picture_height() const override {
    return picture_height_ > 0 ? picture_height_ : PICTURE_HEIGHT;
  };
  [[nodiscard]] std::span<const uint8_t> picture_data() const override {
    return picture_data_;
  };
  void picture_data(std::vector<unsigned char> picture_data) {
    picture_data_ = std::move(picture_data);
  };
  void set_picture_dimensions(int width, int height) {
    picture_width_ = width;
    picture_height_ = height;
  };
  uint8_t palette_index() const override { return 38; };
  Money starting_payment(JobRole role) const {
    // This is the implementation as the original does it
    int base = serving;

    switch (role) {
    case JobRole::CHEF:
      base += cooking;
      break;
    case JobRole::WAITER:
      base += management;
      break;
    case JobRole::MANAGER:
      base += 6 * management;
      break;
    default:
      break;
    }

    base += pizzas_per_day;
    base += trustworthiness;

    /* intelligence factor (percent) */
    int factor = 100 + intelligence / 2;

    /* scale and round */
    int pay = (base * factor) / 100;
    return Money::from_dollars(math::round_up_to_next_power10<1>(pay));
  }

private:
  std::vector<uint8_t> picture_data_;
  int picture_width_ = 0;  // Actual decoded width (0 = use PICTURE_WIDTH)
  int picture_height_ = 0; // Actual decoded height (0 = use PICTURE_HEIGHT)
};

constexpr int MULTIPLIER_CONSTANT = 220; // From the original

class Character : public RawRenderable {
public:
  Character(CharacterData*);

  // Use the default copy semantics.
  Character(const Character&) = default;
  Character& operator=(const Character&) = default;

  // Getters for the basic stats:
  uint8_t id() const { return data_->id; };
  const std::string& first_name() const { return first_name_; };
  const std::string& last_name() const { return last_name_; };
  std::string name() const { return first_name() + " " + last_name(); };
  uint8_t age() const { return age_; };
  void age(uint8_t a) { age_ = a; };
  uint8_t popularity() const { return popularity_; };
  uint8_t health() const { return health_; };
  uint8_t energy() const { return energy_; };
  uint8_t intelligence() const { return intelligence_; };
  uint8_t charisma() const { return charisma_; };
  uint8_t dexterity() const { return dexterity_; };
  uint8_t personality() const { return personality_; };
  uint8_t courage() const { return courage_; };
  uint8_t experience() const { return experience_; };
  uint8_t trustworthiness() const { return trustworthiness_; };
  uint8_t honesty() const { return honesty_; };
  uint8_t cooking() const { return cooking_; };
  uint8_t serving() const { return serving_; };
  uint8_t management() const { return management_; };
  uint8_t motivation() const { return motivation_; };
  int pizzas_per_day() const { return pizzas_per_day_; };
  Money starting_money() const { return data_->starting_money; };
  uint8_t gender_flag() const { return data_->gender_flag; };
  uint32_t popularity_multiplier() const {
    return popularity_ / 2 + MULTIPLIER_CONSTANT;
  };
  uint32_t courage_multiplier() const {
    return courage_ / 2 + MULTIPLIER_CONSTANT;
  };

  // Returns a CharacterData snapshot with current runtime values
  CharacterData to_character_data() const;

  // Setters for the things that can change:
  void name(std::string first, std::string last) {
    first_name_ = std::move(first);
    last_name_ = std::move(last);
  }
  void popularity(uint8_t popularity) { popularity_ = popularity; };
  void personality(uint8_t personality) { personality_ = personality; };
  void courage(uint8_t courage) { courage_ = courage; };
  void experience(uint8_t experience) { experience_ = experience; };
  void charisma(uint8_t charisma) { charisma_ = charisma; };
  void dexterity(uint8_t dexterity) { dexterity_ = dexterity; };
  void adjust_courage(int delta) {
    courage_ = static_cast<uint8_t>(std::clamp(courage_ + delta, 0, 255));
  }
  void adjust_popularity(int delta) {
    popularity_ = static_cast<uint8_t>(std::clamp(popularity_ + delta, 0, 255));
  }
  void adjust_personality(int delta) {
    personality_ =
        static_cast<uint8_t>(std::clamp(personality_ + delta, 0, 255));
  }
  void health(uint8_t health) { health_ = health; };
  void energy(uint8_t energy) { energy_ = energy; };
  void intelligence(uint8_t intelligence) { intelligence_ = intelligence; };
  void cooking(uint8_t cooking) { cooking_ = cooking; };
  void serving(uint8_t serving) { serving_ = serving; };
  void management(uint8_t management) { management_ = management; };
  void motivation(uint8_t motivation) {
    motivation_ = std::min(motivation, static_cast<uint8_t>(100));
  };
  void trustworthiness(uint8_t trust) {
    trustworthiness_ = std::min(trust, static_cast<uint8_t>(100));
  };
  void honesty(uint8_t hon) {
    honesty_ = std::min(hon, static_cast<uint8_t>(100));
  };
  void pizzas_per_day(int ppd) { pizzas_per_day_ = ppd; };

  int picture_width() const override;
  int picture_height() const override;
  std::span<const uint8_t> picture_data() const override;
  uint8_t palette_index() const override;
  void picture_data(std::vector<unsigned char>);

private:
  // The cast record holds the portrait and the values every copy of this
  // character starts from; everything below it can drift per player.
  const CharacterData* data_;
  std::string first_name_;
  std::string last_name_;
  uint8_t charisma_;
  uint8_t dexterity_;
  uint8_t age_;
  uint8_t popularity_;
  uint8_t personality_;
  uint8_t courage_;
  uint8_t health_;
  uint8_t energy_;
  uint8_t intelligence_;
  uint8_t experience_;
  uint8_t cooking_;
  uint8_t serving_;
  uint8_t management_;
  uint8_t motivation_;
  uint8_t trustworthiness_;
  uint8_t honesty_;
  int pizzas_per_day_;
};

} // namespace core::character
