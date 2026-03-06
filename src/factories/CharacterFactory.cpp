/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "factories/CharacterFactory.h"

#include <cassert>
#include <cstring>
#include <fstream>

#include "core/character/Character.h"
#include "engine/Logger.h"
#include "gfx/Surface.h"
#include "resources/BitplaneDecoder.h"

std::unordered_map<uint8_t, std::unique_ptr<core::character::CharacterData>>
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

  uint8_t unknown1[11];
  uint8_t pregancy_possible;
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

  uint8_t age;
  uint8_t popularity;
  uint8_t health;
  uint8_t energy;
  uint8_t intelligence;
  uint8_t charisma;
  uint8_t dexterity;
  uint8_t personality;
  uint8_t courage;
  uint8_t experience;
  // Trustworthiness is reliability, not honesty (that's a hidden stat)
  uint8_t trustworthiness;
  uint8_t honesty;
  uint8_t cooking;
  uint8_t serving;
  uint8_t management;

  // Skip 2 unknown bytes. Fun fact, in the Amiga code we find references to
  // skills like 'selling', 'driving', 'IT skills' and 'irritability', could be
  // a left over of one of those?
  uint8_t unknown8[2];
  uint8_t motivation;
  uint8_t unknown9[4];
};

// Amiga version structure
struct CharacterFileAmiga {
  // Header: first byte is the character index,
  uint8_t file_index;
  uint8_t index_offset;
  uint8_t header_padding[17]; // Extended header on Amiga (19 total - 2 = 17)

  // Bitplane picture data (1239 bytes starting at byte 19)
  // This is 52x59 pixels with 3 bitplanes (7 bytes/line x 59 lines x 3 planes =
  // 1239).
  uint8_t picture_data[1239];

  // 8 bytes of padding/unknown data
  uint8_t padding_after_image[8];

  // Name starts at byte 1266 = 19 + 1239 + 8
  // First and last names (each 16 bytes).
  char first_name[16];
  char last_name[16];

  uint8_t unknown1[11];
  uint8_t pregancy_possible;
  uint8_t unknown2[22];

  // Starting money (10 bytes read; first 8 bytes interpreted as a double).
  uint8_t starting_money_raw[10];

  uint8_t pizzas_per_day;
  uint8_t unknown3;
  uint8_t unknown4;
  uint8_t unknown5;
  uint8_t unknown6;
  uint8_t unknown7;
  uint8_t age;
  uint8_t popularity;
  uint8_t health;
  uint8_t energy;
  uint8_t intelligence;
  uint8_t charisma;
  uint8_t dexterity;
  uint8_t personality;
  uint8_t courage;
  uint8_t experience;
  uint8_t trustworthiness;
  uint8_t honesty;
  uint8_t cooking;
  uint8_t serving;
  uint8_t management;
  uint8_t unknown8[2];
  uint8_t motivation;
  uint8_t unknown9[4];
};
#pragma pack(pop)

template <typename CharFileType>
static void populate_character_data(core::character::CharacterData* c,
                                    const CharFileType& cf,
                                    bool skip_names = false) {
  if (!skip_names) {
    c->first_name = std::string(cf.first_name,
                                strnlen(cf.first_name, sizeof(cf.first_name)));
    c->last_name =
        std::string(cf.last_name, strnlen(cf.last_name, sizeof(cf.last_name)));
  }

  // Interpret the first 8 bytes of starting_money_raw as a double (big-endian
  // on Amiga)
  double money = 0.0;
  if (platform::is_amiga()) {
    // Byte-swap for big-endian double
    uint8_t swapped[8];
    for (int i = 0; i < 8; ++i) {
      swapped[i] = cf.starting_money_raw[7 - i];
    }
    memcpy(&money, swapped, sizeof(double));
  }
  else {
    memcpy(&money, cf.starting_money_raw, sizeof(double));
  }
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

  Logger::debug(
      "{} {} has piz/day {} pregnancy: {} and unknowns: {}, {}, {}, {}, {}",
      cf.first_name, cf.last_name, cf.pizzas_per_day, cf.pregancy_possible,
      cf.unknown3, cf.unknown4, cf.unknown5, cf.unknown6, cf.unknown6);
}

