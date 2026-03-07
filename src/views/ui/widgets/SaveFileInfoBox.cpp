/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/widgets/SaveFileInfoBox.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

#include "core/save/SaveGameManager.h"
#include "core/util/Constants.h"
#include "engine/FormatCompat.h"
#include "engine/Logger.h"
#include "engine/geom/Spaces.h"
#include "resources/FontManager.h"

using json = nlohmann::json;

constexpr int INFO_BOX_W = 350;
constexpr int INFO_BOX_H = 150;
constexpr int LINE_HEIGHT = 14;
constexpr int TEXT_COLOR = 240;
constexpr int LABEL_COLOR = 1;

void SaveFileInfoBox::set_file(const std::string& filename) {
  info_ = SaveFileInfo{};
  has_info_ = false;

  try {
    namespace fs = std::filesystem;
    auto full_path =
        fs::path(save::SaveGameManager::get_saves_directory()) / filename;

    std::ifstream file(full_path);
    if (!file.is_open()) {
      return;
    }

    json j;
    file >> j;

    info_.filename = filename;
    // Strip .json extension for display
    if (info_.filename.size() > 5 &&
        info_.filename.substr(info_.filename.size() - 5) == ".json") {
      info_.filename = info_.filename.substr(0, info_.filename.size() - 5);
    }

    info_.saved_at = j.value("saved_at", "Unknown");

    if (j.contains("time") && j["time"].contains("display")) {
      info_.game_date = j["time"]["display"].get<std::string>();
    }

    if (j.contains("players")) {
      for (size_t i = 0; i < j["players"].size() && i < NUMBER_OF_PLAYERS;
           ++i) {
        const auto& p = j["players"][i];
        if (p.is_null()) {
          continue;
        }
        info_.player_names[i] = p.value("name", "");
      }
    }

    info_.active_player = j.value("active_player", -1);
    info_.active_city = j.value("active_city", -1);

    has_info_ = true;
  }
  catch (const std::exception& e) {
    Logger::warn("Failed to read save file info: {}", e.what());
  }
}

void SaveFileInfoBox::clear() {
  has_info_ = false;
  info_ = SaveFileInfo{};
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

  context_.font->render_line_onto(block,
                                  compat::format("File: {}", info_.filename),
                                  text_pos, FontFace::SUPERSMA, TEXT_COLOR);
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
