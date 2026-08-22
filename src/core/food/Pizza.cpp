/*
 * SPDX-FileCopyrightText: 2014 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/food/Pizza.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>
#include "core/food/Ingredient.h"
#include "engine/Logger.h"

constexpr int MAX_SCORE = 100;

namespace {
// Pizza center coordinates
constexpr int PIZZA_CENTER_X = 116 * 2;
constexpr int PIZZA_CENTER_Y = 117 * 2;

// Appearance calculation
constexpr int APPEARANCE_DISTANCE_THRESHOLD = 60 * 2;
constexpr int APPEARANCE_PENALTY_DIVISOR = 8;

// The dough counts toward the mass the flavours are diluted into, but not
// toward the normality weight thresholds.
constexpr int DOUGH_BASELINE_WEIGHT = 250;

// Weight thresholds for normality
constexpr int WEIGHT_THRESHOLD_LIGHT = 700;
constexpr int WEIGHT_THRESHOLD_VERY_LIGHT = 400;

// Normality penalties
constexpr int NORMALITY_MIN_CATEGORIES = 5;
constexpr int NORMALITY_CATEGORY_BASE = 70;
constexpr int NORMALITY_CATEGORY_STEP = 10;

// Refinement calculation
constexpr int REFINEMENT_DIVISOR = 2;

// Judge scoring
constexpr int JUDGE_MAX_SCORE = 20;
constexpr int JUDGE_INGREDIENT_THRESHOLD = 10;
constexpr int JUDGE_TOPPING_THRESHOLD = 28;
constexpr int JUDGE_FLAVOR_DIVISOR = 10; // Scale flavors from 0-100 to 0-10

// Demographic scoring
constexpr int DEMOGRAPHIC_APPEARANCE_THRESHOLD = 60;
constexpr int DEMOGRAPHIC_FLAVOR_DIVISOR = 10;
} // namespace

namespace core::food {

auto operator<<(std::ostream& os, const Pizza& pizza) -> std::ostream& {
  os << "Pizza:\n";
  os << "  name = " << pizza.name_ << "\n";
  os << "  toppings = " << "\n";
  for (const auto& topping : pizza.toppings_) {
    std::cout << "topping " << *topping;
    os << "    " << *topping;
  }

  return os;
}

Pizza::Pizza() : Pizza("") { scores_.fill(0xFF); }

Pizza::Pizza(std::string name) : name_(std::move(name)) { scores_.fill(0xFF); }

void Pizza::add_topping(std::unique_ptr<Topping> topping) {
  // Logger::debug("Adding topping to Pizza {}", name.c_str());
  toppings_.push_back(std::move(topping));
}

std::unique_ptr<Topping> Pizza::remove_latest_topping() {
  if (!toppings_.empty()) {
    auto t = std::move(toppings_.back());
    toppings_.pop_back();
    return t;
  }
  return nullptr;
}

void Pizza::finalize(const CustomerTasteProfiles& taste_profiles,
                     const std::string& name) {
  if (name != "") {
    name_ = name;
  }
  update_stats(taste_profiles);
  finalized_ = true;
}

void Pizza::finalize_against(const Pizza& reference,
                             const CustomerTasteProfiles& taste_profiles,
                             const std::string& name) {
  if (name != "") {
    name_ = name;
  }
  compute_physical_stats();

  // Compare this pizza to the reference one
  int mismatch = compare_to_other_pizza(reference);

  // Convert mismatch to a score (0 = perfect match, 100 = worst)
  // Higher mismatch means lower score
  int total_score = MAX_SCORE - mismatch;
  total_score = std::clamp(total_score, 0, MAX_SCORE);

  // Set imitation percentage (how well this pizza matches the reference)
  imitation_ = total_score;
  // For competitions this is not really true but the pizza is not saved anyway,
  // so we just leave it:
  origin_ = Origin::Canonical;

  // Distribute the score across judges using integer division
  // Example: 93 / 5 = 18 remainder 3
  // So: 2 judges give 18, and 3 judges give 19
  int base_score = total_score / NUMBER_OF_JUDGES;
  int remainder = total_score % NUMBER_OF_JUDGES;
  int num_lower = NUMBER_OF_JUDGES - remainder;

  // First num_lower judges get base_score, then num_higher get base_score+1
  for (int i = 0; i < NUMBER_OF_JUDGES; ++i) {
    if (i < num_lower) {
      scores_[i] = base_score;
    }
    else {
      scores_[i] = base_score + 1;
    }
  }

  calculate_demographic_scores(taste_profiles);
  finalized_ = true;
}

void Pizza::compute_physical_stats() {
  flavors_ = calculate_flavor_percentages();

  float calories_sum = 0;
  for (const auto& topping : toppings()) {
    calories_sum += topping->calories();
  }
  calories_ = static_cast<int>(calories_sum);

  appearance_ = calculate_appearance();
  refinement_ = calculate_refinement();
  normality_ = calculate_normality();
  herb_score_ = calculate_herb_score();
}

void Pizza::update_stats(const CustomerTasteProfiles& taste_profiles) {
  compute_physical_stats();
  calculate_judge_scores();
  calculate_demographic_scores(taste_profiles);
}

const std::vector<std::unique_ptr<Topping>>& Pizza::toppings() const {
  return toppings_;
}

const std::array<int, NUMBER_OF_JUDGES>& Pizza::scores() const {
  return scores_;
}

int Pizza::compare_to_other_pizza(const Pizza& other) const {
  const auto& other_toppings = other.toppings();
  const auto& this_toppings = this->toppings();

  int mismatch_total = 0;
  int ref_count = 0; // how many reference toppings we actually compare

  // The original loops up to 96 toppings from the reference pizza
  std::size_t count = std::min<std::size_t>(96, other_toppings.size());

  for (std::size_t i = 0; i < count; ++i) {
    const auto& ref_topping = other_toppings[i];
    int best_so_far = MAX_SCORE;

    // Compare it to every topping in "this" pizza
    for (const auto& user_topping : this_toppings) {
      int diff = topping_comparison_score(*ref_topping.get(), *user_topping);
      best_so_far = std::min(diff, best_so_far);
      // If we ever find a 0 difference, we can early-out:
      if (best_so_far == 0) {
        break;
      }
    }

    mismatch_total += best_so_far;
    ++ref_count;
  }

  if (ref_count > 0) {
    // Average mismatch in [0..100]
    int average_mismatch = mismatch_total / ref_count;
    return average_mismatch;
  }
  // No toppings in reference pizza => no mismatch
  return 0;
}

int Pizza::topping_comparison_score(const Topping& ref_topping,
                                    const Topping& user_topping) {
  // Check if ingredients differ entirely, if they differ
  // the mismatch is 100 right away.)
  if (ref_topping.ingredient()->name() != user_topping.ingredient()->name()) {
    return MAX_SCORE;
  }

  // Compare chop state & rotation state
  int ref_chop = ref_topping.chopped_state();
  int ref_rot = ref_topping.rotation_state();
  int usr_chop = user_topping.chopped_state();
  int usr_rot = user_topping.rotation_state();

  int difference = 0;
  bool same_chop = (ref_chop == usr_chop);
  bool same_rot = (ref_rot == usr_rot);
  if (same_chop && same_rot) { // exact match
    difference = 0;
  }
  else if (same_chop ^ same_rot) { // exactly one is true
    difference = 20;
  }
  else {
    difference = 60;
  }

  WorldDelta dp = ref_topping.origin() - user_topping.origin();
  difference += std::abs(dp.dx) / 2;
  difference += std::abs(dp.dy) / 2;

  // Clamp to [0..100] just in case
  difference = std::clamp(difference, 0, 100);

  return difference;
}

Flavors Pizza::calculate_flavor_percentages() const {
  int sum_fat = 0;
  int sum_sweet = 0;
  int sum_salty = 0;
  int sum_hot = 0;

  int total_weight = DOUGH_BASELINE_WEIGHT;

  for (const auto& ut : toppings()) {
    if (!ut) {
      continue;
    }
    const auto& t = *ut;
    const auto* ing = t.ingredient();
    const int w = t.weight();
    if (w <= 0) {
      continue;
    }
    const int scale = (ing->intensity() >> 1); // same as SAR 1 in the exe

    sum_fat += ing->fat() * w * scale;
    sum_sweet += ing->sweet() * w * scale;
    sum_salty += ing->salty() * w * scale;
    sum_hot += ing->hot() * w * scale;

    total_weight += w;
  }

  // The original keeps each taste as 0-10 and truncates the ratio; we store it
  // scaled to 0-100.
  auto taste = [total_weight](int s) -> int {
    if (total_weight <= 0) {
      return 0;
    }
    return std::clamp(s / total_weight, 0, 10) * 10;
  };

  Flavors out;
  out.fat = taste(sum_fat);
  out.sweet = taste(sum_sweet);
  out.salty = taste(sum_salty);
  out.hot = taste(sum_hot);
  return out;
}

int Pizza::calculate_appearance() const {
  // Logger::debug("=== APPEARANCE CALCULATION ===");
  // Logger::debug("  Pizza center: ({}, {})", PIZZA_CENTER_X, PIZZA_CENTER_Y);
  // Logger::debug("  Number of toppings: {}", toppings_.size());

  // Empty pizza has poor appearance
  if (toppings_.empty()) {
    // Logger::debug("  No toppings, appearance = 0");
    return 0;
  }

  int penalty = 0;

  auto excess_over_threshold = [](int v) -> int {
    v = std::abs(v);
    return (v > APPEARANCE_DISTANCE_THRESHOLD)
               ? (v - APPEARANCE_DISTANCE_THRESHOLD)
               : 0;
  };

  for (const auto& topping : toppings_) {
    if (!topping) {
      continue;
    }
    const auto origin = topping->origin();
    int dx_excess = excess_over_threshold(origin.x - PIZZA_CENTER_X);
    int dy_excess = excess_over_threshold(origin.y - PIZZA_CENTER_Y);
    // Logger::debug("    Topping {} at ({}, {}) - dx_excess={}, dy_excess={}",
    //               topping->ingredient()->name(), origin.x, origin.y,
    //               dx_excess, dy_excess);
    penalty += dx_excess + dy_excess;
  }

  // appearance = clamp(100 - (penalty >> 3), 0, 100)
  int appearance = 100 - (penalty / APPEARANCE_PENALTY_DIVISOR);
  appearance = std::clamp(appearance, 0, 100);
  // Logger::debug("  Total penalty: {}", penalty);
  // Logger::debug("  Final appearance: {}", appearance);
  return appearance;
}

int Pizza::calculate_refinement() const {
  // Logger::debug("=== REFINEMENT CALCULATION ===");

  if (toppings_.empty()) {
    // Logger::debug("  No toppings, refinement = 0");
    return 0;
  }

  unsigned sum_chop = 0;
  for (const auto& topping : toppings_) {
    if (!topping) {
      continue;
    }
    int chop_state = topping->chopped_state();
    // Logger::debug("    {} - chop state: {}", topping->ingredient()->name(),
    //               chop_state);
    sum_chop += chop_state; // 0=whole, 1=chopped, 2=minced
  }

  // refinement = clamp((sum_of_chop_quarters * 100) / (2 *
  // number_of_placements), 0, 100)
  unsigned placements = static_cast<unsigned>(toppings_.size());
  unsigned denom = REFINEMENT_DIVISOR * placements;
  unsigned nom = 100u * sum_chop;
  unsigned pct = nom / denom;

  int refinement = (pct > 100) ? 100 : static_cast<int>(pct);
  // Logger::debug("  Sum chop states: {}", sum_chop);
  // Logger::debug("  Placements: {}", placements);
  // Logger::debug("  Formula: ({} * 100) / ({} * {}) = {}", sum_chop,
  //               REFINEMENT_DIVISOR, placements, refinement);
  // Logger::debug("  Final refinement: {}", refinement);

  return refinement;
}

int Pizza::calculate_normality() const {
  // Logger::debug("=== NORMALITY CALCULATION ===");

  // Start at 100%
  int normality = 100;

  // Count unique ingredient types
  std::set<std::string> ingredient_types;
  int total_weight = 0;

  for (const auto& topping : toppings_) {
    if (!topping) {
      continue;
    }
    const auto* ingredient = topping->ingredient();
    int category = static_cast<int>(ingredient->category());
    const char* cat_name[] = {"vegetables", "meat", "fruit", "cheese/herbs"};
    // Logger::debug("    {} -> category {} ({})", ingredient->name(), category,
    //               category < 4 ? cat_name[category] : "unknown");
    ingredient_types.insert(ingredient->name());
    total_weight += topping->weight();
  }

  int ingredient_type_count = static_cast<int>(ingredient_types.size());
  // Logger::debug("  Unique ingredient categories: {}", ingredient_type_count);
  // Logger::debug("  Topping weight: {}g", total_weight);
  // Logger::debug("  Starting normality: {}", normality);

  // If ingredient type count < NORMALITY_MIN_CATEGORIES: reduce by percentage
  // Original: reduceField52ByPercentage(pizza, 70 - count*10)
  // which does: normality = normality * (100 - penalty) / 100
  if (ingredient_type_count < NORMALITY_MIN_CATEGORIES) {
    int penalty = NORMALITY_CATEGORY_BASE -
                  (NORMALITY_CATEGORY_STEP * ingredient_type_count);
    int old_normality = normality;
    normality = (normality * (100 - penalty)) / 100;
    // Logger::debug("  < {} categories penalty: {} * (100 - {}) / 100 = {} (was
    // {})",
    //               NORMALITY_MIN_CATEGORIES, old_normality, penalty,
    //               normality, old_normality);
  }

  // If weight < WEIGHT_THRESHOLD_LIGHT: reduce by 20%
  if (total_weight < WEIGHT_THRESHOLD_LIGHT) {
    int old_normality = normality;
    normality = (normality * 80) / 100;
    // Logger::debug("  < {}g penalty: {} * 80 / 100 = {} (was {})",
    //               WEIGHT_THRESHOLD_LIGHT, old_normality, normality,
    //               old_normality);
  }

  // If weight < WEIGHT_THRESHOLD_VERY_LIGHT: reduce by 50% (additional)
  if (total_weight < WEIGHT_THRESHOLD_VERY_LIGHT) {
    int old_normality = normality;
    normality = (normality * 50) / 100;
    // Logger::debug("  < {}g penalty: {} * 50 / 100 = {} (was {})",
    //               WEIGHT_THRESHOLD_VERY_LIGHT, old_normality, normality,
    //               old_normality);
  }

  int final = std::clamp(normality, 0, 100);
  // Logger::debug("  Final normality: {}", final);
  return final;
}

int Pizza::calculate_herb_score() const {
  // Original: ingredients at indices >= 0x50 (80) are herbs
  // Herbs in our enum: PARSLEY(80), LEMON_BALM(81), SAGE(82), GINGER(83),
  // OREGANO(84)
  // herb_score = clamp(total_herb_weight / 3, 0, 10)

  int herb_weight = 0;
  for (const auto& topping : toppings_) {
    if (!topping) {
      continue;
    }
    const auto* ingredient = topping->ingredient();
    // Check if category is CHEESE and the ingredient is one of the known herbs;
    // parsley is the first of the herbs and there are no ingredients after it:
    if (ingredient->category() == IngredientCategory::CHEESE) {
      auto type = ingredient->type();
      if (static_cast<int>(type) >= static_cast<int>(IngredientType::PARSLEY)) {
        herb_weight += topping->weight();
      }
    }
  }

  return std::clamp(herb_weight / 3, 0, 10);
}

void Pizza::calculate_demographic_scores(
    const CustomerTasteProfiles& taste_profiles) {
  const int normality = normality_;
  const int imitation = imitation_;
  const int appearance = appearance_;
  const bool is_user_recipe = (origin_ == Origin::User);
  const auto pizza_flavors = flavors_;

  for (auto demo : ALL_CONSUMER_TYPES) {
    const auto& tp = taste_profiles[demo];

    int fat_diff =
        std::abs(tp.fat - pizza_flavors.fat / DEMOGRAPHIC_FLAVOR_DIVISOR);
    int sweet_diff =
        std::abs(tp.sweet - pizza_flavors.sweet / DEMOGRAPHIC_FLAVOR_DIVISOR);
    int salty_diff =
        std::abs(tp.salty - pizza_flavors.salty / DEMOGRAPHIC_FLAVOR_DIVISOR);
    int hot_diff =
        std::abs(tp.hot - pizza_flavors.hot / DEMOGRAPHIC_FLAVOR_DIVISOR);

    int sum = fat_diff + sweet_diff + salty_diff + hot_diff;
    int si = 100 - (4 * sum);

    const auto floor_div2 = [](int x) -> int {
      return (x >= 0) ? (x / 2) : ((x - 1) / 2);
    };
    if (is_user_recipe) {
      const int adj = floor_div2(DEMOGRAPHIC_APPEARANCE_THRESHOLD - appearance);
      si -= adj;
    }

    si = std::max(0, si);

    int scaling_factor = is_user_recipe ? normality : imitation;
    // Scores above 100 are meaningful and feed the demand.
    popularity_[demo] = (si * scaling_factor) / 100;
  }
}

void Pizza::calculate_judge_scores() {
  // Logger::debug("=== JUDGE SCORING ===");

  const int normality = normality_; // Use cached value
  const int imitation = imitation_; // Use cached value

  // Get flavor values (these are 0-100 percentages)
  const auto flavors = flavors_;

  // Count ingredients and calculate mass_byte approximation
  int ingredient_count = static_cast<int>(toppings_.size());

  // Calculate total weight for mass_byte approximation
  int total_weight = 0;
  for (const auto& topping : toppings_) {
    if (!topping) {
      continue;
    }
    total_weight += topping->weight();
  }

  // mass_byte approximation: scale weight down to byte range
  int mass_byte = (total_weight >> 7) & 0xFF;

  // count2 seems to be topping count (used in Judge 1)
  int count2 = ingredient_count;

  // Logger::debug("  Normality: {}", normality);
  // Logger::debug("  Ingredient count: {}", ingredient_count);
  // Logger::debug("  Total weight: {}g", total_weight);
  // Logger::debug("  Mass byte: {}", mass_byte);
  // Logger::debug("  Appearance: {}, Refinement: {}", appearance_,
  // refinement_);

  // Judge 0: Emphasizes salty, penalizes fat and few ingredients
  {
    int salty_scaled = flavors.salty / JUDGE_FLAVOR_DIVISOR;
    int fat_scaled = flavors.fat / JUDGE_FLAVOR_DIVISOR;
    int raw = salty_scaled + mass_byte - (fat_scaled / 2);
    if (ingredient_count < JUDGE_INGREDIENT_THRESHOLD) {
      raw -= (JUDGE_INGREDIENT_THRESHOLD - ingredient_count) / 2;
    }
    int score = std::clamp(raw, 0, JUDGE_MAX_SCORE);
    score = (score * normality) / 100;
    score = (score * imitation) / 100;
    scores_[0] = std::clamp(score, 0, JUDGE_MAX_SCORE);
    // Logger::debug(
    //     "  Judge 0 (salty lover): raw={}, after_normality={}, final={}", raw,
    //     score, scores_[0]);
  }

  // Judge 1: Emphasizes appearance and refinement
  {
    int raw = (appearance_ + refinement_) / 10;
    if (count2 < JUDGE_TOPPING_THRESHOLD) {
      raw -= (JUDGE_TOPPING_THRESHOLD - count2) / 2;
    }
    int score = std::clamp(raw, 0, JUDGE_MAX_SCORE);
    score = (score * normality) / 100;
    score = (score * imitation) / 100;
    scores_[1] = std::clamp(score, 0, JUDGE_MAX_SCORE);
    // Logger::debug("  Judge 1 (appearance/refinement): raw={}, "
    //               "after_normality={}, final={}",
    //               raw, score, scores_[1]);
  }

  // Judge 2: Emphasizes hot and herbs
  // Original: score = hot + (herb_score / 10)
  {
    int hot_scaled = flavors.hot / JUDGE_FLAVOR_DIVISOR;
    int raw = hot_scaled + (herb_score_ / 10);
    int score = std::clamp(raw, 0, JUDGE_MAX_SCORE);
    score = (score * normality) / 100;
    score = (score * imitation) / 100;
    scores_[2] = std::clamp(score, 0, JUDGE_MAX_SCORE);
    // Logger::debug("  Judge 2 (hot/herbs): raw={}, after_normality={},
    // final={}",
    //               raw, score, scores_[2]);
  }

  // Judge 3: Emphasizes sweet (with random noise, but we'll skip that)
  {
    int sweet_scaled = flavors.sweet / JUDGE_FLAVOR_DIVISOR;
    int raw = 2 * sweet_scaled;
    // In original: subtract random 0 or 1, we'll skip that for consistency
    int score = std::clamp(raw, 0, JUDGE_MAX_SCORE);
    score = (score * normality) / 100;
    score = (score * imitation) / 100;
    scores_[3] = std::clamp(score, 0, JUDGE_MAX_SCORE);
    // Logger::debug(
    //     "  Judge 3 (sweet tooth): raw={}, after_normality={}, final={}", raw,
    //     score, scores_[3]);
  }

  // Judge 4: Emphasizes fat
  {
    int fat_scaled = flavors.fat / JUDGE_FLAVOR_DIVISOR;
    int raw = fat_scaled + mass_byte;
    int score = std::clamp(raw, 0, JUDGE_MAX_SCORE);
    score = (score * normality) / 100;
    score = (score * imitation) / 100;
    scores_[4] = std::clamp(score, 0, JUDGE_MAX_SCORE);
    // Logger::debug("  Judge 4 (fat lover): raw={}, after_normality={},
    // final={}",
    //               raw, score, scores_[4]);
  }

  // Logger::debug("  Final scores: [{}, {}, {}, {}, {}]", scores_[0],
  // scores_[1],
  //               scores_[2], scores_[3], scores_[4]);
  // Logger::debug("=== END JUDGE SCORING ===");
}

void Pizza::set_disk_stats(const Flavors& flavors, int appearance,
                           int refinement, int normality, int imitation,
                           const CustomerTasteProfiles& taste_profiles) {
  // Logger::debug("=== SETTING DISK STATS ===");
  // Logger::debug("  Flavors: fat={}, sweet={}, salty={}, hot={}", flavors.fat,
  //               flavors.sweet, flavors.salty, flavors.hot);
  // Logger::debug("  Appearance: {}, Refinement: {}, Normality: {}",
  // appearance,
  //               refinement, normality);

  flavors_ = flavors;
  appearance_ = appearance;
  refinement_ = refinement;
  normality_ = normality;
  imitation_ = imitation;

  // Still need to calculate calories from toppings
  float calories_sum = 0;
  for (const auto& topping : toppings()) {
    calories_sum += topping->calories();
  }
  calories_ = static_cast<int>(calories_sum);

  // Recalculate judge and demographic scores with the new stats
  calculate_judge_scores();
  calculate_demographic_scores(taste_profiles);

  // Logger::debug("=== END SETTING DISK STATS ===");
}

void Pizza::set_saved_stats(int appearance, int refinement, int imitation,
                            const CustomerTasteProfiles& taste_profiles) {
  appearance_ = appearance;
  refinement_ = refinement;
  imitation_ = imitation;

  calculate_judge_scores();
  calculate_demographic_scores(taste_profiles);
}

} // namespace core::food
