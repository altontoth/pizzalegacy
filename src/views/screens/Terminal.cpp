/*
 * SPDX-FileCopyrightText: 2014 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/Terminal.h"
#include "core/city/City.h"
#include "core/state/GameState.h"

#include <cassert>
#include <cstdlib>
#include "core/GameTypes.h"
#include "engine/audio/AudioManager.h"
#include "engine/geom/Spaces.h"
#include "engine/ui/InteractionEvent.h"
#include "gfx/Blit.h"
#include "gfx/Draw.h"
#include "magic_enum/magic_enum.hpp"
#include "resources/FontManager.h"
#include "resources/FontTypes.h"

namespace {
constexpr int blitmap_offset_x = 76;
constexpr int blitmap_offset_y = 50;
constexpr int blitmap_box_w = 218;
constexpr int blitmap_box_h = 42;
constexpr int blitmap_box_spacing = 4;

constexpr int prompt_color = 13;    /* beige */
constexpr int highlight_color = 26; /* bright green */
} // namespace

const std::map<TerminalCategory, std::string> category_to_string = {
    {TerminalCategory::REALTORS, "Realtors/Estate Agents"},
    {TerminalCategory::BANKS, "Banks"},
    {TerminalCategory::INSURANCE, "Insurance Companies"},
    {TerminalCategory::AUTHORITIES, "Authorities"},
    {TerminalCategory::OTHERS, "Others"},
};

const std::map<TerminalCategory, std::vector<ActorType>>
    category_to_actor_types = {
        {TerminalCategory::REALTORS, {ActorType::REALTOR}},
        {TerminalCategory::BANKS, {ActorType::BANK}},
        {TerminalCategory::INSURANCE, {ActorType::INSURANCE}},
        {TerminalCategory::AUTHORITIES, {ActorType::MAYOR, ActorType::POLICE}},
        {TerminalCategory::OTHERS, {ActorType::MAFIA, ActorType::ARMS_DEALER}},
};

Terminal::Terminal(GameContext& context, GameState& state, TerminalState ts)
    : MainPanelView(context, state),
      category_background_(context_.vga->get("gfx/blitmap5")),
      current_category_(TerminalCategory::NONE), set_dial_submenu_(false),
      prompt_animator_(
          std::make_unique<TypewriterTextRenderer>(context_.font)) {

  if (ts.category) {
    current_category_ = *ts.category;
    transition_to_phase(DialingPhase::SUCCESSFUL_CONNECTION);
  }
  reset_dialed_number();

  standard_inputmenu();
}

Terminal::~Terminal() {
  // Just in case
  context_.audio->stop_sound(audio::SoundId::PHONE_RINGING);
}

void Terminal::draw_main_menu(const RenderContext& ctx) {
  assert(input_panel_);
  // First, put down the background_ to clear whatever was on the screen.
  //  The part of the blitmap we want:
  ViewRect clip;
  clip.w = 218;
  clip.h = 234;
  clip.x = clip.y = 0;
  ctx.surface->blit(category_background_, clip,
                    {blitmap_offset_x, blitmap_offset_y});

  context_.font->render_text_onto(
      *ctx.surface, "INTERNATIONAL TELENET TERMINAL", ViewPoint{6, 6},
      FontFace::CSTANDAR, prompt_color);
  context_.font->render_text_onto(
      *ctx.surface, "LOADING MULTIMEDIA LIBRARY . . .", ViewPoint{6, 20},
      FontFace::CSTANDAR, prompt_color);

  // TODO: Some less hardcoded way of figuring out the positions, make it more
  // centered too
  ViewRect location(blitmap_offset_x, blitmap_offset_y, 218, 42);
  int x = -6;
  int y = 68;
  int y_offset = 45;
  for (const auto& [key, value] : category_to_string) {
    context_.font->render_text_onto(*ctx.surface, value, ViewPoint{x, y},
                                    FontFace::PIZ_NUMB, highlight_color,
                                    AlignX::Center);
    y += y_offset;
  }

  TerminalCategory categories[] = {
      TerminalCategory::REALTORS,  TerminalCategory::BANKS,
      TerminalCategory::INSURANCE, TerminalCategory::AUTHORITIES,
      TerminalCategory::OTHERS,
  };

  for (int i = 0; i < 5; ++i) {
    TerminalCategory category = categories[i];
    const ViewRect hitbox{blitmap_offset_x,
                          blitmap_offset_y + (blitmap_box_h * i) +
                              (blitmap_box_spacing * i),
                          blitmap_box_w, blitmap_box_h};
    interactions_.create_frame_item(
        hitbox, magic_enum::enum_name(category),
        [this, category](views::ui::InteractionEvent ie) {
          if (ie.type == views::ui::InteractionEvent::Type::Click &&
              ie.button == views::ui::MouseButton::Left) {
            current_category_ = category;
            mark_dirty();
          }
        });
  }

  draw_dialing_prompt(ctx);
}

