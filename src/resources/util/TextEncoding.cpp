/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "resources/util/TextEncoding.h"
#include <utfcpp/utf8.h>
#include "engine/Fatal.h"

namespace text {

std::string iso8859_1_to_utf8(std::string_view input) {
  std::string result;
  // UTF-8 can be up to 2 bytes per char for Latin-1:
  result.reserve(input.size() * 2);

  for (unsigned char c : input) {
    if (c == '\0') {
      break;
    }

    if (c < 0x80) {
      // 0x00-0x7F are identical in ASCII and UTF-8.
      // These values already match UTF-8's single-byte form: 0xxxxxxx.
      result += static_cast<char>(c);
    }
    else {
      // Convert a Latin-1 byte (0x80-0xFF) into a 2-byte UTF-8 sequence.
      //
      // UTF-8 for this range looks like:
      //   110xxxxx 10xxxxxx
      //
      // We must copy the 8 bits of 'c' into those 5+6 payload bits.
      //
      // First byte:
      //   - Start with 11000000 (marks "this is a 2-byte UTF-8 char")
      //   - Insert the top bits of 'c' into the xxxxx field.
      result += static_cast<char>(0xC0 | (c >> 6));
      // Second byte:
      //   - Start with 10000000 (marks "continuation byte")
      //   - Insert the lower 6 bits of 'c'.
      result += static_cast<char>(0x80 | (c & 0x3F));
    }
  }

  return result;
}

std::string convert_platform_text(std::string_view input) {
  // Check if input is already valid UTF-8 by trying to validate it
  // utf8::find_invalid returns end() if the entire string is valid UTF-8
  if (utf8::find_invalid(input.begin(), input.end()) == input.end()) {
    return std::string(input);
  }
  // Both Amiga and DOS assets use ISO-8859-1 for extended characters
  return iso8859_1_to_utf8(input);
}

std::string read_iso8859_1_string(std::istream& is, std::size_t field_size) {
  std::string buf(field_size, '\0');
  if (!is.read(buf.data(), field_size)) {
    fatal("Unexpected EOF while reading string field");
  }

  auto end = buf.find('\0');
  if (end != std::string::npos) {
    buf.resize(end);
  }

  return iso8859_1_to_utf8(buf);
}

} // namespace text
