/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/status/StatusController.h"

#include <array>
#include <string>
#include <string_view>
#include <utility>
#include "core/character/Player.h"
#include "core/city/City.h"
#include "core/city/District.h"
#include "core/city/IngredientTrendSystem.h"
#include "core/city/Property.h"
#include "core/restaurant/PizzaDemand.h"
#include "core/restaurant/Restaurant.h"
#include "core/restaurant/RestaurantEconomy.h"
#include "core/status/StatusHub.h"

template <class... Ts> struct Overload : Ts... {
  using Ts::operator()...;
};
template <class... Ts> Overload(Ts...) -> Overload<Ts...>;

namespace {
// Label bands for an ingredient's popularity trend.
std::string_view trend_label(int trend) {
  if (trend <= -30) {
    return "MEGA-OUT";
  }
  if (trend <= -9) {
    return "OUT";
  }
  if (trend <= 9) {
    return "NEUTRAL";
  }
  if (trend <= 30) {
    return "IN";
  }
  return "MEGA-IN";
}
} // namespace

void StatusController::show_city_default(const CityCtx& c) {
  emit_baseline(build_city_default(c));
}
void StatusController::show_city_property_info(const CityCtx& c, int index) {
  emit_baseline(build_city_property_info(c, index));
}
void StatusController::show_city_competitor_restaurant(const CityCtx& c,
                                                       int index) {
  emit_baseline(build_city_competitor_restaurant(c, index));
}
void StatusController::show_city_customer_info(const CityCtx& c, int index) {
  emit_baseline(build_city_customer_info(c, index));
}
void StatusController::show_city_named_building(const CityCtx& c, int index) {
  emit_baseline(build_city_named_building(c, index));
}
void StatusController::show_city_type_name(const CityCtx& c, int index) {
  StatusMsg msg{
      .payload = build_city_type_name(c, index),
      .scope = StatusScope::City,
  };
  emit_status_msg(msg);
}

void StatusController::show_restaurant(const RestaurantCtx& r) {
  emit_baseline(build_restaurant(r));
}
void StatusController::show_competitor_restaurant(const CityCtx& c,
                                                  const Restaurant& rest) {
  emit_baseline(build_competitor_restaurant(c, rest));
}
void StatusController::show_pizza(const PizzaCtx& p) {
  emit_baseline(build_pizza(p));
}
void StatusController::show_topping(const ToppingCtx& t) {
  emit_baseline(build_topping(t));
}
void StatusController::show_competition(const CompetitionCtx& c) {
  emit_baseline(build_competition(c));
}

void StatusController::emit_baseline(StatusDisplayRecipe r) {
  // Copy the recipe into the lambda. Each call overwrites the baseline.
  hub_.set_baseline([recipe = std::move(r)]() { return recipe; });
}

void StatusController::emit_status_msg(StatusMsg msg) {
  hub_.post_direct(std::move(msg));
}

StatusDisplayRecipe
StatusController::build_competition(const CompetitionCtx& c) {
  StatusDisplayRecipe r;
  if (c.current_ingredient.empty()) {
    r.add_header("GET READY...", StatusAlignment::Left);
  }
  else {
    r.add_header("PIZZA COMPETITION", StatusAlignment::Left);
    r.add_single("ORIGINAL RECIPE:");
    r.add_single(c.pizza_name);
    r.add_two_column("LEVEL:", std::to_string(c.speed_level));
    r.add_single("CURRENT INGREDIENT:");
    r.add_single(c.current_ingredient);
  }
  return r;
}

