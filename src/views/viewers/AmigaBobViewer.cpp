/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/viewers/AmigaBobViewer.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <map>
#include "engine/input/InputManager.h"
#include "resources/BitplaneDecoder.h"
#include "views/ui/popups/TextInputPopup.h"

AmigaBobViewer::AmigaBobViewer(GameContext& context, GameState& state,
                               std::string_view filename)
    : View(context, state), palette_(63), filename_(filename),
      num_bitplanes_(3), header_offset_(19), width_(51), height_(58),
      palette_offset_(0), bitplane_padding_(14), bytes_per_line_override_(-1),
      bitplane_size_extra_(0),
      listener_(context.input->register_listener(*this)) {}

bool AmigaBobViewer::on_action(const input::ActionEvent& ev) {
  if (!ev.state.pressed) {
    return false;
  }
  switch (ev.action) {
  case input::Action::CycleForward:
    // Increase number of bitplanes (1-8)
    if (++num_bitplanes_ > 8) {
      num_bitplanes_ = 1;
    }
    Logger::info("Number of bitplanes: {}", num_bitplanes_);
    mark_dirty();
    break;
  case input::Action::CycleBackward:
    // Decrease number of bitplanes (1-8)
    if (--num_bitplanes_ < 1) {
      num_bitplanes_ = 8;
    }
    Logger::info("Number of bitplanes: {}", num_bitplanes_);
    mark_dirty();
    break;
  case input::Action::CycleUp:
    if (palette_ == 0) {
      ;
      palette_ = 67;
    }
    else {
      palette_--;
    }
    Logger::info("Palette : {}", palette_);
    mark_dirty();
    break;
  case input::Action::CycleDown:
    // Increase palette
    palette_++;
    Logger::info("Palette : {}", palette_);
    mark_dirty();
    break;
  case input::Action::MoveItemLeft:
    // Decrease palette offset
    palette_offset_--;
    palette_offset_ = std::max(palette_offset_, -255);
    Logger::info("Palette offset: {} (indices shifted by {})", palette_offset_,
                 palette_offset_);
    mark_dirty();
    break;
  case input::Action::MoveItemRight:
    // Increase palette offset
    palette_offset_++;
    palette_offset_ = std::min(palette_offset_, 255);
    Logger::info("Palette offset: {} (indices shifted by {})", palette_offset_,
                 palette_offset_);
    mark_dirty();
    break;
  case input::Action::MoveItemUp:
    // Decrease header offset (start reading data earlier)
    header_offset_--;
    header_offset_ = std::max(header_offset_, 0);
    Logger::info("Header offset: {} (skip {} bytes before image data)",
                 header_offset_, header_offset_);
    mark_dirty();
    break;
  case input::Action::MoveItemDown:
    // Increase header offset (start reading data later)
    header_offset_++;
    Logger::info("Header offset: {} (skip {} bytes before image data)",
                 header_offset_, header_offset_);
    mark_dirty();
    break;
  case input::Action::ToggleExtraInfo:
    // Cycle through width/height combinations
    if (width_ == 52 && height_ == 59) {
      width_ = 51;
      height_ = 58;
    }
    else if (width_ == 51 && height_ == 58) {
      width_ = 58;
      height_ = 51;
    }
    else {
      width_ = 52;
      height_ = 59;
    }
    Logger::info("Dimensions: {}x{}", width_, height_);
    mark_dirty();
    break;
  case input::Action::ChangeItem:
    // Decrease bitplane padding
    bitplane_padding_--;
    bitplane_padding_ = std::max(bitplane_padding_, -100);
    Logger::info("Bitplane padding: {} bytes between each plane",
                 bitplane_padding_);
    mark_dirty();
    break;
  case input::Action::ChangeItem2:
    // Increase bitplane padding
    bitplane_padding_++;
    bitplane_padding_ = std::min(bitplane_padding_, 100);
    Logger::info("Bitplane padding: {} bytes between each plane",
                 bitplane_padding_);
    mark_dirty();
    break;
  case input::Action::HideItem:
    // Decrease bytes per line override
    if (bytes_per_line_override_ == -1) {
      bytes_per_line_override_ = (width_ + 7) / 8; // Start with auto value
    }
    bytes_per_line_override_--;
    bytes_per_line_override_ = std::max(bytes_per_line_override_, 1);
    Logger::info("Bytes per line: {} (manual override)",
                 bytes_per_line_override_);
    mark_dirty();
    break;
  case input::Action::LockItem:
    // Increase bytes per line override
    if (bytes_per_line_override_ == -1) {
      bytes_per_line_override_ = (width_ + 7) / 8; // Start with auto value
    }
    bytes_per_line_override_++;
    bytes_per_line_override_ = std::min(bytes_per_line_override_, 20);
    Logger::info("Bytes per line: {} (manual override)",
                 bytes_per_line_override_);
    mark_dirty();
    break;
  case input::Action::LoadRecipe:
    // Decrease height
    height_--;
    height_ = std::max(height_, 1);
    Logger::info("Height: {}", height_);
    mark_dirty();
    break;
  case input::Action::SaveRecipe:
    // Increase height
    height_++;
    height_ = std::min(height_, 100);
    Logger::info("Height: {}", height_);
    mark_dirty();
    break;
  case input::Action::LoadCanonicalRecipe:
    // Decrease bitplane size extra
    bitplane_size_extra_--;
    bitplane_size_extra_ = std::max(bitplane_size_extra_, -100);
    Logger::info("Bitplane size extra: {} bytes added to each plane",
                 bitplane_size_extra_);
    mark_dirty();
    break;
  case input::Action::Fire:
    // Increase bitplane size extra
    bitplane_size_extra_++;
    bitplane_size_extra_ = std::min(bitplane_size_extra_, 100);
    Logger::info("Bitplane size extra: {} bytes added to each plane",
                 bitplane_size_extra_);
    mark_dirty();
    break;
  default:
    return false;
  }
  return true;
}

