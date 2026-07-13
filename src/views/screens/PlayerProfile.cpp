/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/PlayerProfile.h"
#include "core/GameTypes.h"
#include "core/character/Awards.h"
#include "core/character/Character.h"
#include "core/character/Player.h"
#include "core/character/SocialStatus.h"
#include "core/city/Property.h"
#include "core/finance/Money.h"
#include "core/mission/MissionScoring.h"
#include "core/mission/MissionTarget.h"
#include "core/state/GameState.h"
#include "core/status/StatusHub.h"
#include "core/util/Constants.h"
#include "core/util/LegacyConstants.h"
#include "engine/FormatCompat.h"
#include "engine/ui/InteractionEvent.h"
#include "gfx/Blit.h"
#include "resources/FontManager.h"
#include "resources/FontTypes.h"
#include "resources/util/FileUtils.h"
#include "views/base/ViewDefs.h"
#include "views/helpers/CardGfx.h"
#include "views/ui/PopupManager.h"
#include "views/ui/popups/ConfirmPopup.h"
#include "views/ui/popups/NumberInputPopup.h"

namespace {

constexpr std::array<const char*, NUMBER_OF_CITIES> city_names = {
    "Paris",    "Berlin", "Baltimore", "Vienna",    "Chicago",
    "New York", "Madrid", "Rome",      "Stockholm", "London",
};

constexpr int award_count = 6;

// Atlas layout (orden.vga)
constexpr int sprite_width = 41 * 2;
constexpr int sprite_height = 51 * 2;

constexpr int cv_events_per_page = 6;

int cv_total_pages(std::size_t entry_count) {
  return std::max<int>(
      1, (static_cast<int>(entry_count) + cv_events_per_page - 1) /
             cv_events_per_page);
}

// Screen layout (measured from original)
constexpr int grid_columns = 3;
constexpr int step_x = 43;
constexpr int step_y = 52;
constexpr int base_x = 60;
constexpr int base_y = 62;
} // namespace

PlayerProfile::PlayerProfile(GameContext& context, GameState& state,
                             core::PlayerId id)
    : MainPanelView(context, state), id_(id),
      blue_paper_{context_.vga->get("gfx/back32").clone()},
      position_paper_{context_.vga->get("gfx/back34").clone()},
      awards_board_{context_.vga->get("gfx/back35").clone()},
      orden_sheet_{context_.vga->get("gfx/orden").clone()} {

  set_up_buttons();

  if (const auto* player = state_.player(id_)) {
    start_of_day_ = player->start_of_day_hour();
  }
}

void PlayerProfile::set_up_buttons() {
  input_panel_->add_button(Icon::CHARACTER_QUESTION_MARK, ButtonType::Square,
                           ButtonPosition::TopLeft, "Character values", [this] {
                             change_mode(PlayerProfileMode::CharacterValues);
                           });

  input_panel_->add_button(Icon::F, ButtonType::Square,
                           ButtonPosition::TopCenter, "Company",
                           [this] { change_mode(PlayerProfileMode::Company); });

  input_panel_->add_button(
      Icon::WRITTEN_REPORT, ButtonType::Square, ButtonPosition::TopRight,
      "Curriculum Vitae",
      [this] { change_mode(PlayerProfileMode::CurriculumVitae); });

  input_panel_->add_button(
      Icon::FANCY_R, ButtonType::Square, ButtonPosition::MiddleLeft, "Position",
      [this] { change_mode(PlayerProfileMode::Position); });

  input_panel_->add_button(Icon::MEDALS, ButtonType::Square,
                           ButtonPosition::MiddleCenter, "Awards",
                           [this] { change_mode(PlayerProfileMode::Awards); });

  input_panel_->add_button(
      Icon::TARGET_CV, ButtonType::Square, ButtonPosition::MiddleRight,
      "Target CV", [this] { change_mode(PlayerProfileMode::TargetCV); });

  input_panel_->add_button(Icon::LEFT_ARROW, ButtonType::Square,
                           ButtonPosition::BottomLeft, "Turn page",
                           [this] { handle_arrow(PageAction::Previous); });

  input_panel_->add_button(Icon::RIGHT_ARROW, ButtonType::Square,
                           ButtonPosition::BottomCenter, "Turn page",
                           [this] { handle_arrow(PageAction::Next); });

  input_panel_->add_button(Icon::ARROW_BACK, ButtonType::Square,
                           ButtonPosition::BottomRight, "Back",
                           [this] { state_.ui().pop(); });
}

