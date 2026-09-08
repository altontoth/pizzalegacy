/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/GameLocationScreen.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "core/state/GameState.h"
#include "engine/DisplayConstants.h"
#include "engine/PlatformPaths.h"
#include "engine/geom/Spaces.h"
#include "engine/gfx/Draw.h"
#include "engine/input/Action.h"
#include "engine/input/InputManager.h"
#include "engine/ui/InteractionEvent.h"
#include "resources/FontManager.h"
#include "resources/FontTypes.h"
#include "resources/PaletteLoader.h"
#include "resources/VgaManager.h"
#include "resources/util/FileUtils.h"
#include "views/screens/GameLocationContent.h"

namespace {

constexpr int margin = 60;
constexpr int title_y = 40;
constexpr int headline_y = 108;
constexpr int hint_y = 152;

constexpr int button_padding = 8;

constexpr int hover_frame_width = 2;

constexpr int paragraph_spacing = 12;

// Text starts on an even x so it survives the halving into the VGA.
int even_down(int v) {
  if (v % 2 != 0) {
    return v - 1;
  }
  return v;
}

std::vector<std::string_view> paragraphs(std::string_view text) {
  std::vector<std::string_view> out;
  std::size_t start = 0;
  while (start < text.size()) {
    const std::size_t brk = text.find("\n\n", start);
    if (brk == std::string_view::npos) {
      out.push_back(text.substr(start));
      break;
    }
    out.push_back(text.substr(start, brk - start));
    start = brk + 2;
  }
  return out;
}

} // namespace

GameLocationScreen::GameLocationScreen(GameContext& context, GameState& state)
    : View(context, state), listener_(context.input->register_listener(*this)) {
  if (fileutils::game_data_path().empty()) {
    picture_ = &context_.vga->get(platform_paths::data_dir() + "game_location");
  }
  create_button(game_location::browse_button, "gameloc.browse", Choice::Browse);
  create_button(game_location::exit_button, "gameloc.exit", Choice::Exit);
}

void GameLocationScreen::create_button(ViewRect rect, std::string_view id,
                                       Choice choice) {
  interactions_.create_persistent_item(
      rect, id, [this, rect, choice](views::ui::InteractionEvent ev) {
        using Type = views::ui::InteractionEvent::Type;
        if (ev.type == Type::Enter) {
          hovered_ = rect;
          mark_dirty();
        }
        else if (ev.type == Type::Leave) {
          hovered_.reset();
          mark_dirty();
        }
        else if (ev.type == Type::Click) {
          choose(choice);
        }
      });
}

void GameLocationScreen::choose(Choice choice) {
  if (choice == Choice::Exit) {
    state_.request_exit();
    return;
  }
  dialog_.open();
}

bool GameLocationScreen::on_action(const input::ActionEvent& ev) {
  if (!ev.state.pressed) {
    return false;
  }
  if (ev.action == input::Action::SubmitInput) {
    choose(Choice::Browse);
    return true;
  }
  if (ev.action == input::Action::CancelInput) {
    choose(Choice::Exit);
    return true;
  }
  return false;
}

void GameLocationScreen::tick() {
  const bool was_open = dialog_.waiting();
  if (std::optional<std::string> folder = dialog_.take_choice()) {
    state_.request_game_path(*folder);
  }
  if (was_open && !dialog_.waiting()) {
    state_.request_window_raise();
  }
}

void GameLocationScreen::draw_button(const RenderContext& ctx, ViewRect rect,
                                     std::string_view label) {
  gfx::draw::box(*ctx.surface, rect, resources::fallback_palette::black,
                 game_location::button_border);
  draw_centered(ctx, label, FontFace::ASTANDAR, rect.center().x,
                rect.y + button_padding);
}

void GameLocationScreen::draw_centered(const RenderContext& ctx,
                                       std::string_view text, FontFace face,
                                       int center_x, int y) {
  const TextStyle style{.color = resources::fallback_palette::black};
  const int width = context_.font->size_text(text, face, style).width;
  context_.font->render_line_onto(*ctx.surface, text,
                                  ViewPoint{even_down(center_x - width / 2), y},
                                  face, style);
}

void GameLocationScreen::render(const RenderContext& ctx) {
  if (picture_) {
    ctx.surface->blit_raw(*picture_);
    draw_hover(ctx);
    return;
  }

  ctx.surface->fill(resources::fallback_palette::white);

  constexpr int center_x = DisplayConstants::SCREEN_WIDTH / 2;
  draw_centered(ctx, "Pizza Legacy", FontFace::MENU, center_x, title_y);
  draw_centered(ctx, game_location::headline, FontFace::ASTANDAR, center_x,
                headline_y);

  const ViewRect body{margin, hint_y,
                      DisplayConstants::SCREEN_WIDTH - 2 * margin,
                      DisplayConstants::SCREEN_HEIGHT - hint_y - margin};
  const TextStyle hint{.color = resources::fallback_palette::black};
  ViewRect line = body;
  for (std::string_view paragraph : paragraphs(game_location::hint)) {
    const auto metrics = context_.font->render_into_rect(
        *ctx.surface, paragraph, line, FontFace::PIZ_NUMB, hint);
    line.y += metrics.height + paragraph_spacing;
    line.h = body.y + body.h - line.y;
  }

  draw_button(ctx, game_location::browse_button, game_location::browse_label);
  draw_button(ctx, game_location::exit_button, game_location::exit_label);
  draw_hover(ctx);
}

void GameLocationScreen::draw_hover(const RenderContext& ctx) {
  if (!hovered_) {
    return;
  }
  const ViewRect& r = *hovered_;
  const int overhang = game_location::button_border - 1;
  gfx::draw::box(*ctx.surface,
                 ViewPoint{r.x - hover_frame_width, r.y - hover_frame_width},
                 ViewPoint{r.x + r.w + overhang, r.y + r.h + overhang},
                 resources::fallback_palette::cursor_red, hover_frame_width);
}
