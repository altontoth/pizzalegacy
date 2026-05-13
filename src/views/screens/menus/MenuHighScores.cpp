/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/menus/MenuHighScores.h"
#include "core/save/SaveGameManager.h"
#include "core/state/GameState.h"
#include "engine/FormatCompat.h"
#include "resources/FontManager.h"
#include "views/screens/menus/MenuTheme.h"
#include "views/ui/widgets/PositionMarkerMenuItem.h"
#include "views/ui/widgets/TextMenuItem.h"

namespace {

constexpr int col_position_x = 0;
constexpr int col_name_x = 40;
constexpr int col_score_right_x = 260;

class HighScoreRowItem : public MenuItem {
public:
  HighScoreRowItem(GameContext& gc, int position, save::HighScoreEntry e,
                   FontFace face)
      : MenuItem(gc, face), position_(position), entry_(std::move(e)) {}

  void draw(Surface& screen, ViewPoint p, bool /*is_selected*/) override {
    constexpr TextStyle left{.align = AlignX::Left};
    constexpr TextStyle right{.align = AlignX::RightEdge};

    auto pos_str = compat::format("{}.", position_);
    auto score_str = compat::format("{}", entry_.score);

    context_.font->render_line_onto(
        screen, pos_str, {p.x + col_position_x, p.y}, font_face_, left);
    context_.font->render_line_onto(screen, entry_.name,
                                    {p.x + col_name_x, p.y}, font_face_, left);
    context_.font->render_line_onto(
        screen, score_str, {p.x + col_score_right_x, p.y}, font_face_, right);

    auto sz = measure();
    width_ = sz.w;
    height_ = sz.h;
  }

  void handle_action() override {}

  Size measure() const override {
    auto metrics = context_.font->size_text(entry_.name, font_face_);
    return {col_score_right_x, metrics.height};
  }

private:
  int position_;
  save::HighScoreEntry entry_;
};

} // namespace

using namespace views::ui::menu;

MenuHighScores::MenuHighScores(GameContext& context, GameState& state)
    : Menu(context, state, ViewPoint{120, 140}, 3) {

  const FontFace face = FontFace::PIZ_NUMB;

  auto entries = save::SaveGameManager::load_high_scores();

  if (entries.empty()) {
    add_menu_item(std::make_unique<TextMenuItem>(
        context, "No high scores yet.", []() {}, face));
  }
  else {
    for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
      add_menu_item(
          std::make_unique<HighScoreRowItem>(context, i + 1, entries[i], face));
    }
  }

  add_menu_item(std::make_unique<PositionMarkerMenuItem>(context, 156, 234));
  add_menu_item(std::make_unique<TextMenuItem>(
      context, "Back", [this] { state_.ui().pop(); }, FontFace::RUBY, style,
      sel_style));

  update_selection(nullptr);
}

Surface MenuHighScores::background() const {
  return default_background("High Scores");
}
