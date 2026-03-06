/*
 * SPDX-FileCopyrightText: 2012 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>

#include "core/character/Character.h"

class CharacterFactory {
public:
  static core::character::CharacterData* create_character(uint8_t character_id);

private:
  static std::unordered_map<uint8_t,
                            std::unique_ptr<core::character::CharacterData>>
      cache_;
};
