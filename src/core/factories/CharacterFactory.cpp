/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/factories/CharacterFactory.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "core/character/Character.h"
#include "core/city/JobMarket.h"
#include "engine/Fatal.h"
#include "engine/Logger.h"
#include "resources/util/FileUtils.h"

std::unordered_map<int, std::unique_ptr<core::character::CharacterData>>
    CharacterFactory::cache_ = {};

#pragma pack(push, 1)

struct CharacterFileDOS {
  // Header (5 bytes): first byte is the character index,
  uint8_t file_index;
  uint8_t index_offset;
  uint8_t header_padding[3];

  // Picture data (3068 bytes) - raw indexed pixels (59x52).
  uint8_t picture_data[3068];

  // First and last names (each 16 bytes).
  char first_name[16];
  char last_name[16];

  uint8_t unknown1_a[9];      // +0x1F .. +0x27
  uint8_t field_28;           // +0x28
  uint8_t unknown1_b[1];      // +0x29
  uint8_t pregnancy_possible; // 0x4C / 0x2A
  uint8_t unknown2[22];

  // Starting money (10 bytes read; first 8 bytes interpreted as a double).
  uint8_t starting_money_raw[10];

  // Pizzas per day (1 byte).
  uint8_t pizzas_per_day;

  uint8_t unknown3;
  uint8_t unknown4;
  uint8_t unknown5;
  uint8_t unknown6;
  uint8_t unknown7;

  uint8_t age;          // 0x52 / 0x30
  uint8_t popularity;   // 0x53 / 0x31
  uint8_t health;       // 0x54 / 0x32
  uint8_t energy;       // 0x55 / 0x33
  uint8_t intelligence; // 0x56 / 0x34
  uint8_t charisma;     // 0x57 / 0x35
  uint8_t dexterity;    // 0x58 / 0x36
  uint8_t personality;  // 0x59 / 0x37
  uint8_t courage;      // 0x5A / 0x38
  uint8_t experience;   // 0x5B / 0x39
  // Trustworthiness is reliability, not honesty (that's a hidden stat)
  uint8_t trustworthiness; // 0x5C / 0x3A
  uint8_t honesty;         // 0x5D / 0x3B
  uint8_t cooking;         // 0x5E / 0x3C
  uint8_t serving;         // 0x5F / 0x3D
  uint8_t management;      // 0x60 / 0x3E

  // Skip 2 unknown bytes. Fun fact, in the Amiga code we find references to
  // skills like 'selling', 'driving', 'IT skills' and 'irritability', could be
  // a left over of one of those?
  uint8_t unknown8[2];
  uint8_t motivation;
  uint8_t unknown9[4];
};

#pragma pack(pop)

template <typename CharFileType>
static void populate_character_data(core::character::CharacterData* c,
                                    const CharFileType& cf) {
  c->first_name =
      std::string(cf.first_name, strnlen(cf.first_name, sizeof(cf.first_name)));
  c->last_name =
      std::string(cf.last_name, strnlen(cf.last_name, sizeof(cf.last_name)));

  double money = 0.0;
  memcpy(&money, cf.starting_money_raw, sizeof(double));
  c->starting_money = Money{static_cast<int>(money * 100)};

  c->pizzas_per_day = cf.pizzas_per_day;
  c->age = cf.age;
  c->popularity = cf.popularity;
  c->health = cf.health;
  c->energy = cf.energy;
  c->intelligence = cf.intelligence;
  c->charisma = cf.charisma;
  c->dexterity = cf.dexterity;
  c->personality = cf.personality;
  c->courage = cf.courage;
  c->experience = cf.experience;
  c->trustworthiness = cf.trustworthiness;
  c->cooking = cf.cooking;
  c->serving = cf.serving;
  c->management = cf.management;
  c->motivation = cf.motivation;
}

core::character::CharacterData*
CharacterFactory::create_character(int id, core::character::JobRole role) {
  assert(id >= 0);
  auto it = cache_.find(id);
  if (it != cache_.end()) {
    return it->second.get();
  }
  if (id < core::character::JobMarket::ORIGINAL_SLOTS) {
    return create_original(id);
  }
  return create_double(id, role);
}