void PlayerProfile::tick() {
  if (card_slider_.update()) {
    mark_dirty();
  }
}

void PlayerProfile::render(const RenderContext& ctx) {
  interactions_.begin_frame();
  const auto* player = state_.player(id_);
  if (!background_) {
    background_ = create_overview(player);
  }
  gfx::blit::blit_raw_with_palette(*ctx.surface, *background_);

  if (mode_ == PlayerProfileMode::Overview && card_slider_.idle()) {
    auto* bob = context_.bob;
    const Size up_btn_size = bob->get("bob/stanbobs.bob").frame(39).size();
    const Size dn_btn_size = bob->get("bob/stanbobs.bob").frame(40).size();
    interactions_.create_frame_item(
        ViewRect{{258, 284}, up_btn_size}, "up_btn",
        [this](views::ui::InteractionEvent ie) {
          if (ie.type == views::ui::InteractionEvent::Type::Click &&
              ie.button == views::ui::MouseButton::Left) {
            start_of_day_ = std::min(start_of_day_ + 1, 9);
            state_.player(id_)->start_of_day_hour(
                static_cast<uint8_t>(start_of_day_));
            background_.reset();
            mark_dirty();
          }
        });
    interactions_.create_frame_item(
        ViewRect{{258, 314}, dn_btn_size}, "down_btn",
        [this](views::ui::InteractionEvent ie) {
          if (ie.type == views::ui::InteractionEvent::Type::Click &&
              ie.button == views::ui::MouseButton::Left) {
            start_of_day_ = std::max(start_of_day_ - 1, 0);
            state_.player(id_)->start_of_day_hour(
                static_cast<uint8_t>(start_of_day_));
            background_.reset();
            mark_dirty();
          }
        });
    interactions_.create_frame_item(
        ViewRect{316, 180, 34, 34}, "vacation",
        [this](views::ui::InteractionEvent ie) {
          if (ie.type == views::ui::InteractionEvent::Type::Click &&
              ie.button == views::ui::MouseButton::Left) {
            const ViewPoint popup_origin{4 + 28, 16 + 24};
            context_.popup->origin(popup_origin);
            context_.popup->create<ConfirmPopup>(
                "Do you really want to go on holiday?",
                [this, popup_origin](bool agree) {
                  if (agree) {
                    context_.popup->origin(popup_origin);
                    context_.popup->create<NumberInputPopup>(
                        "Go on holiday", "days", 7, 2, 30, 1,
                        [this](int value) {
                          state_.player(id_)->holiday(value);
                          state_.ui().pop();
                          state_.turn().request_end_of_turn();
                        });
                  }
                });
          }
        });
  }

  // foreground card (may be sliding)
  card_slider_.render(*ctx.surface, 0);
  if (card_slider_.on_screen()) {
    // A card is visible, so clicking anywhere dismisses it back to overview.
    interactions_.create_frame_item(
        ViewRect{0, 0, views::base::MAIN_PANEL_WIDTH,
                 views::base::MAIN_PANEL_HEIGHT},
        "page_dismiss", [this](views::ui::InteractionEvent ie) {
          if (ie.button == views::ui::MouseButton::Left &&
              ie.type == views::ui::InteractionEvent::Type::Click) {
            change_mode(PlayerProfileMode::Overview);
          }
        });
  }
}

