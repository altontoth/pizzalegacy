/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/character/Staff.h"
#include "core/restaurant/Restaurant.h"
#include "core/state/InterruptionRate.h"
#include "engine/geom/Spaces.h"
#include "resources/Blitmap.h"
#include "views/base/RestaurantSubView.h"
#include "views/base/ViewDefs.h"
#include "views/ui/CardSlider.h"

#include <functional>

enum class StaffScreen {
  OVERVIEW,
  APPLICANT_CARD,
  APPLICATIONS,
  TRAINING,
};

class StaffView : public RestaurantSubView {
public:
  static constexpr int PANEL_SPACE_ON_TOP = 16;
  StaffView(GameContext& context, GameState& state);
  void tick() override;
  void render(const RenderContext& ctx) override;
  InterruptionRate interruption_rate() const override {
    return InterruptionRate::OnAction;
  }

private:
  void navigate_to_restaurant(core::RestaurantId new_id) override;
  void handle_left_mouse_click(ViewPoint p);
  void create_input_panel();
  void change_screen(StaffScreen);
  void change_job(core::character::JobRole);
  void on_staff_slot_clicked(core::character::JobRole job, int slot_index);

  // Button configuration methods
  void add_overview_buttons();
  void add_applicant_buttons();
  void add_common_buttons();
  void handle_next_applicant();
  void handle_previous_applicant();

  // Rendering methods
  void draw_gray_header(Surface& dst, int y, int height = 16);
  void draw_green_area(Surface& dst, int y, int height);
  void draw_green_rows(Surface& dst, int y, int count);
  void draw_card_name(Surface& card, std::string_view first,
                      std::string_view last);
  void apply_wage_change(Staff& staff, int new_wage_dollars);
  Surface create_top_bar(const std::string& title = "");
  Surface create_manager_settings_panel(Restaurant* r);
  void draw_manager_settings_interactions();
  void draw_training_interactions();

  // Builds the interaction handler shared by every manager-settings button:
  // highlight settings_row while held, run on_click on release, clear the
  // highlight on release or cancel.
  views::ui::InteractionLayers::Callback
  settings_button_handler(int settings_row, std::function<void()> on_click);
  Surface create_training_card(const Staff& staff);
  void show_training_card();
  void on_training_clicked(const Training& course);
  Staff* selected_staff();
  Surface create_data_card(core::character::CharacterData* c,
                           core::character::JobRole role);
  Surface create_data_card(const Staff& staff);
  Surface create_data_card_impl(const RawRenderable& portrait,
                                const core::character::CharacterData& data,
                                int payment_dollars,
                                std::string_view tab_label = "");
  Surface create_application_card(uint8_t character_id);
  std::pair<std::string, std::string>
  load_application_text(uint8_t character_id);
  Surface create_empty_card(std::string_view label = "");
  Surface create_staff_overview(Restaurant* r);
  Surface create_staff_slots(Restaurant* r);
  void load_applicant_pool();
  void show_applicant_card(core::character::JobRole role);
  Surface make_card(core::character::JobRole role, std::size_t idx);

  Blitmap<Blitmap3Icon>& blitmap3_;
  StaffScreen screen_{StaffScreen::OVERVIEW};
  core::character::JobRole job_{core::character::JobRole::CHEF};
  std::optional<Surface> top_bar_;
  std::optional<Surface> empty_card_surface_;
  std::optional<Surface> empty_staff_slots_surface_;
  std::optional<Surface> empty_staff_overview_surface_;
  views::ui::CardSlider card_slider_{
      views::base::MAIN_PANEL_HEIGHT - PANEL_SPACE_ON_TOP, PANEL_SPACE_ON_TOP};
  bool input_panel_needs_update_{false};
  std::optional<int> selected_slot_index_;
  std::unordered_map<core::character::JobRole,
                     std::vector<core::character::CharacterData*>>
      applicant_pool_;
  // which candidate index is currently on screen for each role
  std::unordered_map<core::character::JobRole, std::size_t> applicant_cursor_{
      {{core::character::JobRole::CHEF, 0},
       {core::character::JobRole::WAITER, 0},
       {core::character::JobRole::MANAGER, 0}}};
  std::optional<int> pressed_settings_row_;
  std::optional<int> pressed_training_row_;
};
