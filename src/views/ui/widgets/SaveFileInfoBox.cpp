/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/widgets/SaveFileInfoBox.h"

#include "core/save/SaveGameManager.h"
#include "core/util/Constants.h"
#include "engine/FormatCompat.h"
#include "engine/geom/Spaces.h"
#include "resources/FontManager.h"

constexpr int INFO_BOX_W = 350;
constexpr int INFO_BOX_H = 150;
constexpr int LINE_HEIGHT = 14;
constexpr int TEXT_COLOR = 240;
constexpr int LABEL_COLOR = 1;

void SaveFileInfoBox::set_file(const std::string& filename) {
  info_ = save::SaveMetadata{};
  has_info_ = false;

  auto maybe_meta = save::SaveGameManager::read_save_metadata(filename);
  if (!maybe_meta) {
    return;
  }

  info_ = std::move(*maybe_meta);
  has_info_ = true;
}

void SaveFileInfoBox::clear() {
  has_info_ = false;
  info_ = save::SaveMetadata{};
}

void SaveFileInfoBox::draw(Surface& screen, ViewPoint p, bool is_selected) {
  auto block = Surface(INFO_BOX_W, INFO_BOX_H, PaletteHandle{38}, 15);
  block.fill(14);

  uint8_t white = 15;
  block.fill_rect(block.rect().inset(2, 2), white);

  ViewPoint text_pos{6, 6};

  if (!has_info_) {
    context_.font->render_line_onto(block, "Select a save file", text_pos,
                                    FontFace::SUPERSMA, LABEL_COLOR);
    screen.blit_raw(block, p);
    return;
  }

  context_.font->render_line_onto(
      block, compat::format("File: {}", info_.display_name), text_pos,
      FontFace::SUPERSMA, TEXT_COLOR);
  text_pos.y += LINE_HEIGHT;

  context_.font->render_line_onto(block,
                                  compat::format("Saved: {}", info_.saved_at),
                                  text_pos, FontFace::SUPERSMA, TEXT_COLOR);
  text_pos.y += LINE_HEIGHT;

  if (!info_.game_date.empty()) {
    context_.font->render_line_onto(
        block, compat::format("Game date: {}", info_.game_date), text_pos,
        FontFace::SUPERSMA, TEXT_COLOR);
    text_pos.y += LINE_HEIGHT;
  }

  context_.font->render_line_onto(block, "Players:", text_pos,
                                  FontFace::SUPERSMA, LABEL_COLOR);
  text_pos.y += LINE_HEIGHT;

  constexpr int COL2_OFFSET = INFO_BOX_W / 2;
  constexpr int ROWS = NUMBER_OF_PLAYERS / 2;
  for (int row = 0; row < ROWS; ++row) {
    int left = row;
    int right = row + ROWS;
    auto left_text =
        compat::format("{}: {}", left + 1, info_.player_names[left]);
    auto right_text =
        compat::format("{}: {}", right + 1, info_.player_names[right]);
    context_.font->render_line_onto(block, left_text, text_pos,
                                    FontFace::SUPERSMA, TEXT_COLOR);
    context_.font->render_line_onto(block, right_text,
                                    {text_pos.x + COL2_OFFSET, text_pos.y},
                                    FontFace::SUPERSMA, TEXT_COLOR);
    text_pos.y += LINE_HEIGHT;
  }

  if (info_.active_player >= 0) {
    constexpr std::array<const char*, NUMBER_OF_CITIES> CITY_NAMES = {
        "Paris",    "Berlin", "Baltimore", "Vienna",    "Chicago",
        "New York", "Madrid", "Rome",      "Stockholm", "London",
    };

    std::string player_name = (info_.active_player < NUMBER_OF_PLAYERS)
                                  ? info_.player_names[info_.active_player]
                                  : "?";
    std::string city_name =
        (info_.active_city >= 0 && info_.active_city < NUMBER_OF_CITIES)
            ? CITY_NAMES[info_.active_city]
            : "?";

    context_.font->render_line_onto(
        block, compat::format("Active: {} ({})", player_name, city_name),
        text_pos, FontFace::SUPERSMA, TEXT_COLOR);
  }

  screen.blit_raw(block, p);
}

Size SaveFileInfoBox::measure() const { return Size{INFO_BOX_W, INFO_BOX_H}; }

void SaveFileInfoBox::handle_action() {}

bool SaveFileInfoBox::on_interaction(views::ui::InteractionEvent ie) {
  return false;
}