void Terminal::tick() {
  assert(input_panel_);

  if (prompt_animator_->tick()) {
    mark_dirty();
  }

  // Handle auto-dialing (just like manually dialing the numbers in sequence)
  if (dialing_phase_ == DialingPhase::AUTO_DIALING) {
    if (auto_dial_timer_ > 0) {
      --auto_dial_timer_;
    }
    else if (auto_dial_index_ < pending_phone_number_.length()) {
      dial_number(pending_phone_number_[auto_dial_index_]);
      ++auto_dial_index_;
      auto_dial_timer_ = 6;
      mark_dirty();
    }
    else {
      // All digits dialed, transition to next phase
      // Clear the flag so manual dial code doesn't re-process this number
      fully_dialed_number_ = false;

      // Check if actor is refusing calls (angry from haggling, etc.)
      if (pending_actor_id_ && state_.is_refusing_calls(*pending_actor_id_)) {
        // Show "no connection" message
        forced_error_index_ = 1; // "no connection" is index 1
        transition_to_phase(DialingPhase::ERROR);
      }
      else {
        transition_to_phase(DialingPhase::WAITING_FOR_CONNECTION);
      }
    }
  }

  if (dialing_phase_ != DialingPhase::IDLE && phase_timer_ > 0) {
    if (--phase_timer_ == 0) {
      on_dialing_phase_finished();
    }
  }

  // Only process fully dialed number if we're in manual dialing mode (IDLE
  // phase) Auto-dialing (AUTO_DIALING phase) should go through the
  // WAITING/RINGING sequence because by definition they are dialing a correct
  // phone number
  if (fully_dialed_number_ && dialing_phase_ == DialingPhase::IDLE) {
    char number_string[PHONE_NUMBER_LENGTH + 1];
    for (int i = 0; i < PHONE_NUMBER_LENGTH; ++i) {
      // This should work even on EBCDIC :-)
      number_string[i] = dialed_number_[i];
    }
    number_string[PHONE_NUMBER_LENGTH] = '\0';

    // Store the phone number for later use
    pending_phone_number_ = number_string;

    // Find which actor was called
    bool dialed_existing_number = false;
    pending_actor_type_ = ActorType::NONE;
    pending_actor_id_.reset();

    uint32_t phone_num = static_cast<uint32_t>(std::atoi(number_string));
    for (const auto& [category, _] : category_to_string) {
      auto actors = actors_by_category(category);
      for (const auto* actor : actors) {
        if (actor->phone_number() == phone_num) {
          dialed_existing_number = true;
          pending_actor_type_ = actor->type();
          pending_actor_id_ = actor->id();

          // Track arms dealer as known if dialed
          if (actor->type() == ActorType::ARMS_DEALER) {
            auto* city = state_.active_city();
            auto* player = state_.active_player();
            auto player_id = player->id();

            if (!city->is_arms_dealer_known(player_id, actor->id())) {
              city->add_known_arms_dealer(player_id, actor->id());
            }
          }

          break;
        }
      }
      if (pending_actor_id_) {
        break;
      }
    }

    fully_dialed_number_ = false;
    reset_dialed_number();

    if (!dialed_existing_number) {
      // Keep keypad mode active so user can see what they dialed
      // standard_inputmenu() will be called when returning to IDLE later
      transition_to_phase(DialingPhase::ERROR);
    }
    else if (pending_actor_id_ &&
             state_.is_refusing_calls(*pending_actor_id_)) {
      forced_error_index_ = 1; // "no connection"
      transition_to_phase(DialingPhase::ERROR);
    }
    else {
      // Exit keypad mode for successful connection
      standard_inputmenu();
      // Start the connection sequence (skip AUTO_DIALING since we manually
      // dialed) State replacement will happen in CONNECTING phase
      transition_to_phase(DialingPhase::WAITING_FOR_CONNECTION);
    }
  }

  if (set_dial_submenu_) {
    keypad_menu();
    set_dial_submenu_ = false;
  }
}

