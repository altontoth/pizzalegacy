/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/common/Ids.h"
#include "views/base/MainPanelView.h"
#include "views/ui/CardSlider.h"

class Player;

class PlayerProfile final : public MainPanelView {
public:
  PlayerProfile(GameContext& context, GameState& state, core::PlayerId id);
  void tick() override;
  void render(const RenderContext& ctx) override;

private:
  enum class PlayerProfileMode {
    Overview,
    CharacterValues,
    Company,
    CurriculumVitae,
    Position,
    Awards,
    TargetCV,
  };
  enum class PageAction { Previous, Next };
  void set_up_buttons();
  void change_mode(PlayerProfileMode);
  void handle_arrow(PageAction);
  bool switch_to_player(core::PlayerId new_id);
  Surface create_character_values(const Player*);
  Surface create_company(const Player*);
  Surface create_curriculum_vitae(const Player*, int page);
  Surface create_position(const Player*);
  Surface create_awards(const Player*);
  Surface create_target_cv(const Player*, int page);
  Surface create_overview(const Player* player);

  core::PlayerId id_;
  PlayerProfileMode mode_{PlayerProfileMode::Overview};
  int cv_page_{0};
  static constexpr int max_cv_pages = 6;
  static constexpr int space_on_top = 2;
  static constexpr int slider_height = 352;
  views::ui::CardSlider card_slider_{slider_height, space_on_top};
  const Surface blue_paper_;
  const Surface position_paper_;
  const Surface awards_board_;
  const Surface orden_sheet_;
  // The overview screen, drawn as the static backdrop behind any sliding card.
  std::optional<Surface> background_;
  int start_of_day_{9};
  bool player_switched_{false};
};