void AmigaBobViewer::tick() {}

void AmigaBobViewer::render(const RenderContext& ctx) {
  ctx.surface->fill(1);
  ctx.surface->palette(PaletteHandle{palette_});

  Logger::info("Character File Viewer: Loading file '{}'", filename_);

  std::ifstream is = fileutils::open(filename_);
  if (!is.is_open()) {
    fatal("Character File Viewer: Could not open file: " + filename_);
  }

  is.seekg(0, std::ios::end);
  std::streamsize length = is.tellg();
  is.seekg(0, std::ios::beg);

  Logger::info("Character File: File size: {} bytes", length);

  std::vector<uint8_t> data(static_cast<std::size_t>(length));

  if (!is.read(reinterpret_cast<char*>(data.data()), length)) {
    fatal("Character File Viewer: Failed to read file data: " + filename_);
  }

  // Character file structure:
  // - 18 bytes header (id, id+104, padding)
  // - 1248 bytes bitplane data (58x51, 3 bitplanes)
  // - 16 bytes first name
  // - 16 bytes last name
  // - rest is character attributes

  if (length < 18 + 1248 + 32) {
    Logger::warn("File too small to be a character file: {} bytes", length);
    return;
  }

  // Read header
  uint8_t file_index = data[0];
  uint8_t index_offset = data[1];

  Logger::info("Character header: index={}, offset={}", file_index,
               index_offset);

  // Read name (starts at byte 1266 = 18 + 1248)
  std::string first_name(reinterpret_cast<char*>(&data[18 + 1248]), 16);
  std::string last_name(reinterpret_cast<char*>(&data[18 + 1248 + 16]), 16);

  // Trim null bytes
  first_name = first_name.c_str();
  last_name = last_name.c_str();

  Logger::info("Character name: {} {}", first_name, last_name);

  // Use adjustable parameters
  const int WIDTH = width_;
  const int HEIGHT = height_;

  // Calculate how much data we need based on parameters
  int bytes_per_line = (bytes_per_line_override_ == -1)
                           ? (WIDTH + 7) / 8
                           : bytes_per_line_override_;

  // Calculate actual size INCLUDING padding between bitplanes
  int bytes_per_plane = bytes_per_line * HEIGHT;
  int total_bitplane_data = 0;
  for (int i = 0; i < num_bitplanes_; ++i) {
    total_bitplane_data += bytes_per_plane;
    if (i < num_bitplanes_ - 1) {
      total_bitplane_data += bitplane_padding_; // Padding between planes
    }
    if (i == num_bitplanes_ - 1) {
      total_bitplane_data += bitplane_size_extra_; // Extra on last plane
    }
  }

  Logger::info("=== Decoding Parameters ===");
  Logger::info("Header offset: {}", header_offset_);
  Logger::info("Number of bitplanes: {}", num_bitplanes_);
  Logger::info("Image dimensions: {}x{}", WIDTH, HEIGHT);
  Logger::info("Bytes per line: {} {}", bytes_per_line,
               bytes_per_line_override_ == -1 ? "(auto)" : "(manual override)");
  Logger::info("Bytes per plane: {} bytes", bytes_per_plane);
  Logger::info("Bitplane padding: {} bytes", bitplane_padding_);
  Logger::info("Bitplane size extra: {} bytes", bitplane_size_extra_);
  Logger::info("Total bitplane data needed: {} bytes (including padding)",
               total_bitplane_data);
  Logger::info("File size: {} bytes, character data ends at byte {}",
               data.size(), data.size() - 104);

  // Extract bitplane data starting at adjustable offset
  int bitplane_size = total_bitplane_data;
  if (header_offset_ + bitplane_size > static_cast<int>(data.size()) - 104) {
    Logger::warn("Not enough data! offset {} + size {} > available {} (file "
                 "size {} - 104 char data)",
                 header_offset_, bitplane_size, data.size() - 104, data.size());
    bitplane_size = data.size() - 104 - header_offset_;
  }

  std::vector<uint8_t> bitplane_data(data.begin() + header_offset_,
                                     data.begin() + header_offset_ +
                                         bitplane_size);

  Logger::info("First 32 bytes of bitplane data at offset {}:", header_offset_);
  for (int i = 0; i < 32 && i < static_cast<int>(bitplane_data.size());
       i += 8) {
    int remaining = std::min(8, static_cast<int>(bitplane_data.size()) - i);
    if (remaining >= 8) {
      Logger::info("  {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x}",
                   bitplane_data[i], bitplane_data[i + 1], bitplane_data[i + 2],
                   bitplane_data[i + 3], bitplane_data[i + 4],
                   bitplane_data[i + 5], bitplane_data[i + 6],
                   bitplane_data[i + 7]);
    }
  }

  // Decode the bitplane data with 2x scaling for visibility (planar format)
  gfx::Surface decoded = BitplaneDecoder::decode(
      bitplane_data, WIDTH, HEIGHT, num_bitplanes_, bytes_per_line, palette_, 2,
      bitplane_padding_, bitplane_size_extra_);

  Logger::info("Decoded surface: {}x{}", decoded.size().w, decoded.size().h);

  // Count unique indices and show distribution
  auto* pixels = decoded.data();
  std::map<uint8_t, int> index_counts;
  for (int y = 0; y < HEIGHT; ++y) {
    for (int x = 0; x < WIDTH; ++x) {
      int scaled_x = x * 2;
      int scaled_y = y * 2;
      uint8_t pixel_index = pixels[scaled_y * decoded.width() + scaled_x];
      index_counts[pixel_index]++;
    }
  }

  /*
  // Dump pixel indices in grid format (before scaling, so we show original
  data) printf("\n=== Pixel Index Grid (%dx%d) ===\n", WIDTH, HEIGHT); for (int
  y = 0; y < HEIGHT; ++y) { for (int x = 0; x < WIDTH; ++x) {
      // Account for 2x scaling - take every other pixel
      int scaled_x = x * 2;
      int scaled_y = y * 2;
      uint8_t pixel_index = pixels[scaled_y * decoded.width() + scaled_x];
      if (pixel_index == 6) {
        printf("  ");
      }
      else {
        printf("%1x ", pixel_index);
      }
    }
    printf("\n");
  }
  printf("=== End Pixel Index Grid ===\n\n");
  */

  // Apply palette offset if set
  if (palette_offset_ != 0) {
    Logger::info("Applying palette offset: {}", palette_offset_);
    auto* pixels = decoded.data();
    size_t pixel_count = decoded.width() * decoded.height();
    for (size_t i = 0; i < pixel_count; ++i) {
      int new_index = static_cast<int>(pixels[i]) + palette_offset_;
      // Clamp to 0-255
      new_index = std::clamp(new_index, 0, 255);
      pixels[i] = static_cast<uint8_t>(new_index);
    }
  }

  // Blit to screen
  ctx.surface->blit_raw(decoded, ViewPoint{50, 50}); // Offset from top-left

  Logger::info("=== Character: {} {} (ID: {}) ===", first_name, last_name,
               file_index);
}