Surface PlayerProfile::create_overview(const Player* player) {
  Surface s = context_.vga->get("gfx/back33").clone();

  const ViewPoint first_name_pos{160, 68};
  const ViewPoint last_name_pos{160, 108};
  auto* font = context_.font;
  std::string full_name = player->name();
  std::string first_name;
  std::string last_name;

  // Find the first space in the name; original just splits on the first space,
  // it seems, so we match that behavior. If the name is too long, so be it
  // (just overflow like the original).
  size_t space_pos = full_name.find(' ');

  if (space_pos != std::string_view::npos) {
    // Split name at the first space
    first_name = full_name.substr(0, space_pos);
    last_name = full_name.substr(space_pos + 1);
  }
  else {
    last_name = full_name;
  }

  if (!first_name.empty()) {
    font->render_text_onto(s, first_name, first_name_pos, FontFace::ASTANDAR,
                           16, AlignX::CenterPoint);
  }
  font->render_text_onto(s, last_name, last_name_pos, FontFace::ASTANDAR, 16,
                         AlignX::CenterPoint);

  const ViewPoint game_mode_pos{116, 244};
  const int line_height = 22;
  const int text_color = 240;

  auto game_mode = state_.game_mode();
  if (game_mode == GameMode::QUICKSTART) {
    // In the original it always shows "Free mode", I think it's just to
    // differentiate between mission and free mode, so quick start is just free
    // mode for this purpose.
    game_mode = GameMode::FREEMODE;
  }

  const int label_x_offset = 136;
  font->render_text_onto(s, to_string(state_.game_mode()), game_mode_pos,
                         FontFace::PIZ_NUMB, text_color);
  const ViewPoint player_count_pos{game_mode_pos.x + label_x_offset,
                                   game_mode_pos.y};
  font->render_text_onto(s, std::to_string(state_.human_player_count()),
                         player_count_pos, FontFace::PIZ_NUMB, text_color,
                         AlignX::RightEdge);

  const ViewPoint score_label_pos{game_mode_pos.x,
                                  game_mode_pos.y + line_height};
  font->render_text_onto(s, "Score", score_label_pos, FontFace::PIZ_NUMB,
                         text_color);
  const ViewPoint score_value_pos{game_mode_pos.x + label_x_offset,
                                  game_mode_pos.y + line_height};
  font->render_text_onto(s, std::to_string(player->score()), score_value_pos,
                         FontFace::PIZ_NUMB, text_color, AlignX::RightEdge);

  const ViewPoint start_of_day_pos{game_mode_pos.x + 8,
                                   score_value_pos.y + line_height};
  font->render_text_onto(s, "start of day:", start_of_day_pos,
                         FontFace::PIZ_NUMB, text_color);

  const ViewPoint oclock_pos{game_mode_pos.x + 26,
                             start_of_day_pos.y + line_height};
  font->render_text_onto(s, compat::format("{} o'clock", start_of_day_),
                         oclock_pos, FontFace::PIZ_NUMB, text_color);

  auto* bob = context_.bob;
  auto up_arrow = bob->get("bob/stanbobs.bob").frame(39);
  auto down_arrow = bob->get("bob/stanbobs.bob").frame(40);

  const ViewPoint up_pos{258, 284};
  const ViewPoint down_pos{258, 314};
  s.blit_masked(up_arrow, up_pos);
  s.blit_masked(down_arrow, down_pos);

  std::string difficulty(state_.difficulty_level(), '*');
  font->render_text_onto(s, difficulty,
                         ViewPoint{game_mode_pos.x, oclock_pos.y + line_height},
                         FontFace::PIZ_NUMB, text_color);

  return s;
}

void PlayerProfile::change_mode(PlayerProfileMode mode) {
  if (mode == mode_ && !player_switched_) {
    return;
  }

  // We use this boolean to keep track of having switch players, so that
  // we can have mode changes to the same mode is a noop, unless we changed the
  // player.
  player_switched_ = false;

  // CV and Target CV are restricted when viewing another player
  if ((mode == PlayerProfileMode::CurriculumVitae ||
       mode == PlayerProfileMode::TargetCV) &&
      id_.value != state_.active_player()->id().value) {
    state_.status().post_direct({
        .payload = "ACCESS TO OTHER PERSON'S CAREER PROHIBITED",
        .scope = StatusScope::Global,
    });
    return;
  }

  const auto* player = state_.player(id_);
  mode_ = mode;
  switch (mode) {
  case PlayerProfileMode::Overview:
    card_slider_.dismiss();
    break;
  case PlayerProfileMode::CharacterValues:
    card_slider_.replace(create_character_values(player));
    break;
  case PlayerProfileMode::Company:
    card_slider_.replace(create_company(player));
    break;
  case PlayerProfileMode::CurriculumVitae:
    cv_page_ = 0;
    card_slider_.replace(create_curriculum_vitae(player, cv_page_));
    break;
  case PlayerProfileMode::Position:
    card_slider_.replace(create_position(player));
    break;
  case PlayerProfileMode::Awards:
    card_slider_.replace(create_awards(player));
    break;
  case PlayerProfileMode::TargetCV:
    cv_page_ = 0;
    card_slider_.replace(create_target_cv(player, cv_page_));
    break;
  }
}