void Terminal::draw_category(const RenderContext& ctx, TerminalCategory cat) {
  assert(input_panel_);
  clear_current_screen();

  std::string title = category_to_string.at(cat);
  context_.font->render_text_onto(*ctx.surface, title, ViewPoint{-18, 46},
                                  FontFace::PIZ_NUMB, highlight_color,
                                  AlignX::Center);

  // Constants for table rendering
  constexpr int CELL_PADDING = 6;
  constexpr int CELL_HEIGHT = 26;
  constexpr int CELL_WIDTH_NAME = 226;
  constexpr int CELL_WIDTH_NUMBER = 86;

  ViewPoint top_left = {44, 80};

  // Filter arms dealers in OTHERS category based on discovery
  std::vector<const Actor*> actors_to_display;
  if (cat == TerminalCategory::OTHERS) {
    auto* city = state_.active_city();
    auto* player = state_.active_player();
    auto player_id = player->id();

    // Initialize on first access if needed (for save game compatibility)
    uint8_t known_count = city->known_arms_dealer_count(player_id);
    if (known_count == 0) {
      city->initialize_known_arms_dealers(player_id);
    }

    auto known_arms_dealer_ids = city->known_arms_dealers(player_id);
    known_count = city->known_arms_dealer_count(player_id);

    // Get all actors in OTHERS category
    auto all_actors = actors_by_category(cat);

    // Special visibility rules for arms dealers only
    if (known_count < 3 || known_count >= 8) {
      // Show all actors (both mafia and arms dealers)
      actors_to_display = all_actors;
    }
    else {
      for (const auto* actor : all_actors) {
        if (actor->type() == ActorType::MAFIA) {
          // Mafia is always visible
          actors_to_display.push_back(actor);
        }
        else if (actor->type() == ActorType::ARMS_DEALER) {
          // Only show known arms dealers
          if (std::find(known_arms_dealer_ids.begin(),
                        known_arms_dealer_ids.end(),
                        actor->id()) != known_arms_dealer_ids.end()) {
            actors_to_display.push_back(actor);
          }
        }
      }
    }
  }
  else {
    // All other categories: show all actors
    actors_to_display = actors_by_category(cat);
  }

  for (const auto* actor : actors_to_display) {
    std::string name = actor->name();
    std::string phone_nr = std::to_string(actor->phone_number());
    ViewRect name_rect = {top_left.x, top_left.y, CELL_WIDTH_NAME, CELL_HEIGHT};

    ViewPoint bottom_right =
        top_left + PxOffset{CELL_WIDTH_NAME + CELL_WIDTH_NUMBER, CELL_HEIGHT};

    gfx::draw::box(*ctx.surface, top_left, bottom_right, highlight_color, 2);
    context_.font->render_text_onto(
        *ctx.surface, name,
        ViewPoint{top_left.x + CELL_PADDING, top_left.y + CELL_PADDING},
        FontFace::PIZ_NUMB, highlight_color);

    context_.font->render_text_onto(
        *ctx.surface, phone_nr,
        ViewPoint{top_left.x + CELL_WIDTH_NAME + CELL_PADDING + CELL_PADDING,
                  top_left.y + CELL_PADDING},
        FontFace::PIZ_NUMB, highlight_color);

    const auto cb = [this, phone_nr](views::ui::InteractionEvent ie) {
      if (ie.type == views::ui::InteractionEvent::Type::Click &&
          ie.button == views::ui::MouseButton::Left) {
        // Only allow clicking when idle (not during active dialing/connection)
        if (dialing_phase_ == DialingPhase::IDLE) {
          start_dialing_sequence(phone_nr);
        }
      }
    };
    ViewRect click_box{top_left.x, top_left.y,
                       CELL_WIDTH_NAME + CELL_WIDTH_NUMBER, CELL_HEIGHT};
    interactions_.create_frame_item(click_box, name, cb);

    // Increment y for the next row
    top_left.y += CELL_HEIGHT;
  }

  // Draw dialing prompt overlay if user is dialing
  draw_dialing_prompt(ctx);
}

