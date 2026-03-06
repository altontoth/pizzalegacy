/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/viewers/RawViewer.h"

#include <algorithm>
#include <fstream>
#include <vector>
#include "engine/DisplayConstants.h"
#include "engine/ui/InteractionEvent.h"
#include "gfx/Draw.h"
#include "gfx/Surface.h"
#include "resources/BitplaneDecoder.h"
#include "resources/FontManager.h"
#include "views/base/ViewDefs.h"

constexpr int camctrl_amount_px_x = 10;
constexpr int camctrl_amount_px_y = 10;

// Load a raw image file. If width and height are provided, assume bitplane
// format. Otherwise, try to read 4-byte header with width/height (little-endian
// uint32_t)
static gfx::Surface load_raw_image(const std::string& filename,
                                   std::optional<int> explicit_width,
                                   std::optional<int> explicit_height) {
  std::ifstream file(filename, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    fatal("Failed to open raw image file: " + filename);
  }

  std::streamsize file_size = file.tellg();
  file.seekg(0, std::ios::beg);

  // Read all file data upfront so it's available in all code paths
  std::vector<uint8_t> raw_data(file_size);
  file.read(reinterpret_cast<char*>(raw_data.data()), file_size);
  file.close();

  uint32_t width = 0, height = 0;
  std::streamsize data_start = 0;

  // If dimensions are explicitly provided, auto-detect format based on file
  // size
  if (explicit_width && explicit_height) {
    width = *explicit_width;
    height = *explicit_height;
    data_start = 0;

    // Calculate expected sizes for different formats
    size_t expected_chunky = static_cast<size_t>(width * height);
    size_t expected_6bit = static_cast<size_t>(width * height * 6 / 8);

    // Try 5-plane bitplane with and without padding
    int data_bytes_per_line = (width + 7) / 8;
    int padded_bytes_per_line =
        ((data_bytes_per_line + 15) / 16) * 16; // Round up to 16-byte boundary
    size_t expected_5plane_padded =
        static_cast<size_t>(padded_bytes_per_line * height * 5);
    size_t expected_5plane_unpadded =
        static_cast<size_t>(data_bytes_per_line * height * 5);

    Logger::info("Using explicit dimensions: {}x{}", width, height);
    Logger::info("File size: {}, chunky: {}, 6-bit: {}, 5-plane (padded): {}, "
                 "5-plane (unpadded): {}",
                 file_size, expected_chunky, expected_6bit,
                 expected_5plane_padded, expected_5plane_unpadded);
    Logger::info("5-plane details: {} data bytes/line, {} padded bytes/line",
                 data_bytes_per_line, padded_bytes_per_line);

    // Auto-detect format based on file size
    if (file_size == static_cast<std::streamsize>(expected_5plane_unpadded)) {
      Logger::info("Detected 5-plane bitplane format without padding");
      return BitplaneDecoder::decode(raw_data, width, height, 5,
                                     data_bytes_per_line);
    }
    if (file_size == static_cast<std::streamsize>(expected_5plane_padded)) {
      Logger::info("Detected 5-plane bitplane format with padding");
      return BitplaneDecoder::decode(raw_data, width, height, 5,
                                     padded_bytes_per_line);
    }
    if (file_size == static_cast<std::streamsize>(expected_chunky)) {
      Logger::info("Detected chunky format (1 byte per pixel)");
      // Chunky format - just read directly as indexed pixels
      return gfx::Surface::from_indexed(raw_data, width, height,
                                        gfx::PaletteHandle{0}, 2);
    }
    Logger::warn(
        "File size doesn't match known formats, trying 5-plane (unpadded)");
    return BitplaneDecoder::decode(raw_data, width, height, 5,
                                   data_bytes_per_line);
  }

  // Try to read header from file (non-bitplane format)
  if (file_size < 8) {
    fatal("Raw file too small to contain header: " + filename);
  }

  // Read width and height from first 8 bytes (little-endian uint32_t)
  width = *reinterpret_cast<const uint32_t*>(raw_data.data());
  height = *reinterpret_cast<const uint32_t*>(raw_data.data() + 4);
  data_start = 8;

  Logger::info("Raw image header: width={}, height={}", width, height);

  // Verify that file size matches expected data size
  size_t expected_size = data_start + (width * height);
  if (static_cast<size_t>(file_size) != expected_size) {
    Logger::warn("File size mismatch: expected {}, got {} (difference: {})",
                 expected_size, file_size,
                 static_cast<int>(file_size) - static_cast<int>(expected_size));
  }

  // Extract pixel data from raw_data buffer starting at data_start
  size_t pixel_count = width * height;
  std::vector<uint8_t> pixels(pixel_count, 0);
  size_t available_pixels = file_size - data_start;
  size_t copy_count = std::min(pixel_count, available_pixels);

  if (copy_count > 0) {
    std::copy(raw_data.begin() + data_start,
              raw_data.begin() + data_start + copy_count, pixels.begin());
  }

  // Create surface from indexed pixel data with palette 0
  return gfx::Surface::from_indexed(pixels, width, height,
                                    gfx::PaletteHandle{0}, 2);
}

