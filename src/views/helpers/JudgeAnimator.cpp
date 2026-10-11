/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/helpers/JudgeAnimator.h"
#include "engine/gfx/Blit.h"
#include "resources/FontManager.h"

JudgeAnimator::JudgeAnimator(GameContext& context) : context_(context) {}

void JudgeAnimator::start(const std::array<int, NUMBER_OF_JUDGES>& scores,
                          JudgingMode mode) {
  scores_ = scores;
  judging_mode_ = mode;
  animating_ = true;
  current_box_index_ = 0;

  // Calculate final positions and heights for each judge's score bar
  for (std::size_t i = 0; i < NUMBER_OF_JUDGES; i++) {
    int score = scores_[i];

    if (score < score_offset_) {
      box_heights_[i] = 52 + (score * 4);
    }
    else {
      box_heights_[i] = 72;
    }

    box_current_y_positions_[i] = table_offset_; // Start from table
    box_final_y_positions_[i] = table_offset_ - box_heights_[i] -
                                ((score - score_offset_) * (score > 5 ? 4 : 0));
    ticks_to_wait_after_finishing_[i] = (20 - score);
  }
}

bool JudgeAnimator::tick() {
  if (!animating_) {
    return false;
  }

  if (current_box_index_ < NUMBER_OF_JUDGES) {
    int i = current_box_index_;
    if (box_current_y_positions_[i] > box_final_y_positions_[i]) {
      box_current_y_positions_[i] -= 2;
      return true;
    }
    // Box has reached its final position
    if (ticks_to_wait_after_finishing_[i]-- > 0) {
      return false;
    }
    current_box_index_++;
    return true; // Need redraw for next box
  }

  if (current_box_index_ == NUMBER_OF_JUDGES) {
    animating_ = false;
  }

  return false;
}

void JudgeAnimator::render(const RenderContext& ctx) {
  gfx::blit::blit_with_palette(*ctx.surface, context_.vga->get("gfx/jury"));

  const int status_color = 26;
  std::string message = "evaluating your recipe...";
  if (judging_mode_ == JudgingMode::Standard) {
    message = "comparing with standard recipe...";
  }
  else if (judging_mode_ == JudgingMode::Competition) {
    message = "--- COMPETITION ---";
  }
  context_.font->render_text_onto(*ctx.surface, message, ViewPoint{60, 30},
                                  FontFace::PIZ_NUMB, status_color);

  constexpr int box_width = 74;
  constexpr std::array<uint8_t, NUMBER_OF_JUDGES - 1> room_between_boxes = {
      62, 46, 38, 46};

  int x = 0;
  for (int i = 0; i < NUMBER_OF_JUDGES; i++) {
    int score = scores_[i];
    int x_offset = (i < static_cast<int>(room_between_boxes.size()))
                       ? room_between_boxes[i] + box_width
                       : 0;

    int y;
    if (i < current_box_index_) {
      // Box is done animating
      y = box_final_y_positions_[i];
    }
    else if (i == current_box_index_) {
      // Box is being animated
      y = box_current_y_positions_[i];
    }
    else {
      // Box not yet animated
      x += x_offset;
      continue;
    }

    int height = box_heights_[i];
    // If the box is supposed to be partially behind the table,
    // limit the height to the part that is above the table.
    if (y + height > table_offset_) {
      height = table_offset_ - y;
    }

    // Draw the score box
    ViewRect box(x, y, box_width, height);
    const int box_color = 29;
    ctx.surface->fill_rect(box, box_color);

    // The brown stick protrudes into the sign a little bit
    constexpr int box_offset_into_card = 2;

    // If score > 5, draw the brown stick
    if (score > 5 &&
        y + box_heights_[i] - box_offset_into_card < table_offset_) {
      int stick_x = x + (box_width / 2) - 5; // Centered under the box
      int stick_y = y + height - box_offset_into_card;
      int stick_height = table_offset_ - stick_y;
      ViewRect stick(stick_x, stick_y, 10, stick_height);

      const int brown = 15;
      ctx.surface->fill_rect(stick, brown);
    }

    // If box is done animating, render the score
    if (y == box_final_y_positions_[i]) {
      // Center the text (shift more for double digits)
      int text_x = x + (box_width / 2) - (score > 9 ? 14 : 5);
      int text_y = y + 24;
      const int text_color = 115; // black
      context_.font->render_text_onto(*ctx.surface, std::to_string(score),
                                      ViewPoint{text_x, text_y},
                                      FontFace::ASTANDAR, text_color);
    }
    x += x_offset;
  }
}

void JudgeAnimator::skip_to_end() {
  for (int i = 0; i < NUMBER_OF_JUDGES; ++i) {
    box_current_y_positions_[i] = box_final_y_positions_[i];
  }
  current_box_index_ = NUMBER_OF_JUDGES;
  animating_ = false;
}