core::character::CharacterData*
CharacterFactory::create_character(uint8_t character_id) {

  assert(character_id >= 0 && character_id < 100);
  // Check if the character is already in the cache
  auto it = cache_.find(character_id);
  if (it != cache_.end()) {
    return it->second.get();
  }

  static_assert(NUMBER_OF_CHARACTERS <= 100,
                "filename[] too small for >99 characters");

  auto c = std::make_unique<core::character::CharacterData>();
  c->id = character_id;

  char filename[20];
  snprintf(filename, sizeof(filename), "chr/c%d", character_id);

  if (platform::is_amiga()) {
    std::ifstream is = fileutils::open(filename);
    Logger::debug("Loading Amiga character file: {}", filename);

    CharacterFileAmiga cf;
    is.read(reinterpret_cast<char*>(&cf), sizeof(cf));
    is.close();

    if (cf.file_index != character_id) {
      Logger::warn("File index {} does not match expected {}", cf.file_index,
                   character_id);
    }

    // Store names as-is; Font rendering will convert encoding if needed
    c->first_name = std::string(cf.first_name,
                                strnlen(cf.first_name, sizeof(cf.first_name)));
    c->last_name =
        std::string(cf.last_name, strnlen(cf.last_name, sizeof(cf.last_name)));

    // Amiga character image dimensions (different from DOS 52x59)
    const int width = 51;
    const int height = 58;
    const int num_bitplanes = 3;

    // For debugging trying to find the right offset:
    const int header_offset = 0;
    const uint8_t palette_index = 63;

    // Calculate bytes per line: 51 pixels / 8 = 6.375, round up to 7
    int bytes_per_line = (width + 7) / 8;          // = 7
    int bytes_per_plane = bytes_per_line * height; // 7 * 58 = 406

    // Calculate actual data needed after header
    int bytes_after_header = sizeof(cf.picture_data) - header_offset;

    // Character files use the same 14-byte padding as standalone BOB files
    const int bitplane_padding = 14;

    // Extract bitplane data (skip header)
    std::vector<uint8_t> bitplane_data(cf.picture_data + header_offset,
                                       cf.picture_data +
                                           sizeof(cf.picture_data));

    // Decode the bitplane data WITHOUT scaling (scale=1)
    auto surface =
        BitplaneDecoder::decode(bitplane_data, width, height, num_bitplanes,
                                bytes_per_line, palette_index, /*scale=*/1,
                                bitplane_padding, /*bitplane_size_extra=*/0);

    // printf("Surface created: %dx%d\n", surface.size().w, surface.size().h);
    // printf("Surface pixel count: %zu\n", surface.pixels().size());

    // Convert surface to vector
    auto pixels_span = surface.pixels();
    std::vector<unsigned char> pixels_vec(pixels_span.begin(),
                                          pixels_span.end());
    // printf("Converted to vector: %zu pixels (52x59)\n", pixels_vec.size());

    // Remap character pixels: Amiga characters use indices 0-7 expecting
    // grayscale, but palette 38 has grayscale at indices 8-15. Add 8 to remap.
    for (auto& pixel : pixels_vec) {
      pixel += 8;
    }

    // Store the decoded pixel data and dimensions
    c->picture_data(pixels_vec);
    c->set_picture_dimensions(width, height);

    // Populate character data, but skip names since we already converted them
    // to UTF-8
    populate_character_data(c.get(), cf, true);
  }
  else {
    // Fall back to DOS format (.pc extension)
    snprintf(filename, sizeof(filename), "chr/c%de.pc", character_id);

    std::ifstream is = fileutils::open(filename);
    CharacterFileDOS cf;
    is.read(reinterpret_cast<char*>(&cf), sizeof(cf));
    is.close();

    if (cf.file_index != character_id) {
      Logger::warn("File index {} does not match expected {}", cf.file_index,
                   character_id);
    }

    // DOS format has raw indexed pixel data
    c->picture_data(std::vector<unsigned char>(
        cf.picture_data, cf.picture_data + sizeof(cf.picture_data)));

    populate_character_data(c.get(), cf);
  }

  auto* char_ptr = c.get();
  cache_[character_id] = std::move(c);
  return char_ptr;
}