void Terminal::draw_dialing_prompt(const RenderContext& ctx) {
  // Draw manual or auto-dialing prompt when user is dialing
  if (keypad_mode_) {
    int x = 76;
    int y = 330;
    context_.font->render_text_onto(*ctx.surface, "new number>",
                                    ViewPoint{x, y}, FontFace::PIZ_NUMB,
                                    prompt_color);
    x += 115;

    // Render each digit individually with pixel spacing
    constexpr int DIGIT_SPACING = 2;
    for (auto digit : dialed_number_) {
      if (digit == 0) {
        break;
      }
      auto metrics = context_.font->render_text_onto(
          *ctx.surface, std::string(1, digit), ViewPoint{x, y},
          FontFace::PIZ_NUMB, highlight_color);
      // TODO: We have TextStyle now that can do this
      x += metrics.width + DIGIT_SPACING;
    }
  }
}

void Terminal::render(const RenderContext& ctx) {
  interactions_.begin_frame();
  ctx.surface->fill(240);
  gfx::blit::blit_with_palette(*ctx.surface, context_.vga->get("gfx/back28"));

  if (dialing_phase_ != DialingPhase::ERROR &&
      dialing_phase_ != DialingPhase::SUCCESSFUL_CONNECTION) {
    switch (current_category_) {
    case TerminalCategory::REALTORS:
    case TerminalCategory::BANKS:
    case TerminalCategory::INSURANCE:
    case TerminalCategory::AUTHORITIES:
    case TerminalCategory::OTHERS:
      draw_category(ctx, current_category_);
      break;
    case TerminalCategory::NONE:
      draw_main_menu(ctx);
      break;
    default:
      fatal("Terminal render does not know this category.");
      break;
    }
  }

  // Render text animator if active or completed
  // (keep showing completed text until we explicitly clear it)
  if (prompt_animator_->is_animating() || prompt_animator_->is_complete()) {
    prompt_animator_->render(*ctx.surface);
  }
}

void Terminal::standard_inputmenu() {
  assert(input_panel_);
  keypad_mode_ = false; // Exit keypad mode
  input_panel_->delete_buttons();
  input_panel_->add_button(Icon::TELEPHONE, ButtonType::Horizontal,
                           ButtonPosition::BottomLeft, "Dial new number",
                           [this] { set_dial_submenu_ = true; });
  input_panel_->add_button(Icon::ARROW_BACK, ButtonType::Square,
                           ButtonPosition::BottomRight, "Back", [this] {
                             if (current_category_ == TerminalCategory::NONE) {
                               state_.ui().pop();
                             }
                             else {
                               current_category_ = TerminalCategory::NONE;
                               clear_current_screen();
                               mark_dirty();
                             }
                           });
}

void Terminal::keypad_menu() {
  assert(input_panel_);
  keypad_mode_ = true; // Enter keypad mode
  mark_dirty();
  input_panel_->delete_buttons();
  input_panel_->add_button(Icon::ONE, ButtonType::Square,
                           ButtonPosition::TopLeft, "Dial 1",
                           [this] { dial_number('1'); });
  input_panel_->add_button(Icon::TWO, ButtonType::Square,
                           ButtonPosition::TopCenter, "Dial 2",
                           [this] { dial_number('2'); });
  input_panel_->add_button(Icon::THREE, ButtonType::Square,
                           ButtonPosition::TopRight, "Dial 3",
                           [this] { dial_number('3'); });
  input_panel_->add_button(Icon::FOUR, ButtonType::Square,
                           ButtonPosition::MiddleLeft, "Dial 4",
                           [this] { dial_number('4'); });
  input_panel_->add_button(Icon::FIVE, ButtonType::Square,
                           ButtonPosition::MiddleCenter, "Dial 5",
                           [this] { dial_number('5'); });
  input_panel_->add_button(Icon::SIX, ButtonType::Square,
                           ButtonPosition::MiddleRight, "Dial 6",
                           [this] { dial_number('6'); });
  input_panel_->add_button(Icon::SEVEN, ButtonType::Square,
                           ButtonPosition::BottomLeft, "Dial 7",
                           [this] { dial_number('7'); });
  input_panel_->add_button(Icon::EIGHT, ButtonType::Square,
                           ButtonPosition::BottomCenter, "Dial 8",
                           [this] { dial_number('8'); });
  input_panel_->add_button(Icon::NINE, ButtonType::Square,
                           ButtonPosition::BottomRight, "Dial 9",
                           [this] { dial_number('9'); });
}

void Terminal::reset_dialed_number() {
  for (int i = 0; i < PHONE_NUMBER_LENGTH; ++i) {
    dialed_number_[i] = 0;
  }
}