bool PlayerProfile::switch_to_player(core::PlayerId new_id) {
  if (new_id.value == id_.value) {
    return false;
  }

  id_ = new_id;
  background_.reset();
  player_switched_ = true;
  // Re-show the current mode for the new player; change_mode slides the old
  // card out and the new one in.
  change_mode(mode_);
  mark_dirty();
  return true;
}

void PlayerProfile::handle_arrow(PageAction action) {
  // CV and Target CV modes: page through CV pages
  if (mode_ == PlayerProfileMode::CurriculumVitae ||
      mode_ == PlayerProfileMode::TargetCV) {
    const auto* player = state_.player(id_);
    const bool target_mode = mode_ == PlayerProfileMode::TargetCV;
    std::size_t entry_count = 0;
    if (target_mode) {
      if (state_.is_mission_active()) {
        entry_count = state_.mission_target()->milestones.size();
      }
    }
    else {
      entry_count =
          state_.message_manager().get_cv_entries_for(player->id()).size();
    }
    int total_pages = cv_total_pages(entry_count);

    int new_page = (action == PageAction::Next)
                       ? std::min(cv_page_ + 1, total_pages - 1)
                       : std::max(cv_page_ - 1, 0);
    if (new_page != cv_page_) {
      cv_page_ = new_page;
      card_slider_.set_current(target_mode
                                   ? create_target_cv(player, cv_page_)
                                   : create_curriculum_vitae(player, cv_page_));
      mark_dirty();
    }
    return;
  }

  // All other modes: switch which player we're viewing
  core::PlayerId new_id = id_;
  if (action == PageAction::Next) {
    new_id.value = (id_.value + 1) % NUMBER_OF_PLAYERS;
  }
  else {
    new_id.value = (id_.value == 0) ? NUMBER_OF_PLAYERS - 1 : id_.value - 1;
  }
  switch_to_player(new_id);
}
Surface PlayerProfile::create_character_values(const Player* player) {
  Surface char_val = blue_paper_.clone();
  const auto& character = player->character();

  views::helpers::draw_framed_portrait(char_val, character, {42, 20});

  auto* font = context_.font;
  constexpr int text_color = 240;
  constexpr int headers_x = 178;
  int line_height = 20;
  int y = 12;
  font->render_line_onto(char_val, "Character values", {headers_x, y},
                         FontFace::PIZ_NUMB, text_color);
  y += line_height;
  font->render_line_onto(char_val, player->name(), {headers_x, y},
                         FontFace::PIZ_NUMB, text_color);
  y += line_height;
  font->render_line_onto(char_val, compat::format("{} Years", character.age()),
                         {headers_x, y}, FontFace::PIZ_NUMB, text_color);

  constexpr int labels_x = 98;
  constexpr int values_x = 334;
  y = 170;
  line_height = 18;
  struct StatRow {
    const char* label;
    uint8_t (core::character::Character::*getter)() const;
  };
  static constexpr std::array<StatRow, 10> stat_rows{{
      {"Health", &core::character::Character::health},
      {"Energy", &core::character::Character::energy},
      {"Popularity", &core::character::Character::popularity},
      {"Cooking", &core::character::Character::cooking},
      {"Selling", &core::character::Character::serving},
      {"Management", &core::character::Character::management},
      {"Personality", &core::character::Character::personality},
      {"Intelligence", &core::character::Character::intelligence},
      {"Charisma", &core::character::Character::charisma},
      {"Courage", &core::character::Character::courage},
  }};

  for (const StatRow& row : stat_rows) {
    const int value = (character.*(row.getter))();

    font->render_line_onto(char_val, row.label, {labels_x, y},
                           FontFace::PIZ_NUMB, text_color);

    font->render_line_onto(char_val, std::to_string(value), {values_x, y},
                           FontFace::PIZ_NUMB, text_color, AlignX::RightEdge);

    y += line_height;
  }

  return char_val;
}
Surface PlayerProfile::create_company(const Player* player) {
  Surface page = blue_paper_.clone();
  constexpr int text_color = 240;
  auto* font = context_.font;

  // Restaurant chain
  font->render_text_onto(page, "Restaurant chain", {192, 24},
                         FontFace::PIZ_NUMB, text_color, AlignX::CenterPoint);

  // Company name
  font->render_text_onto(page, player->name(), {192, 48}, FontFace::PIZ_NUMB,
                         text_color, AlignX::CenterPoint);

  // Owner: name
  const auto& character = player->character();
  std::string owner_line = compat::format(
      "Owner: {} {}", character.first_name(), character.last_name());
  font->render_line_onto(page, owner_line, {62, 68}, FontFace::PIZ_NUMB,
                         text_color);

  // Weekly profit
  font->render_line_onto(page, "Weekly profit", {120, 108}, FontFace::PIZ_NUMB,
                         text_color);

  // Separator line
  page.fill_rect({56, 100, 282, 1}, text_color);

  // city rows
  int y = 132;
  constexpr int row_height = 18;
  for (int i = 0; i < NUMBER_OF_CITIES; ++i) {
    auto city_id = core::CityId{i};

    font->render_line_onto(page, city_names[i], {78, y}, FontFace::PIZ_NUMB,
                           text_color);

    if (state_.is_city_loaded(city_id)) {
      Money city_profit{};
      for (auto* rest : state_.restaurants_for(player->id())) {
        if (rest && rest->property().city().id() == city_id) {
          city_profit = city_profit + rest->last_weekly_profit();
        }
      }
      font->render_line_onto(page, compat::format("{:i}", city_profit),
                             {336, y}, FontFace::PIZ_NUMB, text_color,
                             AlignX::RightEdge);
    }
    else {
      font->render_line_onto(page, "---------------", {188, y},
                             FontFace::PIZ_NUMB, text_color);
    }
    y += row_height;
  }

  return page;
}

