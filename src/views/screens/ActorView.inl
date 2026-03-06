/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/FormatCompat.h"
#include "magic_enum/magic_enum.hpp"
#include "resources/FontManager.h"

#include "gfx/Blit.h"
#include "views/base/ViewDefs.h"

constexpr int DEFAULT_CONVERSATION_ANIMATION_DURATION = 67;

const uint8_t eye_x_offsets[] = {0x09, 0x0A, 0x02, 0x0B, 0x05, 0x01, 0x09,
                                 0x00, 0x17, 0x0A, 0x04, 0x16, 0x0A, 0x0A,
                                 0x07, 0x04, 0x05, 0x08, 0x07, 0x0B, 0x09,
                                 0x0D, 0x17, 0x0D, 0x09};

const uint8_t eye_y_offsets[] = {0x19, 0x06, 0x0D, 0x14, 0x15, 0x0B, 0x18,
                                 0x1B, 0x1B, 0x29, 0x13, 0x20, 0x17, 0x12,
                                 0x13, 0x14, 0x20, 0x1C, 0x12, 0x10, 0x15,
                                 0x1B, 0x13, 0x18, 0x14};

const uint8_t character_y_adjustments[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x06, 0x0A,
    0x0E, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0A, 0x07, 0x00, 0x00};

const uint8_t head_x_positions[] = {0x31, 0x4C, 0x35, 0x2F, 0x64, 0x3B,
                                    0x5D, 0x2E, 0x5E, 0x9A, 0x48, 0x64,
                                    0x76, 0x64, 0x64, 0x64};

const uint8_t head_y_positions[] = {0x48, 0x43, 0x4A, 0x50, 0x64, 0x46,
                                    0x46, 0x42, 0x44, 0x40, 0x58, 0x64,
                                    0x39, 0x64, 0x64, 0x64};

template <typename LineEnum>
ActorView<LineEnum>::ActorView(
    GameContext& context, GameState& state, Actor* actor,
    std::unique_ptr<BaseConversation<LineEnum>> conversation)
    : MainPanelView(context, state), actor_(actor),
      conversation_(std::move(conversation)),
      actor_bob_(context_.bob->get(actor->bob_filename())),
      bg_filename_(compat::format("gfx/back{}", actor_->background_id())) {

  actor_->state(ActionState::TALKING);

  phase_timer_ = DEFAULT_CONVERSATION_ANIMATION_DURATION;

  input_panel_->add_button(Icon::ARROW_BACK, ButtonType::Square,
                           ButtonPosition::BottomRight, "Back",
                           [&state] { state.ui().pop(); });

  input_panel_->add_button(Icon::THUMBS_UP, ButtonType::Horizontal,
                           ButtonPosition::BottomLeft, "Agree",
                           [this] { advance_proposal(true); });

  input_panel_->add_button(Icon::THUMBS_DOWN, ButtonType::Vertical,
                           ButtonPosition::TopRight, "Reject",
                           [this] { advance_proposal(false); });
}

template <typename LineEnum> void ActorView<LineEnum>::tick() {

  if (happy_stage_ != 0 && --happy_timer_ == 0) {
    switch (happy_stage_) {
    case 1: // switch to Neutral
      actor_->state(ActionState::WAITING);
      happy_timer_ = happy_stage_2_delay;
      happy_stage_ = 2;
      break;

    case 2: // back to Happy
      actor_->state(ActionState::HAPPY);
      happy_timer_ = happy_stage_3_delay;
      happy_stage_ = 3;
      break;

    case 3: // done, resume dialogue
      happy_stage_ = 0;
      current_phase_ = ConversationPhase::TALKING;
      actor_->state(ActionState::TALKING);
      phase_timer_ = DEFAULT_CONVERSATION_DELAY;
      stop_rendering_dialog = false;
      break;
    }
    mark_dirty();
  }

  if (phase_timer_ > 0) {
    if (--phase_timer_ == 0) {
      on_phase_finished();
    }
  }

  bool should_mark_dirty = update_idle_animations();
  should_mark_dirty |= update_face_animations();
  should_mark_dirty |= update_conversation();

  count_++;
  if (should_mark_dirty) {
    mark_dirty();
  }
}

template <typename LineEnum> void ActorView<LineEnum>::on_phase_finished() {
  Logger::debug("phase finished");
  switch (current_phase_) {
  case ConversationPhase::EMOTING:
    // We are done emoting -> Show the next line (or next conversation logic).
    stop_rendering_dialog = false;
    current_phase_ = ConversationPhase::TALKING;
    actor_->state(ActionState::TALKING);
    mark_dirty();
    phase_timer_ = DEFAULT_CONVERSATION_ANIMATION_DURATION;
    break;
  case ConversationPhase::TALKING:
    phase_timer_ = 0;
    if (conversation_->has_ended()) {
      current_phase_ = ConversationPhase::ENDING;
      phase_timer_ = 10;
    }
    else {
      current_phase_ = ConversationPhase::WAITING;
      actor_->state(ActionState::WAITING);
    }
    mark_dirty();
    break;
  case ConversationPhase::ENDING:
    Logger::debug("Conversation over!");
    state_.ui().pop();
    break;
  default:
    break;
  }
}