StatusDisplayRecipe StatusController::build_city_default(const CityCtx& c) {
  auto* state = c.state;
  auto* player = state->active_player();
  assert(player);

  StatusDisplayRecipe r;
  r.add_header(c.city->name());
  r.add_header(
      compat::format("IN {}", GameTime::to_string(state->time().season())));
  r.add_header(player->name());
  r.add_two_column("RESTAURANTS",
                   std::to_string(state->restaurant_count_for_player_in_city(
                       player->id(), c.city->id())));
  r.add_two_column("RECIPES", std::to_string(player->recipe_count()));
  r.add_two_column("CONTRACTS", std::to_string(player->contract_count_for_city(
                                    c.city->id())));
  r.add_header("DISTRICT:");
  auto district_index = state->active_district_index();
  r.add_single(district_index ? c.city->district(*district_index).name()
                              : "- NONE -");
  return r;
}

void StatusController::add_property_header(StatusDisplayRecipe& r,
                                           const CityCtx& c, int index) {
  auto* property = c.city->property_by_index(index);
  if (!property) {
    return;
  }
  bool is_depot = property->type() == PropertyType::DEPOT;
  bool has_restaurant = false;
  Restaurant* restaurant = nullptr;

  std::string type_str;
  if (is_depot) {
    type_str = "Depot";
  }
  else if (property->has_restaurant()) {
    type_str = "Restaurant";
    restaurant = c.state->restaurant(*property->restaurant_id());
    has_restaurant = true;
  }
  else {
    type_str = "Business Premises";
  }
  r.add_header(type_str);

  std::string status_str;
  switch (property->status()) {
  case PremiseStatus::FOR_SALE:
    status_str = "FOR SALE";
    break;
  case PremiseStatus::NOT_AVAILABLE:
    status_str = "NOT AVAILABLE";
    break;
  case PremiseStatus::FOR_RENT:
    status_str = "FOR RENT";
    break;
  case PremiseStatus::FOR_SALE_RENT:
    status_str = "FOR SALE/RENT";
    break;
  case PremiseStatus::OWNED:
  case PremiseStatus::RENTED:
    if (is_depot) {
      status_str = c.state->active_player()->name();
    }
    else if (has_restaurant) {
      status_str = restaurant->name();
    }
    else {
      status_str = "SOMETHINGWRONG";
    }
    break;
  }
  if (!status_str.empty()) {
    r.add_header(status_str);
  }

  if (property->type() != PropertyType::DEPOT) {
    r.add_header("-------------");
  }
}

StatusDisplayRecipe StatusController::build_city_property_info(const CityCtx& c,
                                                               int index) {
  StatusDisplayRecipe r;
  add_property_header(r, c, index);

  auto* property = c.city->property_by_index(index);
  if (!property) {
    return r;
  }

  if (property->type() == PropertyType::DEPOT) {
    r.add_two_column({.text = "CAPACITY:"},
                     {
                         .text = std::to_string(property->size().w *
                                                property->size().h * 100),
                         .x = 90,
                         .align = StatusAlignment::Left,
                     });
  }
  else {
    r.add_two_column({.text = "SIZE:"},
                     {
                         .text = std::to_string(property->size().w) + "x" +
                                 std::to_string(property->size().h),
                         .x = 90,
                         .align = StatusAlignment::Left,
                     });
  }
  r.add_two_column(
      {.text = "PRICE:"},
      {
          .text = compat::format("{:i}", property->purchase_price()),
          .x = 100,
          .align = StatusAlignment::Left,
      });

  r.add_two_column(
      {.text = "RENT:"},
      {
          .text = compat::format("{:i}", property->get_monthly_rent()),
          .x = 114,
          .align = StatusAlignment::Left,
      });

  const Actor* realtor = c.city->actor_by_id(property->realtor_id());
  r.add_header("Realtor:");
  r.add_single(realtor->name());
  return r;
}

