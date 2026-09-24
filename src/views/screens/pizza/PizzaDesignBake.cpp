/*
 * SPDX-FileCopyrightText: 2013 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// The bake workflow: starting a bake session, finishing a design, and the
// post-judging bookkeeping.

#include "core/events/MilestoneId.h"
#include "core/factories/PizzaFactory.h"
#include "core/food/PizzaJudging.h"
#include "core/food/StandardPizza.h"
#include "core/state/GameState.h"
#include "engine/FormatCompat.h"
#include "engine/Logger.h"
#include "engine/ui/ICursorManager.h"
#include "resources/BobSizeFn.h"
#include "views/screens/pizza/PizzaDesign.h"
#include "views/screens/pizza/PizzaDesignHelpers.h"
#include "views/screens/restaurant/RecipeListRow.h"
#include "views/ui/PopupManager.h"
#include "views/ui/popups/ConfirmPopup.h"
#include "views/ui/popups/TextInputPopup.h"

#include <algorithm>
#include <memory>
#include <string>

using namespace core::food;
using pizza_design::find_row_by_key;

void PizzaDesign::on_bake_pressed() {
  auto* player = competing_player();
  auto rows = build_recipe_list_rows(
      player->recipes(),
      state_.restaurants_in_city(player->id(), state_.active_city_id()));
  rows.push_back(RecipeListRow{.name = "<NEW>"}); // last is <NEW>

  const auto* sel = find_row_by_key(rows, *active_key_);
  if (!sel) {
    Logger::warn("Something is wrong, I don't have a row?!");
    return;
  }
  const auto& row = *sel;

  BakeSession s{};
  if (!row.recipe_id && !row.reference) {
    s.kind = BakeKind::NewUser;
    s.suggested_name = "";
  }
  else if (!row.recipe_id && row.reference) {
    s.kind = BakeKind::InstantiateStandard;
    s.reference_id = row.reference;
    s.suggested_name = row.name;
  }
  else {
    s.kind = BakeKind::RecreateExisting;
    s.existing_id = row.recipe_id;
    s.reference_id = row.reference;
    s.suggested_name = row.name;
    s.working = Pizza(s.suggested_name);
    auto sp = std::make_shared<BakeSession>(std::move(s));

    context_.popup->origin(ViewPoint{6, 14});
    // Ask first, continue in callback
    context_.popup->create<ConfirmPopup>("Recreate recipe?",
                                         [this, sp](bool ok) {
                                           if (!ok) {
                                             return;
                                           }
                                           this->begin_bake(std::move(*sp));
                                         });
    return;
  }

  begin_bake(std::move(s));
}

void PizzaDesign::begin_bake(core::food::BakeSession s) {
  bake_ = std::move(s);
  design_mode_ = true;
  bake_->working.update_stats(state_.customer_taste_profiles());
  status_.show_pizza(PizzaCtx{
      .state = &state_,
      .wip_pizza = &bake_->working,
      .recipe = nullptr,
  });
  change_mode(PizzaDesignMode::Design);
}

void PizzaDesign::on_design_finished() {
  state_.advance_time_blocks(5);
  design_mode_ = false;
  if (!bake_) {
    Logger::warn("I don't have a bake?!");
    change_mode(PizzaDesignMode::View);
    return;
  }

  if (bake_->reference_id) {
    auto get_bob_size =
        make_ingredient_bob_size_fn<Bob, core::food::Topping>(zutaten_bob_);
    auto reference_pizza = PizzaFactory::create_pizza(
        *bake_->reference_id, get_bob_size, state_.customer_taste_profiles());

    if (reference_pizza) {
      bake_->working.finalize_against(*reference_pizza,
                                      state_.rules().pizza_judging,
                                      state_.customer_taste_profiles());
    }
    else {
      Logger::warn("Failed to load reference pizza {}",
                   core::food::to_string(*bake_->reference_id));
    }
  }
  else {
    // Finalize the pizza but don't set name yet for NewUser
    std::string tmp = (bake_->kind == BakeKind::NewUser) ? "recipe" : "";
    bake_->working.finalize(state_.customer_taste_profiles(), tmp);
  }

  post_bake();
}

void PizzaDesign::finish_new_user(std::string name) {
  if (name.empty()) {
    name = "<NEW>";
  }
  bake_->working.name(name);
  auto* cp = competing_player();
  auto id =
      cp->recipes().create_user(std::move(name), std::move(bake_->working));
  change_active_key(id);

  state_.record_milestone(core::events::MilestoneId::CreatedFirstPizza,
                          cp->id());

  if (competition_) {
    // Annual competition: hand results to GameState and exit.
    state_.ui().pop();
    state_.competitions().finalize_annual_pizza_competition(
        std::move(*competition_));
    return;
  }

  bake_.reset();
  design_mode_ = false;
  // Replace the OwnPizzaJudging state on the stack with a clean Recipes state.
  // Any subsequent view rebuild (e.g. when the CreatedFirstPizza fullscreen is
  // pushed and then dismissed) will construct PizzaDesign in Recipes mode
  // instead of re-entering handle_post_judging with the already-consumed bake.
  state_.ui().replace_top<PizzaDesignState>(PizzaDesignState{
      .initial_mode = PizzaDesignMode::Recipes,
      .active_key = active_key_,
  });
}

void PizzaDesign::finish_recreate_existing() {
  auto& recipes = competing_player()->recipes();
  auto rid = *bake_->existing_id;

  if (bake_->reference_id) {
    recipes.create_standard(*bake_->reference_id, std::move(bake_->working));
    change_active_key(*bake_->reference_id);
  }
  else {
    // Recreating a user recipe
    const Recipe* existing = recipes.find(rid);
    if (!existing) {
      Logger::warn("Could not find recipe {} to recreate", rid.value);
      cancel_bake();
      return;
    }
    recipes.recreate_user(rid, std::string(existing->display_name()),
                          std::move(bake_->working));
    change_active_key(rid);
  }
}

void PizzaDesign::post_bake() {
  auto bake_ptr = std::make_shared<core::food::BakeSession>(std::move(*bake_));
  auto active_key = active_key_;
  JudgingMode jmode = bake_ptr->reference_id.has_value() ? JudgingMode::Standard
                                                         : JudgingMode::Own;

  // Carry the annual competition session through the judge screen so that the
  // PizzaDesign afterwards has access
  auto annual_session =
      competition_
          ? std::make_shared<CompetitionSession>(std::move(*competition_))
          : nullptr;
  competition_.reset();

  state_.ui().push<PizzaJudgesState>(
      bake_ptr->working.scores(),
      [&state = state_, bake_ptr, active_key, annual_session] {
        state.ui().pop();
        state.ui().replace_top<PizzaDesignState>(
            PizzaDesignMode::OwnPizzaJudging, std::nullopt, bake_ptr,
            active_key, annual_session);
      },
      jmode);
}

void PizzaDesign::cancel_bake() {
  active_topping_.reset();
  context_.cursor->clear_custom_cursor();
  change_mode(PizzaDesignMode::Recipes);
  bake_.reset();
}

void PizzaDesign::handle_post_judging() {
  state_.advance_time_blocks(3);

  if (mode_ == PizzaDesignMode::CompetitionJudging) {
    if (!competition_) {
      Logger::warn("CompetitionJudging but no competition session!");
      state_.ui().pop();
      return;
    }
    if (bake_) {
      int match_score = bake_->working.imitation();
      auto& c = *competition_;
      if (c.human_slot >= 0 &&
          c.human_slot < static_cast<int>(c.participants.size())) {
        c.participants[c.human_slot].match_score = match_score;
        c.participants[c.human_slot].time_ticks =
            static_cast<int>(c.human_design_ticks) / 30;
        int speed_bonus = match_score * (c.speed_level * 20) / 100;
        int final_score =
            std::clamp(match_score + speed_bonus -
                           (c.participants[c.human_slot].time_ticks / 3),
                       0, 200);
        c.participants[c.human_slot].final_score = final_score;
      }
      bake_.reset();
    }

    // If more human players remain in the session, chain to the next one
    // instead of finalizing. AI scores are simulated only after the last human.
    auto& c = *competition_;
    int next_slot = c.human_slot + 1;
    if (next_slot < static_cast<int>(c.participants.size()) &&
        c.participants[next_slot].is_human) {
      c.human_slot = next_slot;
      core::PlayerId next_pid = c.participants[next_slot].player_id;
      auto session = std::make_shared<CompetitionSession>(std::move(c));
      competition_.reset();
      state_.ui().replace_top<PizzaDesignState>(PizzaDesignState{
          .initial_mode = PizzaDesignMode::MonthlyChallenge,
          .competition_session = std::move(session),
          .competing_player_id = next_pid,
      });
      return;
    }

    finalize_competition_results();
    return;
  }

  if (!bake_) {
    Logger::warn("No bake session after judging!");
    change_mode(PizzaDesignMode::Recipes);
    return;
  }

  // Handle post-judging flow based on bake kind
  if (bake_->kind == BakeKind::NewUser) {
    // For annual competition: record score before naming
    if (competition_) {
      const auto& scores = bake_->working.scores();
      int total = 0;
      for (int s : scores) {
        total += s;
      }
      auto& c = *competition_;
      if (c.human_slot >= 0 &&
          c.human_slot < static_cast<int>(c.participants.size())) {
        c.participants[c.human_slot].match_score = total;
        c.participants[c.human_slot].final_score = total;
      }
    }

    // After a competition you get the list of recipes in the background when
    // asking for the recipe name, otherwise you get the info screen.
    change_mode(competition_ ? PizzaDesignMode::Recipes
                             : PizzaDesignMode::Info);

    context_.popup->create<TextInputPopup>(
        "name of recipe:", max_recipe_name_length,
        [this](std::string input) { this->finish_new_user(std::move(input)); });
  }
  else {
    const bool poor_imitation =
        bake_->reference_id && bake_->working.imitation() < 10;

    // Commit recipe now that judging is done
    if (bake_->kind == BakeKind::InstantiateStandard) {
      competing_player()->recipes().create_standard(*bake_->reference_id,
                                                    std::move(bake_->working));
      change_active_key(*bake_->reference_id);
    }
    else if (bake_->kind == BakeKind::RecreateExisting) {
      finish_recreate_existing();
    }

    if (poor_imitation) {
      int reference_id = static_cast<int>(*bake_->reference_id);
      constexpr int cook_book_page_offset = 9;
      int page = (reference_id + cook_book_page_offset) / 2;
      bake_.reset();
      // Replace OwnPizzaJudging so dismissing the reminder returns to Recipes.
      state_.ui().replace_top<PizzaDesignState>(PizzaDesignState{
          .initial_mode = PizzaDesignMode::Recipes,
          .active_key = active_key_,
      });
      state_.ui().push<MaestroMessageState>(MaestroMessageState{
          .text = compat::format(
              "You need a bit more practice baking the standard recipes. This "
              "recipe can be found on page {} of the great Pizza Tycoon cook "
              "book.",
              page),
          .on_dismiss = {},
      });
      return;
    }

    change_mode(PizzaDesignMode::Recipes);
    bake_.reset();
  }
}
