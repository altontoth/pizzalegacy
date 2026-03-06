/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/widgets/CharacterSelectionBox.h"
#include "engine/geom/Spaces.h"
#include "resources/FontManager.h"

constexpr int CHAR_BOX_W = 224;
constexpr int CHAR_BOX_H = 224;
constexpr int max_name_length = 19;

void CharacterSelectionBox::draw(Surface& screen, ViewPoint p,
                                 bool is_selected) {
  auto* character = *character_;
  auto block = Surface(CHAR_BOX_W, CHAR_BOX_H, PaletteHandle{38}, 15);

  ViewRect r = ViewRect{2, 0, block.width() - 2, block.height()};
  uint8_t light_gray = 14;
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
                                  FontFace::PIZ_NUMB, BLACK);
  this_y += y_offset;

  for (const auto& item : items) {
    context_.font->render_text_onto(block, item.label,
                                    ViewPoint{col1_x, this_y},
                                    FontFace::PIZ_NUMB, BLACK);

    context_.font->render_text_onto(block, item.get_value(),
                                    // Because we're right aligned we move it a
                                    // bit away from the right border this way:
                                    ViewPoint{-8, this_y}, FontFace::PIZ_NUMB,
                                    BLACK, AlignX::Right);

    this_y += y_offset;
  }
  screen.blit_raw(block, p);
  height_ = block.height();
}

Size CharacterSelectionBox::measure() const {
  return Size{CHAR_BOX_W, CHAR_BOX_H};
}

void CharacterSelectionBox::handle_action() {}

bool CharacterSelectionBox::on_interaction(views::ui::InteractionEvent ie) {
  if (ie.type == views::ui::InteractionEvent::Type::Scroll) {
    if (on_scroll_) {
      on_scroll_(ie.scroll_delta);
      return true;
    }
    return false;
  }

  // we care only about key events routed to us by the menu
  if (ie.type != views::ui::InteractionEvent::Type::Click) {
    return false;
  }
  keyboard_enabled_ = true;
  rename_active_ = true;
  return true;
}

bool CharacterSelectionBox::on_text(const input::TextEvent& ev) {
  new_name_.append(ev.text);
  if (new_name_.size() > max_name_length) {
    new_name_.resize(max_name_length); // trim extra characters
  }
  mark_dirty();
  return true;
}

bool CharacterSelectionBox::on_action(const input::ActionEvent& ev) {
  if (!rename_active_ || ev.state.pressed) {
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
      new_name_.pop_back();
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

void CharacterSelectionBox::end_rename() {
  rename_active_ = false;
  keyboard_enabled_ = false;
  new_name_.clear();
  text_session_.reset();
}