template <typename LineEnum> bool ActorView<LineEnum>::update_conversation() {
  if (conversation_delay_ > 0) {
    conversation_delay_--;
    return false;
  }
  // No changes in conversation
  return false;
}

template <typename LineEnum>
bool ActorView<LineEnum>::update_face_animations() {
  if (current_phase_ == ConversationPhase::TALKING && count_ % 4 == 0) {
    return actor_->next_state();
  }
  return false;
}

template <typename LineEnum>
bool ActorView<LineEnum>::update_idle_animations() {
  int blink_for = rand() % 3 + 3;
  if (count_ % 5 == 0) {
    if (rand() % 35 > 32) {
      actor_->blinking(true);
      last_blink_at_ = count_;
      return true;
    }
  }
  if (actor_->is_blinking() && count_ > (last_blink_at_ + blink_for)) {
    actor_->blinking(false);
    return true;
  }

  return false;
}

template <typename LineEnum> bool ActorView<LineEnum>::update_emotions() {
  auto emotion = conversation_->emotion();
  if (!already_emoted_) {
    switch (emotion) {
    case Emotion::NEUTRAL:
      actor_->state(ActionState::WAITING);
      already_emoted_ = true;
      break;
    case Emotion::ANGRY:
      actor_->state(ActionState::ANGRY);
      already_emoted_ = true;
      break;
    case Emotion::HAPPY:
      actor_->state(ActionState::HAPPY);
      already_emoted_ = true;
      break;
    }
  }
  return true;
}

template <typename LineEnum>
void ActorView<LineEnum>::render(const RenderContext& ctx) {
  ctx.surface->fill(240);
  gfx::blit::blit_raw_with_palette(*ctx.surface,
                                   context_.vga->get(bg_filename_));

  render_character_head(ctx);
  render_contract_terms(ctx);
  if (!stop_rendering_dialog) {
    render_dialog(ctx);
  }
}

template <typename LineEnum>
void ActorView<LineEnum>::render_character_head(const RenderContext& ctx) {
  auto character = actor_bob_.frame(actor_->state());
  auto eyes = actor_bob_.frame(BLINKING_EYE_STATE);

  // Get the indexes to our arrays:
  auto character_idx = actor_->head_id() - 1;
  auto bg_idx = actor_->background_id() - 10;

  if (actor_->is_blinking()) {
    int eye_x = eye_x_offsets[character_idx] * 2;
    int eye_y = eye_y_offsets[character_idx] * 2;
    character.blit_masked(eyes, ViewPoint{eye_x, eye_y});
  }

  // Get sprite dimensions at 1x
  int sprite_width_1x = character.width() / 2;
  int sprite_height_1x = character.height() / 2;

  // Calculate base position
  int x_1x = head_x_positions[bg_idx] - (sprite_width_1x / 2);
  int y_1x = head_y_positions[bg_idx] - sprite_height_1x;
  y_1x += character_y_adjustments[character_idx];

  int x = x_1x * 2;
  int y = y_1x * 2;

  ctx.surface->blit_masked(character, ViewPoint{x, y});
}

template <typename LineEnum>
void ActorView<LineEnum>::render_contract_terms(const RenderContext& ctx) {
  auto terms = conversation_->contract_terms();
  if (!terms) {
    return;
  }

  constexpr int box_x = 208;
  constexpr int box_y = 24;
  constexpr int box_w = 150;
  constexpr int line_h = 22;
  constexpr int box_h = 120;
  constexpr int text_x = 4;
  constexpr int text_y = 2;
  constexpr int val_x = 148;

  ctx.surface->fill_rect(ViewRect{box_x, box_y, box_w, box_h}, 240);

  Surface s(box_w, box_h, PaletteHandle{0}, 0);

  auto render_row = [&](int row, const std::string& label,
                        const std::string& value) {
    int y = text_y + row * line_h;
    context_.font->render_text_onto(s, label, ViewPoint{text_x, y},
                                    FontFace::PIZ_NUMB, 15, AlignX::Left);
    context_.font->render_text_onto(s, value, ViewPoint{val_x, y},
                                    FontFace::PIZ_NUMB, 15, AlignX::RightEdge);
  };

  render_row(0, "sum:", compat::format("{:w}", terms->sum));
  render_row(1, "discount:", compat::format("{} %", terms->discount_pct));
  render_row(2, "charge:", compat::format("{:w}", terms->charge));
  render_row(3, "penalty:", compat::format("{:w}", terms->penalty));
  render_row(4, "term:", compat::format("{} days  ", terms->term_days));

  ctx.surface->blit_masked(s, ViewPoint{box_x, box_y});
}

