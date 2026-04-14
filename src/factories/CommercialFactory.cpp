/*
 * SPDX-FileCopyrightText: 2012 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "factories/CommercialFactory.h"
#include <string>
#include "engine/Fatal.h"

Commercial CommercialFactory::create(int commercial_id) {
  std::string filename;
  Commercial::PlayType type = Commercial::LOOP_COMPLETE;
  int number_of_frames = 10;

  std::string title;
  std::string director;

  switch (commercial_id) {
  case 0:
    filename = "gfx/spot0";
    title = "Monster Movie";
    director = "Steven Spielking";
    type = Commercial::LOOP_START;
    break;
  case 1:
    filename = "gfx/spot1";
    title = "Food Monster";
    director = "Alfred Hitchhiker";
    break;
  case 2:
    filename = "gfx/spot2";
    title = "Eat and Let Die";
    director = "Dickie Lovey";
    type = Commercial::LOOP_INNER;
    number_of_frames = 8;
    break;
  case 3:
    filename = "gfx/spot3";
    title = "Lemming";
    director = "Klint Ostwood";
    type = Commercial::LOOP_END;
    break;
  case 4:
    filename = "gfx/spot4";
    title = "Falling Up";
    director = "Quentin Chaneytino";
    type = Commercial::REVERSE;
    break;
  case 5:
    filename = "gfx/spot5";
    title = "Farce";
    director = "Steven Spielking";
    break;
  case 6:
    filename = "gfx/spot6";
    title = "Go Shopping";
    director = "Alfred Hitchhiker";
    number_of_frames = 9;
    break;
  case 7:
    filename = "gfx/spot7";
    title = "Paradise";
    director = "Dickie Lovey";
    break;
  case 8:
    filename = "gfx/spot8";
    title = "Fiendlishly good";
    director = "Klint Ostwood";
    type = Commercial::PAUSE_END;
    break;
  case 9:
    filename = "gfx/tvmisc";
    // Not the actual number of frames, but logic further on assumes that
    // the frame number is never higher than the number of frames, but since
    // we start at a middle frame here, that doesn't work.
    number_of_frames = 10;
    type = Commercial::STATIC;
    break;
  case 10:
    filename = "gfx/tvmisc";
    number_of_frames = 5;
    break;
  default:
    fatal("Invalid commercial id specified");
  }

  return Commercial(commercial_id, title, director, number_of_frames, type,
                    filename);
}