static StatusDisplayRecipe
build_competitor_recipe(const CityCtx& c, Player* owner,
                        const std::string& district_name) {
  auto* state = c.state;
  StatusDisplayRecipe r;

  r.add_header(c.city->name());
  r.add_header(
      compat::format("IN {}", GameTime::to_string(state->time().season())));

  if (owner) {
    r.add_header(owner->name());
    r.add_two_column("RESTAURANTS",
                     std::to_string(state->restaurant_count_for_player_in_city(
                         owner->id(), c.city->id())));
    r.add_two_column("RECIPES", std::to_string(owner->recipe_count()));
    r.add_two_column("CONTRACTS", std::to_string(owner->contract_count_for_city(
                                      c.city->id())));
  }

  r.add_header("DISTRICT:");
  r.add_single(district_name);
  return r;
}

StatusDisplayRecipe
StatusController::build_city_competitor_restaurant(const CityCtx& c,
                                                   int index) {
  auto* property = c.city->property_by_index(index);
  Player* owner = nullptr;
  if (property && property->has_restaurant() && property->owner_or_tenant()) {
    owner = c.state->player(*property->owner_or_tenant());
  }
  auto district_index = c.city->district_by_index(index);
  return build_competitor_recipe(c, owner,
                                 c.city->district(district_index).name());
}

StatusDisplayRecipe
StatusController::build_competitor_restaurant(const CityCtx& c,
                                              const Restaurant& rest) {
  Player* owner = nullptr;
  if (rest.property().owner_or_tenant()) {
    owner = c.state->player(*rest.property().owner_or_tenant());
  }
  std::string district_name = "unknown";
  district_name = rest.property().district().name();
  return build_competitor_recipe(c, owner, district_name);
}

StatusDisplayRecipe StatusController::build_city_customer_info(const CityCtx& c,
                                                               int index) {
  StatusDisplayRecipe r;
  add_property_header(r, c, index);

  r.add_header("POTENTIAL CUSTOMERS:");
  auto* property = c.city->property_by_index(index);
  if (!property) {
    return r;
  }

  // Get restaurant pointer if this property has one
  const Restaurant* restaurant = nullptr;
  if (property->has_restaurant()) {
    auto rest_id = property->restaurant_id();
    if (rest_id) {
      restaurant = c.state->restaurant(rest_id.value());
    }
  }

  property->ensure_demographics(restaurant);
  const auto& counts = property->potential_customers();
  static constexpr std::array<const char*,
                              static_cast<int>(PopulationType::count)>
      labels{"Children",     "Young people",    "Blue collar",
             "White collar", "Business people", "Senior Citizens"};
  for (auto demo : ALL_CONSUMER_TYPES) {
    int i = static_cast<int>(demo);
    r.add_two_column({labels[i]}, {
                                      .text = std::to_string(counts[demo]),
                                      .x = -16,
                                      .align = StatusAlignment::Right,
                                  });
  }
  return r;
}

StatusDisplayRecipe
StatusController::build_city_named_building(const CityCtx& c, int index) {
  StatusDisplayRecipe r;
  r.add_header(c.city->building_type_name(index));
  r.add_header(c.city->building_specific_name(index));
  return r;
}

StatusDisplayRecipe StatusController::build_city_type_name(const CityCtx& c,
                                                           int index) {
  StatusDisplayRecipe r;
  r.add_header(c.city->tile_description(index));
  return r;
}