template <typename LineEnum>
void ActorView<LineEnum>::render_dialog(const RenderContext& ctx) {
  interactions_.begin_frame();
  // Dialog rendering:
  auto d = conversation_->lines();
  int text_x_pos = 0;
  int text_y_pos = 0;
  constexpr int dialog_box_offset_x = 15;
  constexpr int dialog_box_offset_y = 256;
  constexpr int y_offset = 20;

  auto lines = conversation_->lines();

  // Before we start, we add an interaction item over the whole screen,
  // to catch any clicks that didn't hit any of our actual hit points. This
  // is for advancing through clicks, if allowed. Since items are iterated
  // in reverse order this one is checked last.
  interactions_.create_frame_item(
      ViewRect{0, 0, views::base::MAIN_PANEL_WIDTH,
               views::base::MAIN_PANEL_HEIGHT},
      "dialog_catch_all", [this](views::ui::InteractionEvent ie) {
        if (ie.type == views::ui::InteractionEvent::Type::Click) {
          Logger::debug("catch all advance conversation with a click at {}, {}",
                        ie.pos.x, ie.pos.y);
          // We have to check if they clicked outside of the buttons
          // on a proposal, in which case they accepted the proposal:
          // TODO: Some proposals don't allow you to accept by clicking through,
          // so we need some way to differentiate.
          if (conversation_->is_terminal()) {
            current_phase_ = ConversationPhase::ENDING;
            phase_timer_ = 10;
          }
          else if (conversation_->is_proposal()) {
            advance_proposal(true);
          }
          else {
            advance_conversation();
          }
        }
      });

  static constexpr char LINE_FMT[] = "{}.line.{}";

  // Create a dedicated surface for this so that
  // the line width is done correctly.
  Surface s(345, 100, PaletteHandle{0}, 240);
  for (auto line : lines) {
    int w, h;
    auto text_color = 15;
    if (static_cast<int>(line.line_id) == hovered_line_id_) {
      // Draw background rectangle
      auto hover_metrics =
          context_.font->size_text(line.text, FontFace::PIZ_NUMB);
      s.fill_rect(ViewRect{text_x_pos - 2, text_y_pos - 2,
                           hover_metrics.width + 4, hover_metrics.height + 4},
                  13);
    }
    auto metrics = context_.font->render_text_onto(
        s, line.text, ViewPoint{text_x_pos, text_y_pos}, FontFace::PIZ_NUMB,
        text_color, AlignX::Left);

    if (line.is_option) {
      interactions_.create_frame_item(
          ViewRect{dialog_box_offset_x + text_x_pos,
                   dialog_box_offset_y + text_y_pos, metrics.width,
                   metrics.height},
          compat::format("{}.line.{}", magic_enum::enum_name(actor_->type()),
                         magic_enum::enum_name(line.line_id)),
          [this, line](views::ui::InteractionEvent ie) {
            if (ie.type == views::ui::InteractionEvent::Type::Click) {
              advance_conversation(line.line_id);
            }
            else if (ie.type == views::ui::InteractionEvent::Type::Enter) {
              hovered_line_id_ = static_cast<int>(line.line_id);
              mark_dirty();
            }
            else if (ie.type == views::ui::InteractionEvent::Type::Leave) {
              hovered_line_id_ = -1;
              mark_dirty();
            }
          });
    }
    text_y_pos += y_offset;
  }
  ctx.surface->blit_masked(s,
                           ViewPoint{dialog_box_offset_x, dialog_box_offset_y});
}

template <typename LineEnum> void ActorView<LineEnum>::advance_conversation() {
  bool state_changed = conversation_->advance();
  apply_emotion_if_needed(state_changed);
}

template <typename LineEnum>
void ActorView<LineEnum>::advance_conversation(LineEnum choice) {
  bool state_changed = conversation_->advance_line_choice(choice);
  apply_emotion_if_needed(state_changed);
}

template <typename LineEnum>
void ActorView<LineEnum>::advance_proposal(bool accept) {
  auto state_changed = conversation_->advance_proposal(accept);
  apply_emotion_if_needed(state_changed);
  mark_dirty();
}

template <typename LineEnum>
void ActorView<LineEnum>::apply_emotion_if_needed(bool state_changed) {
  auto emotion = conversation_->emotion();
  bool is_choice = conversation_->is_option();
  if (emotion != Emotion::NEUTRAL) {
    phase_timer_ = DEFAULT_CONVERSATION_DELAY;
    current_phase_ = ConversationPhase::EMOTING;
    switch (emotion) {
    case Emotion::HAPPY:
      happy_stage_ = 1;
      happy_timer_ = happy_stage_1_delay;
      actor_->state(ActionState::HAPPY);
      stop_rendering_dialog = true;
      current_phase_ = ConversationPhase::EMOTING;
      phase_timer_ = 100;
      break;
      break;
    case Emotion::ANGRY:
      actor_->state(ActionState::ANGRY);
      stop_rendering_dialog = true;
      break;
    default:
      break;
    }
  }
  else if (!is_choice) {
    current_phase_ = ConversationPhase::TALKING;
    actor_->state(ActionState::TALKING);
    phase_timer_ = DEFAULT_CONVERSATION_DELAY;
  }
}
