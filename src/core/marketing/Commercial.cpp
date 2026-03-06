/*
 * SPDX-FileCopyrightText: 2012 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/marketing/Commercial.h"

Commercial::Commercial(int id, std::string title, std::string director,
                       int frame_count, PlayType type, std::string(filename))
    : id_(id), title_(title), director_(director), frame_count_(frame_count),
      current_frame_(0), type_(type), count_(0), filename_(filename) {
  if (type == STATIC) {
    current_frame_ = STATIC_START_FRAME;
  }
}

int Commercial::id() const { return id_; }

std::string Commercial::title() const { return title_; }

std::string Commercial::director() const { return director_; }

int Commercial::frame_count() const { return frame_count_; }

int Commercial::current_frame() const { return current_frame_; }

void Commercial::next_frame() {
  switch (type_) {
  case LOOP_START:
    if (count_ < LOOP_START_FRAME * LOOP_START_REPEATS &&
        current_frame_ >= LOOP_START_FRAME && (count_ % current_frame_) == 0 &&
        (count_ / current_frame_) < LOOP_START_REPEATS) {

      // Do -1 because there will be a ++current_frame_ later.
      current_frame_ = -1;
      count_--;
    }
    break;
  case REVERSE:
    if (current_frame_ > 0 && count_ + 1 >= frame_count_) {
      // Start reversing (-2 because of the ++ later on)
      current_frame_ -= 2;
    }
    else if (current_frame_ == 0 && count_ + 1 >= frame_count_) {
      // Done reversing, reset to begin state
      count_ = 0;
      current_frame_ = 0;
    }
    break;
  case LOOP_INNER:
    if (count_ < 8 && current_frame_ == LOOP_INNER_FRAME) {
      // Jump back one frame:
      current_frame_ -= 2;
    }
    if (count_ > 10 && count_ < 14) {
      // The last frame stays a bit longer:
      current_frame_--;
    }
    break;
  case LOOP_END:
    if (current_frame_ + 1 == frame_count_ && count_ < frame_count_) {
      current_frame_ = LOOP_END_SKIP_BACK - 1;
    }
    break;
  case PAUSE_END:
    if (count_ >= frame_count_ - 1 && count_ < frame_count_ + 4) {
      current_frame_--;
    }
    break;
  case STATIC:
    // Just toggle between frame 5 and 6
    if (count_ == 1) {
      current_frame_ = STATIC_START_FRAME - 1;
      count_ = -1;
    }
    break;
  case INTRO:
    if (current_frame_ + 1 > frame_count_) {
      count_ = -1;
      current_frame_ = -1;
    }
    break;
  case LOOP_COMPLETE:
    break;
  }

  if (++count_ > MAX_TOTAL_FRAMES) {
    count_ = 0;
  }
  if (++current_frame_ >= frame_count_) {
    current_frame_ = 0;
    count_ = 0;
  }
}

std::string Commercial::filename() const { return filename_; }
