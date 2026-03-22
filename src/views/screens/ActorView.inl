/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "engine/FormatCompat.h"
#include "magic_enum/magic_enum.hpp"
#include "resources/FontManager.h"

#include "core/util/LegacyConstants.h"
#include "gfx/Blit.h"
#include "views/base/ViewDefs.h"
#include "views/ui/PopupManager.h"
#include "views/ui/popups/ChequePopup.h"

constexpr int DEFAULT_CONVERSATION_ANIMATION_DURATION = 67;

// Two-nod sequence: down 2, down 4, up 2, up 0, down 2, down 4, up 2, up 0
constexpr int nod_y_offsets[] = {2, 4, 2, 0, 2, 4, 2, 0};
constexpr int NOD_NUM_FRAMES = 8;
constexpr int NOD_TICKS_PER_FRAME = 3;

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
      frozen_lines_.clear();
      current_phase_ = ConversationPhase::TALKING;
      actor_->state(ActionState::TALKING);
      phase_timer_ = DEFAULT_CONVERSATION_DELAY;

      break;
    }
    mark_dirty();
  }

  if (nod_frame_ != 0 && --nod_timer_ == 0) {
    nod_frame_++;
    if (nod_frame_ > NOD_NUM_FRAMES) {
      nod_frame_ = 0;
      frozen_lines_.clear();
      current_phase_ = ConversationPhase::TALKING;
      actor_->state(ActionState::TALKING);

      phase_timer_ = DEFAULT_CONVERSATION_DELAY;
    }
    else {
      nod_timer_ = NOD_TICKS_PER_FRAME;
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
    frozen_lines_.clear();
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
    if (conversation_->on_end_) {
      conversation_->on_end_();
    }
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
  if (nod_frame_ != 0) {
    return false;
  }
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
    case Emotion::NOD:
      already_emoted_ = true;
      break;
    }
  }
  return true;
}

template <typename LineEnum>
void ActorView<LineEnum>::render(const RenderContext& ctx) {
  auto& bg = context_.vga->get(bg_filename_);
  ctx.surface->fill(240);
  ctx.surface->palette(bg.palette());
  ctx.surface->blit_masked(bg);

  render_character_head(ctx);
  render_contract_terms(ctx);
  render_dialog(ctx);
}

