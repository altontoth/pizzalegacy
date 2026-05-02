/*
 * SPDX-FileCopyrightText: 2013 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/GameTypes.h"
#include "core/events/LeventId.h"
#include "core/food/BakeSession.h"
#include "core/food/Pizza.h"
#include "core/food/Recipe.h"
#include "core/food/Topping.h"
#include "core/state/CompetitionSession.h"
#include "core/state/UIState.h"
#include "engine/geom/Camera.h"
#include "engine/geom/Spaces.h"
#include "engine/input/InputListener.h"
#include "engine/input/ListenerToken.h"
#include "views/base/BobRenderer.h"
#include "views/base/MainPanelView.h"
#include "views/ui/DropdownManager.h"
#include "views/ui/status/StatusController.h"

#define NUMBER_OF_DROPDOWNS 4

struct DesignResult {
  enum class Outcome { Saved, Cancelled };
  Outcome outcome;
  // On Saved:
  std::optional<core::RecipeId> id; // the (new or existing) recipe id
  core::food::Recipe recipe;        // finalized recipe data
};

class PizzaDesign : public MainPanelView, public input::InputListener {
public:
  PizzaDesign(GameContext& context, GameState& state, PizzaDesignState pms);
  ~PizzaDesign();
  void tick() override;
  void render(const RenderContext& ctx) override;
  bool on_action(const input::ActionEvent& ev) override;
  enum class IngredientDropdown { VEGETABLES, MEAT, FRUIT, CHEESE };

private:
  bool handle_left_mouse_click(ViewPoint p);
  bool handle_right_mouse_click(ViewPoint p);
  bool handle_mouse_hover(ViewPoint p);
  static bool is_inside_pizza(ViewPoint p);
  void draw_pizza(const RenderContext& ctx) const;
  void draw_ingredients(const RenderContext& ctx) const;
  void draw_pizza_info(const RenderContext& ctx) const;
  void draw_recipes(const RenderContext& ctx);
  void create_dropdowns();
  void start_chop();
  void create_input_panel();
  void change_mode(PizzaDesignMode mode);
  bool is_new_selected() const;
  void on_bake_pressed();
  void on_design_finished();
  void begin_bake(core::food::BakeSession s);
  void finish_new_user(std::string name);
  void finish_recreate_existing();
  void post_bake();
  void cancel_bake();
  core::food::Pizza* current_pizza();
  const core::food::Pizza* current_pizza() const;
  void load_user_recipe(int number);
  void load_canonical_recipe(int number);
  void load_recipe_for_debug(const std::string& filename);
  void load_user_recipe_for_debug(int number);
  void load_canonical_for_debug(int number);
  void change_active_key(RecipeKey change_active_key);
  void update_status_ingredient();
  void handle_post_judging();

  // Competition
  Player* competing_player() const;
  void begin_competition(std::shared_ptr<CompetitionSession> session);
  void begin_annual_competition(std::shared_ptr<CompetitionSession> session);
  void on_reveal_complete();
  void on_competition_design_finished();
  void finalize_competition_results();
  void draw_competition_reveal(const RenderContext& ctx) const;
  void draw_competition_results(const RenderContext& ctx);
  void update_competition_status();
  void setup_pizza_design_interactions(const RenderContext& ctx);

  PizzaDesignMode mode_;
  Camera camera_;
  // This one is so we know what mode to use for the input menu:
  bool design_mode_{false};
  std::unique_ptr<core::food::Topping> active_topping_;
  Bob& zutaten_bob_;
  Surface blade_horizontal_;
  Surface blade_vertical_;
  bool chopping_{false};
  bool chop_horizontal_{true};
  std::unique_ptr<core::food::Topping> previous_topping_;
  BobRenderer renderer_{"bob/zutaten.bob", context_.bob};
  DropdownManager<IngredientDropdown> dropdowns_;
  uint64_t chop_start_ms_{0};
  uint64_t chop_last_phase_{0};
  std::optional<RecipeKey> active_key_{new_row_tag};
  int page_{0};
  static constexpr int recipes_per_page_ = 14;
  size_t recipe_count_{0};
  std::optional<core::food::BakeSession> bake_;
  std::optional<CompetitionSession> competition_;
  std::optional<core::PlayerId> competing_player_id_;
  std::function<void()> on_done_;
  std::optional<core::events::LeventId> pending_competition_message_;
  uint64_t competition_design_ticks_{0};
  int reveal_delay_remaining_ticks_{0};
  bool reveal_speed_boost_{false};
  input::ListenerToken listener_;
  StatusController status_;
};
