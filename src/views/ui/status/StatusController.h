/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>

#include "core/city/City.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/GameState.h"
#include "core/status/StatusHub.h"

// Contexts the controller can render for
struct CityCtx {
  GameState* state{nullptr};
  City* city{nullptr};
};

struct RestaurantCtx {
  GameState* state{nullptr};
  Restaurant* rest{nullptr};
  std::string_view label;
};

struct PizzaCtx {
  GameState* state{nullptr};
  core::food::Pizza* wip_pizza{nullptr};
  const core::food::Recipe* recipe{nullptr};
};

struct ToppingCtx {
  GameState* state{nullptr};
  core::food::Topping* topping;
};

struct CompetitionCtx {
  std::string pizza_name;
  int speed_level{1};
  std::string current_ingredient;
};

enum class PropertyDisplayStage {
  DEFAULT,
  PROPERTY_INFO,
  CUSTOMER_INFO,
  NAMED_BUILDING,
  TYPE_NAME,
};

class StatusController {
public:
  explicit StatusController(StatusHub& hub) : hub_(hub) {}

  // Public entrypoints for the views:
  void show_city_default(const CityCtx& c);
  void show_city_property_info(const CityCtx& c, int index);
  void show_city_competitor_restaurant(const CityCtx& c, int index);
  void show_city_customer_info(const CityCtx& c, int index);
  void show_city_named_building(const CityCtx& c, int index);
  void show_city_type_name(const CityCtx& c, int index);

  void show_restaurant(const RestaurantCtx& r);
  void show_competitor_restaurant(const CityCtx& c, const Restaurant& rest);
  void show_pizza(const PizzaCtx& p);
  void show_topping(const ToppingCtx& t);
  void show_competition(const CompetitionCtx& c);

private:
  // Builders
  static StatusDisplayRecipe build_competition(const CompetitionCtx&);
  static StatusDisplayRecipe build_city_default(const CityCtx&);
  static StatusDisplayRecipe build_city_property_info(const CityCtx&,
                                                      int index);
  static StatusDisplayRecipe build_city_competitor_restaurant(const CityCtx&,
                                                              int index);
  static StatusDisplayRecipe
  build_competitor_restaurant(const CityCtx&, const Restaurant& rest);
  static StatusDisplayRecipe build_city_customer_info(const CityCtx&,
                                                      int index);
  static StatusDisplayRecipe build_city_named_building(const CityCtx&,
                                                       int index);
  static StatusDisplayRecipe build_city_type_name(const CityCtx&, int index);
  static StatusDisplayRecipe build_topping(const ToppingCtx& tctx);

  static void add_property_header(StatusDisplayRecipe& r, const CityCtx&,
                                  int index);

  static StatusDisplayRecipe build_restaurant(const RestaurantCtx&);
  static StatusDisplayRecipe build_pizza(const PizzaCtx&);
  void emit_baseline(StatusDisplayRecipe r);
  void emit_status_msg(StatusMsg msg);

  StatusHub& hub_;
};