StatusDisplayRecipe
StatusController::build_restaurant(const RestaurantCtx& rctx) {
  StatusDisplayRecipe r;

  r.add_header(std::string(rctx.label));
  r.add_header(rctx.rest->name());

  // "----" + centered stars + "----", total 17 chars.
  const int stars = rctx.rest->star_rating();
  constexpr int center_width = 9;
  const int pad_left = (center_width - stars) / 2;
  const int pad_right = center_width - stars - pad_left;
  r.add_single("----" + std::string(pad_left, ' ') + std::string(stars, '*') +
               std::string(pad_right, ' ') + "----");

  auto* rest = rctx.rest;
  const int popularity = rest->economy().total_popularity();
  const int cooks = rest->count_cooks();
  const int waiters = rest->count_waiters();
  const bool has_manager = rest->manager().has_value();
  const int pizzas = static_cast<int>(rest->menu().entries().size());
  const int pizza_quality = rest->pizza_quality_rating();
  const int waiting_min = rest->calculate_service_time_minutes();
  const int stay_min =
      static_cast<int>(rest->calculate_service_time_data().avg_seating_time);
  const int local_share = rest->economy().demand_ratio();

  r.add_two_column("POPULARITY", std::to_string(popularity));
  r.add_two_column("COOKS", std::to_string(cooks));
  r.add_two_column("WAITERS", std::to_string(waiters));
  r.add_two_column("MANAGER", has_manager ? "YES" : "NO");
  r.add_two_column("PIZZAS", std::to_string(pizzas));
  r.add_two_column("PIZZA QUALITY", std::to_string(pizza_quality));
  r.add_two_column("WAITING TIME", compat::format("{} MIN", waiting_min));
  r.add_two_column("STAY TIME", compat::format("{} MIN", stay_min));
  r.add_two_column("LOCAL SHARE", compat::format("{}%", local_share));
  return r;
}

StatusDisplayRecipe StatusController::build_pizza(const PizzaCtx& pctx) {
  assert(pctx.recipe || pctx.wip_pizza);
  const auto* recipe = pctx.recipe;
  const auto& p = recipe ? &recipe->pizza() : pctx.wip_pizza;
  StatusDisplayRecipe r;

  const auto& recipes = pctx.state->active_player()->recipes();

  std::size_t total_count = recipes.recipe_count();
  size_t cur_id = recipe ? recipe->id().value + 1 : total_count + 1;
  r.add_header(pctx.cook_book
                   ? "COOK BOOK"
                   : compat::format("RECIPE {} OF {}", cur_id, total_count));
  r.add_header(p->name().empty() ? "" : p->name());

  // recompute fat/sweet/salty/hot + calories
  const auto calories = p->calories();
  const auto f = p->flavors();

  r.add_two_column("calories:", compat::format("{}cal", calories));
  r.add_two_column("fat     :", compat::format("{}%", f.fat));
  r.add_two_column("sweet   :", compat::format("{}%", f.sweet));
  r.add_two_column("salty   :", compat::format("{}%", f.salty));
  r.add_two_column("hot     :", compat::format("{}%", f.hot));

  // Display appearance and refinement scores
  r.add_two_column("appearance:", std::to_string(p->appearance()));
  r.add_two_column("refinement:", std::to_string(p->refinement()));

  const int trend =
      core::restaurant::PizzaDemand::calculate_ingredient_popularity(
          *p, *pctx.state->active_city());
  r.add_two_column("trend:", std::to_string(trend));

  // Display normality for user recipes, imitation for standard recipes
  if (p->origin() == core::food::Origin::User) {
    r.add_two_column("normality:", compat::format("{}%", p->normality()));
  }
  else {
    r.add_two_column("imitation:", compat::format("{}%", p->imitation()));
  }

  return r;
}

StatusDisplayRecipe StatusController::build_topping(const ToppingCtx& tctx) {
  const auto* topping = tctx.topping;
  const auto* ingredient = topping->ingredient();
  StatusDisplayRecipe r;

  r.add_header(ingredient->name());

  r.add_header(compat::format("{} grammes", topping->weight()));
  r.add_header(compat::format("{:.0f} calories", topping->calories()));
  r.add_header(compat::format("intensity: {}", ingredient->intensity()));
  r.add_header(compat::format("fat  : {}", ingredient->fat()));
  r.add_header(compat::format("sweet: {}", ingredient->sweet()));
  r.add_header(compat::format("salty: {}", ingredient->salty()));
  r.add_header(compat::format("hot  : {}", ingredient->hot()));
  const auto& trends = tctx.state->active_city()->ingredient_trend_system();
  const int trend = trends.get_trend(ingredient->type());
  r.add_header(compat::format("trend: {}", trend));
  r.add_header(compat::format("({})", trend_label(trend)));

  return r;
}
