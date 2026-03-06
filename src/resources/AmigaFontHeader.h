/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <cstdint>

#pragma pack(push, 1)

// Amiga TextFont structure (partial, enough to read bitmap fonts)
struct AmigaTextFont {
  // Message structure (Node + 4 bytes)
  uint32_t tf_ln_succ;       // 0x00: successor pointer (unused)
  uint32_t tf_ln_pred;       // 0x04: predecessor pointer (unused)
  uint8_t tf_ln_type;        // 0x08: node type
  uint8_t tf_ln_pri;         // 0x09: priority
  uint32_t tf_ln_name;       // 0x0A: name pointer
  uint16_t tf_msg_length;    // 0x0E: message length
  uint32_t tf_msg_replyport; // 0x10: reply port

  // TextFont specific fields
  uint16_t tf_ysize;     // 0x14: character height
  uint8_t tf_style;      // 0x16: font style flags
  uint8_t tf_flags;      // 0x17: font flags
  uint16_t tf_xsize;     // 0x18: nominal character width
  uint16_t tf_baseline;  // 0x1A: baseline offset
  uint16_t tf_boldsmear; // 0x1C: boldface smear value
  uint16_t tf_accessors; // 0x1E: number of accessors
  uint8_t tf_lochar;     // 0x20: lowest character code
  uint8_t tf_hichar;     // 0x21: highest character code
  uint32_t tf_chardata;  // 0x22: pointer to character data (bitmap)
  uint16_t tf_modulo;    // 0x26: bytes per row in bitmap
  uint32_t tf_charloc;   // 0x28: pointer to character location table
  uint32_t tf_charspace; // 0x2C: pointer to character spacing table
  uint32_t tf_charkern;  // 0x30: pointer to character kerning table
};

static_assert(sizeof(AmigaTextFont) == 0x34, "AmigaTextFont size mismatch");

#pragma pack(pop)
