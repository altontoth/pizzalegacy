/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/widgets/CharacterSelectionBox.h"
#include "engine/geom/Spaces.h"
#include "gfx/Draw.h"
#include "resources/FontManager.h"
#include "resources/util/TextEncoding.h"

constexpr int CHAR_BOX_W = 224;
constexpr int CHAR_BOX_H = 224;
constexpr int max_name_length = 19;

void CharacterSelectionBox::draw(Surface& screen, ViewPoint p,
                                 bool is_selected) {
  auto* character = *character_;
  auto block = Surface(CHAR_BOX_W, CHAR_BOX_H, PaletteHandle{38}, 15);

  ViewRect r = ViewRect{2, 0, block.width() - 2, block.height()};
  uint8_t white = 15;
  block.fill_rect(r, white);

  struct RenderItem {
    std::string label;
    std::function<std::string()> get_value;
  };

  std::vector<RenderItem> items = {
      {.label = "money",
       .get_value =
           [&]() { return compat::format("{:w}", character->starting_money); }},
      {.label = "age",
       .get_value = [&]() { return std::to_string(character->age); }},
      {.label = "popuIarity",
       .get_value = [&]() { return std::to_string(character->popularity); }},
      {.label = "health",
       .get_value = [&]() { return std::to_string(character->health); }},
      {.label = "energy",
       .get_value = [&]() { return std::to_string(character->energy); }},
      {.label = "intelligence",
       .get_value = [&]() { return std::to_string(character->intelligence); }},
      {.label = "charisma",
       .get_value = [&]() { return std::to_string(character->charisma); }},
      {.label = "dexterity",
       .get_value = [&]() { return std::to_string(character->dexterity); }},
      {.label = "personality",
       .get_value = [&]() { return std::to_string(character->personality); }},
      {.label = "cooking",
       .get_value = [&]() { return std::to_string(character->cooking); }},
      {.label = "courage",
       .get_value = [&]() { return std::to_string(character->courage); }}};

  constexpr int y_offset = 18;

  int col1_x = 4;

  int this_y = 4;

  std::string character_name(rename_active_
                                 ? (new_name_ + "_")
                                 : players_[*active_player_].character_name);
  context_.font->render_text_onto(block, character_name,
                                  ViewPoint{col1_x - 1, this_y},
                                  FontFace::PIZ_NUMB, 1);
  this_y += y_offset;

  for (const auto& item : items) {
    context_.font->render_text_onto(
        block, item.label, ViewPoint{col1_x, this_y}, FontFace::PIZ_NUMB, 1);

    context_.font->render_text_onto(block, item.get_value(),
                                    ViewPoint{block.width() - 8, this_y},
                                    FontFace::PIZ_NUMB, 1, TextAnchor::End);

    this_y += y_offset;
  }
  screen.blit_raw(block, p);
  if (is_selected) {
    constexpr uint8_t focus_color = 29;
    gfx::draw::box(screen, ViewRect{p.x, p.y, CHAR_BOX_W, CHAR_BOX_H},
                   focus_color, 2);
  }
  height_ = block.height();
}

Size CharacterSelectionBox::measure() const {
  return Size{CHAR_BOX_W, CHAR_BOX_H};
}

void CharacterSelectionBox::handle_action() { start_rename(); }

bool CharacterSelectionBox::on_adjust(AdjustAxis axis, int delta) {
  if (axis != AdjustAxis::Vertical || !on_scroll_) {
    return false;
  }
  on_scroll_(-delta);
  return true;
}

bool CharacterSelectionBox::on_interaction(views::ui::InteractionEvent ie) {
  if (ie.type == views::ui::InteractionEvent::Type::Scroll) {
    if (on_scroll_) {
      on_scroll_(ie.scroll_delta);
      return true;
    }
    return false;
  }

  // Consume the press; mouse behavior happens on Click.
  if (ie.type == views::ui::InteractionEvent::Type::ButtonDown) {
    return true;
  }
  if (ie.type != views::ui::InteractionEvent::Type::Click) {
    return false;
  }
  start_rename();
  return true;
}

bool CharacterSelectionBox::on_text(const input::TextEvent& ev) {
  new_name_.append(ev.text);
  text::truncate_utf8(new_name_, max_name_length);
  mark_dirty();
  return true;
}

bool CharacterSelectionBox::on_action(const input::ActionEvent& ev) {
  if (!rename_active_ || !ev.state.pressed) {
    return false;
  }
  switch (ev.action) {
  case input::Action::SubmitInput:
    if (!new_name_.empty()) {
      players_[*active_player_].character_name = new_name_;
    }
    end_rename();
    break;
  case input::Action::CancelInput:
    end_rename();
    break;
  case input::Action::Backspace:
    if (new_name_.length() > 0) {
      text::pop_back_utf8(new_name_);
    }
    break;
  case input::Action::EraseLine:
    new_name_.clear();
    break;
  default:
    return false;
  }
  mark_dirty();
  return true;
}

void CharacterSelectionBox::mark_dirty() {
  if (invalidate_) {
    invalidate_();
  }
}

void CharacterSelectionBox::begin_keyboard_session(input::InputManager* input,
                                                   std::function<void()> inv) {
  if (!keyboard_enabled_) {
    Logger::warn("Requesting keyboard session when we already have one!");
    return;
  }
  invalidate_ = std::move(inv);
  text_session_.emplace(input->start_text_input(*this));
}

void CharacterSelectionBox::start_rename() {
  keyboard_enabled_ = true;
  rename_active_ = true;
}

void CharacterSelectionBox::end_rename() {
  rename_active_ = false;
  keyboard_enabled_ = false;
  new_name_.clear();
  text_session_.reset();
}
