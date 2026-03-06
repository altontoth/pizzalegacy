/*
 * SPDX-FileCopyrightText: 2012 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>

// Fall-back to make sure frame count doesn't get too high:
#define MAX_TOTAL_FRAMES 100
// For LOOP_START, at which frame to jump back to beginning:
#define LOOP_START_FRAME 4
// And the amount of loops in LOOP_START:
#define LOOP_START_REPEATS 3
// For LOOP_END, to which frame we jump back when we reach the end:
#define LOOP_END_SKIP_BACK 5
// For LOOP_INNER, at which frame to go back to previous frame:
#define LOOP_INNER_FRAME 5
// For STATIC
#define STATIC_START_FRAME 5

class Commercial {
public:
  enum PlayType {
    LOOP_COMPLETE,
    LOOP_START,
    LOOP_INNER,
    LOOP_END,
    REVERSE,
    PAUSE_END,
    STATIC,
    INTRO,
  };

  Commercial(int id, std::string title, std::string director, int frame_count,
             PlayType type, std::string filename);
  int id() const;
  std::string title() const;
  std::string director() const;
  int frame_count() const;
  int current_frame() const;
  void next_frame();
  PlayType type() const;
  std::string filename() const;

private:
  int id_;
  std::string title_;
  std::string director_;
  int frame_count_;
  int current_frame_;
  PlayType type_;
  int count_;
  std::string filename_;
};