core::character::CharacterData* CharacterFactory::create_original(int id) {
  auto it = cache_.find(id);
  if (it != cache_.end()) {
    return it->second.get();
  }

  auto c = std::make_unique<core::character::CharacterData>();
  c->id = static_cast<uint8_t>(id);

  char filename[20];
  snprintf(filename, sizeof(filename), "chr/c%de.pc", id);

  std::ifstream is = fileutils::open(filename);
  CharacterFileDOS cf;
  is.read(reinterpret_cast<char*>(&cf), sizeof(cf));
  is.close();

  if (cf.file_index != id) {
    Logger::warn("File index {} does not match expected {}", cf.file_index, id);
  }

  c->picture_data(std::vector<unsigned char>(
      cf.picture_data, cf.picture_data + sizeof(cf.picture_data)));

  populate_character_data(c.get(), cf);

  auto* ptr = c.get();
  cache_[id] = std::move(c);
  return ptr;
}

core::character::CharacterData*
CharacterFactory::create_double(int id, core::character::JobRole role) {
  // slot is the 0-based index within all doubles
  // base_id is which original portrait/stats to reuse (wraps at
  // ORIGINAL_SLOTS).
  int slot = id - core::character::JobMarket::ORIGINAL_SLOTS;
  int base_id = slot % core::character::JobMarket::ORIGINAL_SLOTS;
  auto* base = create_original(base_id);

  auto c = std::make_unique<core::character::CharacterData>(*base);
  c->id = static_cast<uint8_t>(id);

  auto lines = fileutils::load_text_lines("t/double.e");
  int first_name_line = (slot * 2);
  int last_name_line = (slot * 2) + 1;
  if (last_name_line >= static_cast<int>(lines.size())) {
    fatal("DOUBLE.E does not have enough lines for character {}.", id);
  }
  c->first_name = lines[first_name_line];
  c->last_name = lines[last_name_line];
  if (c->first_name.empty() || c->first_name.size() < 2) {
    fatal("Double.e: First name too short.");
  }
  if (c->last_name.empty()) {
    fatal("Double.e: Last name is empty for character {}.", id);
  }

  // Salary tweak
  int salary_delta = (static_cast<unsigned char>(c->last_name[0]) * 2) - 156;
  int salary = static_cast<int>(c->starting_money.whole());
  salary = std::clamp(salary + salary_delta, 50, 350);
  c->starting_money = Money::from_dollars(salary);

  // Age tweak: c = clamp(last_name[1], 'a', 'l'); age += (c - 'a') / 4
  int age_char = std::clamp(static_cast<int>(c->last_name[1]),
                            static_cast<int>('a'), static_cast<int>('l'));
  c->age = static_cast<uint8_t>(c->age + ((age_char - 'a') / 4));

  // 13-stat tweaks derived from first name: cycle through chars 1..(len-1)
  // delta = (clamp(c, 'a', 's') - 'k') * 2;
  // If applying this delta pushes the stat outside 0..100, reverse the
  // delta, so e.g. value 10 with delta -15 would end up at -5,
  // so instead we bounce the other way so get +15 => 25. We then clamp this
  // result just in case.
  const std::array<uint8_t*, 13> stats = {
      &c->popularity, &c->health,          &c->energy,      &c->intelligence,
      &c->charisma,   &c->dexterity,       &c->personality, &c->courage,
      &c->experience, &c->trustworthiness, &c->honesty,     &c->cooking,
      &c->serving,
  };
  int name_len = static_cast<int>(c->first_name.size());
  for (int i = 0; i < 13; ++i) {
    int char_idx = (i % (name_len - 1)) + 1;
    int ch = std::clamp(static_cast<int>(c->first_name[char_idx]),
                        static_cast<int>('a'), static_cast<int>('s'));
    int delta = (ch - 'k') * 2;
    int val = *stats[i] + delta;
    if (val < 0 || val > 100) {
      delta = -delta; // bounce
    }
    *stats[i] = static_cast<uint8_t>(std::clamp(*stats[i] + delta, 0, 100));
  }

  // Job-type skill floor: boost job-type skill to at least 2 above the max of
  // all three job skills to ensure they are at least somewhat competent at
  // their job
  uint8_t max_skill = std::max({c->cooking, c->serving, c->management});
  uint8_t floor_val = static_cast<uint8_t>(std::min(max_skill + 2, 100));
  switch (role) {
  case core::character::JobRole::CHEF:
    c->cooking = std::max(c->cooking, floor_val);
    break;
  case core::character::JobRole::WAITER:
    c->serving = std::max(c->serving, floor_val);
    break;
  case core::character::JobRole::MANAGER:
    c->management = std::max(c->management, floor_val);
    break;
  default:
    break;
  }

  auto* ptr = c.get();
  cache_[id] = std::move(c);
  return ptr;
}