void Terminal::dial_number(char nr) {
  // Only allow dialing during specific phases:
  // - IDLE: Manual dialing before connection
  // - AUTO_DIALING: Programmatic dialing when user clicked a phone entry
  // - ERROR_RINGING: User can start typing new number while error ringing
  // continues
  if (dialing_phase_ != DialingPhase::IDLE &&
      dialing_phase_ != DialingPhase::AUTO_DIALING &&
      dialing_phase_ != DialingPhase::ERROR_RINGING) {
    return; // Ignore input during connection/ringing phases
  }

  // Not the most efficient way, could keep a record of where we are, but
  // dialing a number is not that common:
  for (int i = 0; i < PHONE_NUMBER_LENGTH; ++i) {
    if (dialed_number_[i] == 0) {
      dialed_number_[i] = nr;
      if (i == PHONE_NUMBER_LENGTH - 1) {
        fully_dialed_number_ = true;
      }
      break;
    }
  }
  int nr_int = nr - '0';
  auto sound = static_cast<audio::SoundId>(
      static_cast<int>(audio::SoundId::DIAL_TONE_0) + nr_int);
  context_.audio->play_sound(sound);

  mark_dirty();
}

void Terminal::clear_current_screen() { mark_dirty(); }

std::vector<const Actor*>
Terminal::actors_by_category(TerminalCategory category) const {
  std::vector<const Actor*> actor_list;
  auto actor_types = category_to_actor_types.at(category);
  for (ActorType actor_type : actor_types) {
    // Get actors for this ActorType
    auto actors = state_.active_city()->actors(actor_type);
    actor_list.insert(actor_list.end(), actors.begin(), actors.end());
  }
  return actor_list;
}

void Terminal::start_dialing_sequence(const std::string& phone_number) {
  // Store phone number and look up actor
  pending_phone_number_ = phone_number;
  pending_actor_type_ = ActorType::NONE;
  pending_actor_id_ = -1;

  // Find which actor matches this phone number
  uint32_t phone_num = static_cast<uint32_t>(std::atoi(phone_number.c_str()));
  for (const auto& [category, _] : category_to_string) {
    auto actors = actors_by_category(category);
    for (const auto* actor : actors) {
      if (actor->phone_number() == phone_num) {
        pending_actor_type_ = actor->type();
        pending_actor_id_ = actor->id();
        break;
      }
    }
    if (pending_actor_id_ != -1) {
      break;
    }
  }

  // Enter keypad mode and start auto-dialing
  keypad_mode_ = true;
  reset_dialed_number();
  auto_dial_index_ = 0;
  auto_dial_timer_ = 8; // Initial delay before first digit
  transition_to_phase(DialingPhase::AUTO_DIALING);
  mark_dirty();
}

void Terminal::transition_to_phase(DialingPhase phase) {
  dialing_phase_ = phase;

  switch (phase) {
  case DialingPhase::SUCCESSFUL_CONNECTION:
    prompt_animator_->set_text("connection successful", {24, 330},
                               FontFace::PIZ_NUMB, prompt_color);
    prompt_animator_->set_char_delay(1);
    prompt_animator_->start();
    phase_timer_ = 30;
    break;

  case DialingPhase::IDLE:
    phase_timer_ = 0;
    prompt_animator_->clear();
    forced_error_index_ = -1; // Reset forced error message
    // Stop any playing sounds
    context_.audio->stop_sound(audio::SoundId::PHONE_RINGING);
    // Don't reset keypad mode here - user might be actively typing
    break;

  case DialingPhase::AUTO_DIALING:
    // Auto-dialing is handled in tick(), no setup needed here
    phase_timer_ = 0;
    break;

  case DialingPhase::WAITING_FOR_CONNECTION:
    // Exit keypad mode to hide the "new number>" prompt
    keypad_mode_ = false;
    setup_waiting_animation();
    // Start ring sound immediately when text starts appearing
    context_.audio->play_sound(audio::SoundId::PHONE_RINGING,
                               audio::LoopMode::Loop);
    // phase_timer_ is set by setup_waiting_animation()
    break;

  case DialingPhase::RINGING:
    // Ring sound already started in WAITING_FOR_CONNECTION, just continue
    // waiting
    phase_timer_ = 60; // ~1 second (reduced since ringing already started)
    break;

  case DialingPhase::PHONE_PICKUP:
    context_.audio->stop_sound(audio::SoundId::PHONE_RINGING);
    context_.audio->play_sound(audio::SoundId::PHONE_PICKUP);
    phase_timer_ = 20;
    break;

  case DialingPhase::CONNECTING:
    phase_timer_ = 10;
    break;

  case DialingPhase::CONNECTED:
    setup_connected_animation();
    break;

  case DialingPhase::ERROR:
    keypad_mode_ = false; // Exit keypad mode to show menu
    setup_error_animation();
    context_.audio->play_sound(audio::SoundId::PHONE_RINGING,
                               audio::LoopMode::Loop);
    break;

  case DialingPhase::ERROR_RINGING:
    prompt_animator_->clear();
    standard_inputmenu();
    phase_timer_ = 50;
    break;
  }

  mark_dirty();
}