Surface PlayerProfile::create_curriculum_vitae(const Player* player, int page) {
  Surface cv = blue_paper_.clone();
  constexpr int text_color = 240;
  auto* font = context_.font;

  font->render_text_onto(cv, "Curriculum Vitae", {192, 24}, FontFace::PIZ_NUMB,
                         text_color, AlignX::CenterPoint);

  const auto& character = player->character();
  std::string name =
      compat::format("{} {}", character.first_name(), character.last_name());
  font->render_text_onto(cv, name, {192, 48}, FontFace::PIZ_NUMB, text_color,
                         AlignX::CenterPoint);

  constexpr int date_x = 72;
  constexpr int text_x = 160;
  int y = 80;
  constexpr int row_height = 40;

  auto entries = state_.message_manager().get_cv_entries_for(player->id());
  int start = page * cv_events_per_page;
  int end =
      std::min(start + cv_events_per_page, static_cast<int>(entries.size()));

  for (int i = start; i < end; ++i) {
    const auto& entry = entries[i];
    font->render_line_onto(cv, entry.date.cv_date_string(), {date_x, y},
                           FontFace::PIZ_NUMB, text_color);
    font->render_text_onto(
        cv, state_.message_manager().get_cv_text(entry.message_id), {text_x, y},
        FontFace::PIZ_NUMB, text_color);
    y += row_height;
  }

  int total_pages = cv_total_pages(entries.size());
  font->render_line_onto(
      cv, compat::format(" - {}/{} -", page + 1, total_pages), {192, 330},
      FontFace::PIZ_NUMB, text_color, AlignX::CenterPoint);

  return cv;
}

Surface PlayerProfile::create_position(const Player* player) {
  Surface page = position_paper_.clone();
  auto* font = context_.font;

  constexpr int bar_max_width = 138;
  constexpr int bar_height = 24;
  constexpr uint8_t bar_color = 28;
  constexpr int bar_y = 312;
  constexpr int bar_y_text_offset = 4;

  int difficulty = state_.difficulty_level();
  const auto& status = player->social_status();

  // Load rank names from LEBEN.E (lines 0-13 social, 14-27 underworld)
  constexpr int relevant_line_count = 28;
  auto leben_lines = fileutils::load_text_lines("t/leben.e");
  leben_lines.resize(relevant_line_count);

  // Underworld reputation (left)
  constexpr int underworld_x = 54;
  int underworld_rank_val = social_rank(status.underworld_score(), difficulty);
  int underworld_rank_idx = underworld_rank_val + 14;
  double underworld_progress = social_rank_progress(
      status.underworld_score(), underworld_rank_val, difficulty);
  int underworld_fill = static_cast<int>(underworld_progress * bar_max_width);
  page.fill_rect({underworld_x, bar_y, underworld_fill, bar_height}, bar_color);

  std::string underworld_name =
      (underworld_rank_idx < static_cast<int>(leben_lines.size()))
          ? leben_lines[underworld_rank_idx]
          : "";
  font->render_line_onto(page, underworld_name,
                         {underworld_x + 4, bar_y + bar_y_text_offset},
                         FontFace::SUPERSMA, 240);

  // Social status (right)
  constexpr int social_x = 218;
  int social_rank_val = social_rank(status.social_score(), difficulty);
  double social_progress =
      social_rank_progress(status.social_score(), social_rank_val, difficulty);
  int social_fill = static_cast<int>(social_progress * bar_max_width);
  page.fill_rect({social_x, bar_y, social_fill, bar_height}, bar_color);

  int social_line_idx = social_rank_val;
  std::string social_name =
      (social_line_idx < static_cast<int>(leben_lines.size()))
          ? leben_lines[social_line_idx]
          : "";
  font->render_line_onto(page, social_name,
                         {social_x + 4, bar_y + bar_y_text_offset},
                         FontFace::SUPERSMA, 240);

  return page;
}

