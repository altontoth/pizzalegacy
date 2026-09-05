/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/viewers/VgaViewer.h"

#include "engine/DisplayConstants.h"
#include "engine/gfx/Draw.h"
#include "engine/gfx/PaletteBank.h"
#include "engine/gfx/Surface.h"
#include "engine/ui/InteractionEvent.h"
#include "resources/AssetMetadata.h"
#include "resources/FontManager.h"
#include "resources/util/FileUtils.h"
#include "views/base/ViewDefs.h"

VgaViewer::VgaViewer(GameContext& context, GameState& state,
                     std::string filename)
    : View(context, state), filename_(filename),
      image_(context_.vga->get(filename).clone()),
      camctrl_(context.input, camera_),
      listener_(context.input->register_listener(*this)) {

  // Initialize palette from asset metadata if available
  auto palette_opt = resources::AssetMetadataRegistry::get_palette(filename_);
  if (palette_opt.has_value()) {
    palette_ = palette_opt.value();
    Logger::info("VgaViewer: Starting with palette {} for {}", palette_,
                 filename_);
  }

  // Set initial palette on image
  image_.palette(PaletteHandle{palette_});

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

bool VgaViewer::on_action(const input::ActionEvent& ev) {
  if (!ev.state.pressed) {
    return false;
  }
  switch (ev.action) {
  case input::Action::CycleForward:
    if (++palette_ >= gfx::VGA_PALETTE_COUNT) {
      palette_ = 0;
    }
    image_.palette(PaletteHandle{palette_});
    Logger::info("Palette is {} (offset: {})", palette_, palette_offset_);
    mark_dirty();
    break;
  case input::Action::CycleBackward:
    if (palette_ == 0) { // This is an unsigned int, so no --number < 0
      palette_ = gfx::VGA_PALETTE_COUNT - 1;
    }
    else {
      --palette_;
    }
    image_.palette(PaletteHandle{palette_});
    Logger::info("Palette is {} (offset: {})", palette_, palette_offset_);
    mark_dirty();
    break;
  case input::Action::ToggleExtraInfo:
    show_blitmap_overlay_ = !show_blitmap_overlay_;
    mark_dirty();
    Logger::info("Toggle overlay");
    break;
  case input::Action::CycleUp:
    palette_offset_++;
    if (palette_offset_ >= 32) {
      palette_offset_ = 0;
    }
    Logger::info("Palette offset: {}", palette_offset_);
    apply_palette_rotation();
    mark_dirty();
    break;
  case input::Action::CycleDown:
    palette_offset_--;
    if (palette_offset_ < 0) {
      palette_offset_ = 31;
    }
    Logger::info("Palette offset: {}", palette_offset_);
    apply_palette_rotation();
    mark_dirty();
    break;
  default:
    return false;
  }
  return true;
}

void VgaViewer::handle_left_mouse_click(ViewPoint p) {
  if (p.x < image_.width() && p.y < image_.height()) {
    Logger::warn("Palette index at {}x{}: {}", p.x, p.y, image_.get(p));
  }
}

void VgaViewer::apply_palette_rotation() {
  if (palette_offset_ == 0) {
    // No rotation needed, use base palette
    image_.clear_custom_palette();
    return;
  }

  // Get base palette colors
  auto base_colors =
      gfx::PaletteBank::instance().colors(PaletteHandle{palette_});

  // Create rotated palette
  std::vector<gfx::Color> rotated_colors = base_colors;

  for (int i = 0; i < 256; ++i) {
    rotated_colors[i] = base_colors[(i + palette_offset_) % 256];
  }

  // Apply rotated palette to image
  image_.colors(rotated_colors);
}

void VgaViewer::render(const RenderContext& ctx) {
  ctx.surface->fill(0x0F);
  // Get viewport from scroll
  ViewRect vp = camera_.world_to_view(camera_.visible_world_rect());

  // Set the palette (base or rotated)
  ctx.surface->palette(PaletteHandle{palette_});
  if (palette_offset_ != 0) {
    // Apply rotated palette to destination surface
    auto base_colors =
        gfx::PaletteBank::instance().colors(PaletteHandle{palette_});
    std::vector<gfx::Color> rotated_colors = base_colors;
    for (int i = 0; i < 256; ++i) {
      rotated_colors[i] = base_colors[(i + palette_offset_) % 256];
    }
    ctx.surface->colors(rotated_colors);
  }
  else {
    // Clear any custom palette when not rotating
    ctx.surface->clear_custom_palette();
  }

  // Blit the visible portion of the image
  ctx.surface->blit(image_, vp);

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

void VgaViewer::tick() {
  if (camctrl_.tick()) {
    mark_dirty();
  }
}