template <typename LineEnum>
void ActorView<LineEnum>::render_character_head(const RenderContext& ctx) {
  auto character = actor_bob_.frame(actor_->state());
  auto eyes = actor_bob_.frame(BLINKING_EYE_STATE);

  // Get the indexes to our arrays:
  auto character_idx = actor_->head_id() - 1;
  auto bg_idx = actor_->background_id() - 10;

  int nod_offset = (nod_frame_ > 0 && nod_frame_ <= NOD_NUM_FRAMES)
                       ? nod_y_offsets[nod_frame_ - 1]
                       : 0;

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
  int y = y_1x * 2 + nod_offset;

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
  int text_x_pos = 0;
  int text_y_pos = 0;
  constexpr int dialog_box_offset_x = 15;
  constexpr int dialog_box_offset_y = 252;
  constexpr int y_offset = 20;

  auto lines = frozen_lines_.empty() ? conversation_->lines() : frozen_lines_;

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
          if (conversation_->is_terminal()) {
            current_phase_ = ConversationPhase::ENDING;
            phase_timer_ = 10;
          }
          else if (conversation_->is_proposal()) {
            // Only accept via click-through if easy_accept is set;
            // otherwise the player must use the thumbs-up button.
            auto current_lines = conversation_->lines();
            if (!current_lines.empty() && current_lines[0].is_easy_accept) {
              advance_proposal(true);
            }
          }
          else {
            advance_conversation();
          }
        }
      });

  constexpr int dialog_w = 345;
  constexpr int dialog_h = 104;
  int abs_x = dialog_box_offset_x - 2;
  int abs_y = dialog_box_offset_y - 4;

  for (auto line : lines) {
    int line_x = abs_x + text_x_pos;
    int line_y = abs_y + text_y_pos;

    if (static_cast<int>(line.line_id) == hovered_line_id_) {
      auto hover_metrics =
          context_.font->size_text(line.text, FontFace::PIZ_NUMB);
      ctx.surface->fill_rect(
          ViewRect{line_x - 2, line_y - 2, hover_metrics.width + 4,
                   hover_metrics.height + 4},
          13);
    }

    auto metrics = context_.font->render_into_rect(
        *ctx.surface, line.text,
        ViewRect{line_x, line_y, dialog_w - text_x_pos,
                 dialog_h - text_y_pos},
        FontFace::PIZ_NUMB, {.color = gfx::ColorIndex{15}});

    if (line.is_option) {
      interactions_.create_frame_item(
          ViewRect{line_x, line_y, metrics.width, metrics.height},
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
}

template <typename LineEnum> void ActorView<LineEnum>::advance_conversation() {
  auto previous_lines = conversation_->lines();
  bool state_changed = conversation_->advance();
  apply_emotion_if_needed(state_changed, std::move(previous_lines));
  mark_dirty();
}

template <typename LineEnum>
void ActorView<LineEnum>::advance_conversation(LineEnum choice) {
  auto previous_lines = conversation_->lines();
  bool state_changed = conversation_->advance_line_choice(choice);

  if (conversation_->popup_request() ==
      BaseConversation<LineEnum>::PopupRequest::CHEQUE) {
    context_.popup->palette_handle({37});
    context_.popup->origin(ViewPoint{LegacyConstants::WOOD_BORDER_WIDTH_LEFT,
                                     LegacyConstants::WOOD_BORDER_HEIGHT_TOP});
    context_.popup->create<ChequePopup>(
        [this](int amount, int account) {
          conversation_->on_cheque_result(amount, account);
          current_phase_ = ConversationPhase::TALKING;
          actor_->state(ActionState::TALKING);
          phase_timer_ = DEFAULT_CONVERSATION_DELAY;
          mark_dirty();
        });
    return;
  }

  apply_emotion_if_needed(state_changed, std::move(previous_lines));
  mark_dirty();
}

template <typename LineEnum>
void ActorView<LineEnum>::advance_proposal(bool accept) {
  auto previous_lines = conversation_->lines();
  auto state_changed = conversation_->advance_proposal(accept);
  apply_emotion_if_needed(state_changed, std::move(previous_lines));
  mark_dirty();
}

template <typename LineEnum>
void ActorView<LineEnum>::apply_emotion_if_needed(
    bool state_changed,
    std::vector<ConversationLine<LineEnum>> previous_lines) {
  auto emotion = conversation_->emotion();
  bool is_choice = conversation_->is_option();
  if (emotion != Emotion::NEUTRAL) {
    frozen_lines_ = std::move(previous_lines);
    phase_timer_ = DEFAULT_CONVERSATION_DELAY;
    current_phase_ = ConversationPhase::EMOTING;
    switch (emotion) {
    case Emotion::HAPPY:
      happy_stage_ = 1;
      happy_timer_ = happy_stage_1_delay;
      actor_->state(ActionState::HAPPY);
      current_phase_ = ConversationPhase::EMOTING;
      phase_timer_ = 100;
      break;
    case Emotion::ANGRY:
      actor_->state(ActionState::ANGRY);
      break;
    case Emotion::NOD:
      nod_frame_ = 1;
      nod_timer_ = NOD_TICKS_PER_FRAME;
      actor_->state(ActionState::WAITING);
      current_phase_ = ConversationPhase::EMOTING;
      phase_timer_ = 0; // nod_timer_ drives the animation
      break;
    default:
      break;
    }
  }
  else if (conversation_->has_ended() && conversation_->lines().empty()) {
    current_phase_ = ConversationPhase::ENDING;
    phase_timer_ = 1;
  }
  else if (!is_choice) {
    current_phase_ = ConversationPhase::TALKING;
    actor_->state(ActionState::TALKING);
    phase_timer_ = DEFAULT_CONVERSATION_DELAY;
  }
}
