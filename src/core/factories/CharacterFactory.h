/*
 * SPDX-FileCopyrightText: 2012 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>
#include <unordered_map>

#include "core/character/Character.h"

class CharacterFactory {
public:
  // id 0-99: original characters; id 100-199: doubles (reuse portrait from
  // id%100, names from T/DOUBLE.E, stats algorithmically tweaked)
  static core::character::CharacterData* create_character(
      int id, core::character::JobRole role = core::character::JobRole::CHEF);

private:
  static core::character::CharacterData* create_original(int id);
  static core::character::CharacterData*
  create_double(int id, core::character::JobRole role);
  static std::unordered_map<int,
                            std::unique_ptr<core::character::CharacterData>>
      cache_;
};