RawViewer::RawViewer(GameContext& context, GameState& state,
                     std::string filename, std::optional<int> width,
                     std::optional<int> height)
    : View(context, state), filename_(filename),
      image_(load_raw_image(filename, width, height)),
      camctrl_(context.input, camera_),
      listener_(context.input->register_listener(*this)) {

  Logger::warn("Image size is {}x{}", image_.width(), image_.height());
  camera_.view_size(
      Size{DisplayConstants::SCREEN_WIDTH, DisplayConstants::SCREEN_HEIGHT});
  camera_.world_size(image_.size());
  camctrl_.tile_grid(1, 1);

  interactions_.create_persistent_item(
      ViewRect{0, 0, DisplayConstants::SCREEN_WIDTH,
               DisplayConstants::SCREEN_HEIGHT},
      "vgaviewer_catchall", [this](views::ui::InteractionEvent ie) {
        if (ie.type == views::ui::InteractionEvent::Type::Click &&
            ie.button == views::ui::MouseButton::Left) {
          handle_left_mouse_click(ie.local);
        }
      });
}

bool RawViewer::on_action(const input::ActionEvent& ev) {
  if (!ev.state.pressed) {
    return false;
  }
  switch (ev.action) {
  case input::Action::CycleForward:
    if (++palette_ >= gfx::VGA_PALETTE_COUNT) {
      palette_ = 0;
    }
    Logger::info("Palette is {}", palette_);
    mark_dirty();
    break;
  case input::Action::CycleBackward:
    if (palette_ == 0) {
      palette_ = gfx::VGA_PALETTE_COUNT - 1;
    }
    else {
      --palette_;
    }
    Logger::info("Palette is {}", palette_);
    mark_dirty();
    break;
  case input::Action::ToggleExtraInfo:
    show_blitmap_overlay_ = !show_blitmap_overlay_;
    mark_dirty();
    Logger::info("Toggle overlay");
    break;
  default:
    return false;
  }
  return true;
}

void RawViewer::handle_left_mouse_click(ViewPoint p) {
  if (p.x < image_.width() && p.y < image_.height()) {
    Logger::warn("Palette index at {}x{}: {}", p.x, p.y, image_.get(p));
  }
}

void RawViewer::render(const RenderContext& ctx) {
  // Get viewport from scroll
  ViewRect vp = camera_.world_to_view(camera_.visible_world_rect());

  // Blit the visible portion of the image
  ctx.surface->blit(image_, vp);

  // Set the palette
  ctx.surface->palette(PaletteHandle{palette_});

  // Blitmap overlay rendering
  if (filename_.rfind("gfx/blitmap", 0) == 0 && show_blitmap_overlay_) {
    std::ifstream offsets_file = fileutils::open(
        "gfx/offsets" + filename_.substr(11, 1)); // "gfx/blitmap3" -> "3"
    if (!offsets_file.good()) {
      return;
    }

    // Color indices to rotate through for visibility
    std::vector<uint8_t> color_indices = {88, 104, 120, 136, 152};

    // We keep track of which positions we've seen before
    std::map<std::pair<int, int>, int> seen_positions;
    const int camctrl_x = vp.x;
    const int camctrl_y = vp.y;

    const int NUM_ENTRIES = 30;
    for (int i = 0; i < NUM_ENTRIES; ++i) {
      uint32_t val = 0;
      offsets_file.read(reinterpret_cast<char*>(&val), sizeof(val));

      // Double the offsets since they're for 1x but we're displaying 2x
      int x1 = (val & 0xFF) * 2;
      int y1 = ((val >> 8) & 0xFF) * 2;
      int x2 = (((val >> 16) & 0xFF) * 2) + 1;
      int y2 = (((val >> 24) & 0xFF) * 2) + 1;

      int left = std::min(x1, x2);
      int right = std::max(x1, x2);
      int top = std::min(y1, y2);
      int bottom = std::max(y1, y2);

      uint8_t color_idx = color_indices.at(i % color_indices.size());

      // Draw box adjusted for viewport scroll
      gfx::draw::box(*ctx.surface, ViewPoint{left - camctrl_x, top - camctrl_y},
                     ViewPoint{right - camctrl_x, bottom - camctrl_y},
                     color_idx, 1);

      // Draw sprite ID number
      int int_pos_x = left - camctrl_x + 3;
      int int_pos_y = top - camctrl_y + 3;
      std::pair<int, int> key = {int_pos_x, int_pos_y};

      if (seen_positions.contains(key)) {
        int_pos_y += seen_positions[key] * 9;
        seen_positions[key]++;
      }
      else {
        seen_positions[key] = 1;
      }

      context_.font->render_text_onto(*ctx.surface, std::to_string(i),
                                      ViewPoint{int_pos_x, int_pos_y},
                                      FontFace::MINI, color_idx);
    }
  }
}

void RawViewer::tick() {
  if (camctrl_.tick()) {
    mark_dirty();
  }
}
