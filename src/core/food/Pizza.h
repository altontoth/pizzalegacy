/*
 * SPDX-FileCopyrightText: 2014 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <functional>
#include <memory>
#include <vector>
#include "core/Demographics.h"
#include "core/food/CustomerTasteProfiles.h"
#include "core/food/PizzaJudging.h"
#include "core/food/Topping.h"

#include <iostream>
#include <ostream>

#define NUMBER_OF_JUDGES 5

namespace core::food {

enum class Origin { Canonical, User };

struct Flavors {
  // These are percentages, e.g. 80 = 80%
  int fat = 0;
  int sweet = 0;
  int salty = 0;
  int hot = 0;
};

std::ostream& operator<<(std::ostream& os, const class Pizza& pizza);

class Pizza {
  friend std::ostream& operator<<(std::ostream& os, const Pizza& pizza);

public:
  Pizza();
  Pizza(std::string);
  Pizza(const Pizza&) = delete;
  Pizza& operator=(const Pizza&) = delete;
  // Move Constructor
  Pizza(Pizza&& other) noexcept = default;
  Pizza& operator=(Pizza&& other) noexcept = default;

  void add_topping(std::unique_ptr<Topping>);
  std::unique_ptr<Topping> remove_latest_topping();
  const std::vector<std::unique_ptr<Topping>>& toppings() const;
  const std::array<int, NUMBER_OF_JUDGES>& scores() const;
  void finalize(const CustomerTasteProfiles& taste_profiles,
                const std::string& name = "");
  void finalize_against(const Pizza& reference, PizzaJudging judging,
                        const CustomerTasteProfiles& taste_profiles,
                        const std::string& name = "");
  bool is_finalized() const noexcept { return finalized_; }
  // How far this pizza is from the reference by its ingredients and their
  // amounts, as a percentage: 0 is a match, 100 has nothing in common.
  int compare_pizza_composition(const Pizza& other) const;
  // How far this pizza is from the reference pizza by every topping position,
  // chop and rotation state. 0 is a match, 100 has nothing in common.
  int compare_to_other_pizza(const Pizza& other) const;
  static int topping_comparison_score(const Topping& ref_topping,
                                      const Topping& user_topping);
  const std::string& name() const { return name_; }
  void name(std::string n) { name_ = std::move(n); }
  Popularity popularity() const { return popularity_; };
  int calories() const { return calories_; };
  Flavors flavors() const { return flavors_; };
  void update_stats(const CustomerTasteProfiles& taste_profiles);
  int appearance() const { return appearance_; }
  int refinement() const { return refinement_; }
  int normality() const { return normality_; }
  int imitation() const { return imitation_; }
  int herb_score() const { return herb_score_; }
  void set_origin(Origin origin) { origin_ = origin; }
  Origin origin() const { return origin_; }

  // A perfect match of the reference recipe.
  static constexpr int full_imitation = 100;

  // Set stats directly from disk values (bypasses calculation)
  void set_disk_stats(const Flavors& flavors, int appearance, int refinement,
                      int normality, int imitation,
                      const CustomerTasteProfiles& taste_profiles);

  // Set the stats an original save's recipe node persists. Flavors and
  // normality are recomputed from the toppings; only these three are stored.
  void set_saved_stats(int appearance, int refinement, int imitation,
                       const CustomerTasteProfiles& taste_profiles);

  // Calculates demographic popularity scores (0-100 for each of 6 groups).
  void
  calculate_demographic_scores(const CustomerTasteProfiles& taste_profiles);

private:
  void compute_physical_stats();
  [[nodiscard]] Flavors calculate_flavor_percentages() const;
  int calculate_calories();

  // Returns 0..100. Calculates appearance based on topping distribution
  // relative to pizza center. Uses approximation from notes.txt.
  [[nodiscard]] int calculate_appearance() const;

  // Returns 0..100. Based on average chop level (0=whole,1=chopped,2=minced).
  [[nodiscard]] int calculate_refinement() const;

  // Returns 0..100. Quality factor based on ingredient variety and weight.
  [[nodiscard]] int calculate_normality() const;

  // Returns 0..10. Based on total weight of herb toppings divided by 3.
  [[nodiscard]] int calculate_herb_score() const;

  // Calculates judge scores (0-20 for each of 5 judges).
  void calculate_judge_scores();

  std::vector<std::unique_ptr<Topping>> toppings_{};
  std::string name_{};
  std::array<int, NUMBER_OF_JUDGES> scores_{};
  Popularity popularity_{0, 0, 0, 0, 0, 0};
  Origin origin_{Origin::User};
  bool finalized_{false};
  int calories_{0};
  Flavors flavors_{};
  int appearance_{0};
  int refinement_{0};
  int normality_{0};
  int imitation_{full_imitation};
  int herb_score_{0}; // 0-10, herb weight / 3
};

} // namespace core::food