void Terminal::on_dialing_phase_finished() {
  switch (dialing_phase_) {
  case DialingPhase::AUTO_DIALING:
    // Auto-dialing is handled in tick(), shouldn't reach here
    break;

  case DialingPhase::WAITING_FOR_CONNECTION:
    transition_to_phase(DialingPhase::RINGING);
    break;

  case DialingPhase::RINGING:
    transition_to_phase(DialingPhase::PHONE_PICKUP);
    break;

  case DialingPhase::SUCCESSFUL_CONNECTION:
    // Restore standard menu before returning to IDLE
    standard_inputmenu();
    transition_to_phase(DialingPhase::IDLE);
    break;

  case DialingPhase::PHONE_PICKUP:
    transition_to_phase(DialingPhase::CONNECTING);
    break;

  case DialingPhase::CONNECTING:
    // Push the actor terminal view
    if (pending_actor_id_) {
      state_.advance_time_blocks(1); // dialing cost
      // Transition to IDLE first to clean up sounds and state while we're still
      // active
      transition_to_phase(DialingPhase::IDLE);
      // Replace our state with the state they need to return to
      state_.ui().replace_top<TerminalState>(current_category_);
      // Now push the actor terminal view
      state_.ui().push<ActorTerminalState>(pending_actor_type_,
                                           *pending_actor_id_);
    }
    break;

  case DialingPhase::CONNECTED:
    transition_to_phase(DialingPhase::IDLE);
    break;

  case DialingPhase::ERROR:
    transition_to_phase(DialingPhase::ERROR_RINGING);
    break;

  case DialingPhase::ERROR_RINGING:
    transition_to_phase(DialingPhase::IDLE);
    break;

  case DialingPhase::IDLE:
    break;
  }
}

void Terminal::setup_waiting_animation() {
  std::string text = "waiting for connection...";

  prompt_animator_->set_text(text, ViewPoint{76, 330}, FontFace::PIZ_NUMB,
                             prompt_color);
  prompt_animator_->set_char_delay(1);
  prompt_animator_->start();

  phase_timer_ = text.length() + 15;
}

void Terminal::setup_connected_animation() {
  std::string text = "connection successful";

  prompt_animator_->set_text(text, ViewPoint{76, 330}, FontFace::PIZ_NUMB,
                             highlight_color);
  prompt_animator_->set_char_delay(1);
  prompt_animator_->start();

  phase_timer_ = text.length() + 15;
}

void Terminal::setup_error_animation() {
  // Error messages array matching the ones I've seen in the original
  const std::array<std::string, 3> error_messages = {
      "number unobtainable",
      "no connection",
      "line busy",
  };

  int message_index;

  // Check if we're forcing a specific error message (e.g., for refused calls)
  if (forced_error_index_ >= 0 &&
      forced_error_index_ < static_cast<int>(error_messages.size())) {
    message_index = forced_error_index_;
  }
  else {
    // How we decide which error message you get:
    auto phone_num =
        static_cast<uint32_t>(std::atoi(pending_phone_number_.c_str()));
    uint32_t shifted = phone_num >> 4;
    uint32_t remainder = shifted % 5;

    // Map remainder to message index
    if (remainder < error_messages.size()) {
      message_index = static_cast<int>(remainder);
    }
    else {
      message_index = 0;
    }
  }

  std::string text = error_messages[message_index];

  prompt_animator_->set_text(text, ViewPoint{76, 330}, FontFace::PIZ_NUMB,
                             prompt_color);
  prompt_animator_->set_char_delay(1);
  prompt_animator_->start();

  phase_timer_ = text.length() + 10;
}