Surface PlayerProfile::create_awards(const Player* player) {
  Surface page = awards_board_.clone();
  auto* font = context_.font;

  // Title: "awards" centered
  font->render_text_onto(page, "awards", {192, 34}, FontFace::RUBY, 16,
                         AlignX::CenterPoint);

  const auto& awards = player->awards();
  for (size_t i = 0; i < awards.size(); ++i) {
    AwardLevel level = awards[i];

    if (level == AwardLevel::None) {
      continue;
    }
    // Source: column = award type, row = level (1=bronze, 2=silver, 3=gold)
    const int src_x = i * sprite_width;
    const int src_y = (static_cast<int>(level) - 1) * sprite_height;

    ViewRect src_rect{src_x, src_y, sprite_width, sprite_height};

    // Destination position on screen
    const int col = i % grid_columns;
    const int row = i / grid_columns;

    const int dst_x =
        (base_x + col * step_x) * 2 - LegacyConstants::WOOD_BORDER_WIDTH_LEFT;

    const int dst_y =
        (base_y + row * step_y) * 2 - LegacyConstants::WOOD_BORDER_HEIGHT_TOP;

    page.blit(orden_sheet_, src_rect, {dst_x, dst_y});
  }

  return page;
}

Surface PlayerProfile::create_target_cv(const Player* player, int page) {
  Surface cv = blue_paper_.clone();
  constexpr int text_color = 240;
  auto* font = context_.font;

  if (!state_.is_mission_active()) {
    font->render_text_onto(cv, "Target curriculum vitae|in mission mode only",
                           {192, 60}, FontFace::ASTANDAR, text_color,
                           AlignX::CenterPoint);
    return cv;
  }

  font->render_text_onto(cv, "Target Curriculum Vitae", {192, 24},
                         FontFace::PIZ_NUMB, text_color, AlignX::CenterPoint);

  const auto& target = *state_.mission_target();

  // How closely each target milestone has been matched so far (by event,
  // scored on date closeness; 0% until the event is achieved).
  const auto scores = core::mission::per_milestone_scores(
      target, state_.message_manager().get_cv_entries_for(player->id()));

  constexpr int date_x = 72;
  constexpr int text_x = 160;
  constexpr int percent_x = 132;
  int y = 80;
  constexpr int row_height = 40;

  const auto& milestones = target.milestones;
  int start = page * cv_events_per_page;
  int end =
      std::min(start + cv_events_per_page, static_cast<int>(milestones.size()));

  for (int i = start; i < end; ++i) {
    const auto& milestone = milestones[i];
    font->render_line_onto(cv, milestone.date.cv_date_string(), {date_x, y},
                           FontFace::PIZ_NUMB, text_color);
    font->render_text_onto(
        cv, state_.message_manager().get_cv_text(milestone.message_id),
        {text_x, y}, FontFace::PIZ_NUMB, text_color);
    font->render_line_onto(cv, compat::format("{}%", scores[i]),
                           {percent_x, y + (row_height / 2)},
                           FontFace::PIZ_NUMB, text_color, AlignX::RightEdge);
    y += row_height;
  }

  int total_pages = cv_total_pages(milestones.size());
  font->render_line_onto(
      cv, compat::format(" - {}/{} -", page + 1, total_pages), {192, 330},
      FontFace::PIZ_NUMB, text_color, AlignX::CenterPoint);

  return cv;
}
