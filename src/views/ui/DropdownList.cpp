/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/DropdownList.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include "engine/geom/Spaces.h"
#include "views/ui/DropdownDecoration.h"

DropdownList::DropdownList(ViewRect header_rect, ViewPoint origin,
                           std::optional<std::function<void()>> on_activate_cb)
    : header_rect_(header_rect), header_bar_(Surface(bar_width_, bar_height_)),
      origin_(origin), on_activate_cb_(std::move(on_activate_cb)) {

  views::ui::dropdowndecoration::draw_dropdown_decoration(header_bar_, 0);
}

void DropdownList::render(Surface& target_surface, FontManager* font) {
  if (!expanded_) {
    return;
  }

  const int start_x = origin_.x;
  const int start_y = origin_.y;

  uint8_t background_color = 13;
  ViewRect bg_rect = {origin_.x, origin_.y + bar_height_, bar_width_,
                      current_visible_height_ + line_padding_};
  target_surface.fill_rect(bg_rect, background_color);

  target_surface.blit_raw(header_bar_, origin_);

  // Determine how many entries we can show
  int available_space_for_entries = current_visible_height_;
  int max_visible_entries =
      std::max(0, available_space_for_entries / (entry_height_));

  for (int i = 0;
       i < static_cast<int>(entries_.size()) && i < max_visible_entries; ++i) {
    int text_x = start_x + 8;
    int text_y = start_y + bar_height_ + line_padding_ + (i * entry_height_);

    uint8_t text_color_regular = 240;
    uint8_t text_color_highlight = 255;
    auto text_color = (hovered_index_ && *hovered_index_ == i)
                          ? text_color_highlight
                          : text_color_regular;

    if (entries_[i].swatch_color) {
      constexpr int swatch_size = 6;
      constexpr int swatch_gap = 6;
      const int line_height =
          font->size_text(entries_[i].label, FontFace::CSTANDAR).height;
      target_surface.fill_rect(
          ViewRect{text_x, text_y + ((line_height - swatch_size) / 2),
                   swatch_size, swatch_size},
          *entries_[i].swatch_color);
      text_x += swatch_size + swatch_gap;
    }

    font->render_text_onto(target_surface, entries_[i].label,
                           ViewPoint{text_x, text_y}, FontFace::CSTANDAR,
                           text_color);
  }

  int bottom_y =
      start_y + bar_height_ + line_padding_ + current_visible_height_;
  target_surface.blit_raw(header_bar_, {origin_.x, bottom_y});
}

bool DropdownList::tick() {
  bool something_changed = false;
  if (animating_open_ && current_visible_height_ < full_height_) {
    something_changed = true;
    current_visible_height_ += dropdown_anim_speed_;
    if (current_visible_height_ >= full_height_) {
      current_visible_height_ = full_height_;
      animating_open_ = false;
    }
  }
  return something_changed;
}

void DropdownList::add_entry(std::string_view label,
                             std::function<void()> on_click,
                             std::optional<uint8_t> swatch_color) {
  entries_.push_back(DropdownEntry{.label = std::string(label),
                                   .on_click = std::move(on_click),
                                   .swatch_color = swatch_color});
}

std::optional<int> DropdownList::entry_index_at(ViewPoint p) const {
  if (!expanded_) {
    return std::nullopt;
  }

  const int rel_x = p.x - origin_.x;
  const int rel_y = p.y - origin_.y - bar_height_ - line_padding_;

  // Outside the drop-down rectangle:
  if (rel_x < 0 || rel_x > bar_width_ || rel_y < 0) {
    return std::nullopt;
  }

  const int idx = rel_y / entry_height_;
  if (idx >= 0 && idx < static_cast<int>(entries_.size())) {
    return idx;
  }

  return std::nullopt;
}

bool DropdownList::is_expanded() const { return expanded_; }

void DropdownList::add_interactions(views::ui::InteractionContext& ui,
                                    std::function<void()> on_pick) {
  auto id = std::hash<ViewRect>{}(header_rect_);

  int max_rows = current_visible_height_ / entry_height_;
  for (int i = 0; i < max_rows && i < (int)entries_.size(); ++i) {
    ViewRect row_rect{origin_.x,
                      origin_.y + bar_height_ + line_padding_ +
                          (i * entry_height_),
                      bar_width_, entry_height_};
    ui.layers.create_frame_item(
        row_rect, compat::format("dd_row_{}_{}", id, i),
        [this, i, on_pick,
         inv = ui.invalidate](const views::ui::InteractionEvent& ev) {
          switch (ev.type) {
          case views::ui::InteractionEvent::Type::Enter:
            hovered_index_ = i;
            inv();
            break;
          case views::ui::InteractionEvent::Type::Leave:
            hovered_index_.reset();
            inv();
            break;
          case views::ui::InteractionEvent::Type::Click:
            if (ev.button == views::ui::MouseButton::Left) {
              entries_[i].on_click();
              hovered_index_.reset();
              on_pick();
              inv();
            }
            break;
          default:
            break;
          }
        });
  }
}

void DropdownList::expanded(bool expanded) {
  if (expanded) {
    expanded_ = true;
    animating_open_ = true;
    current_visible_height_ = 0;
    full_height_ = static_cast<int>(entries_.size()) * entry_height_;
    if (on_activate_cb_) {
      (*on_activate_cb_)();
    }
  }
  else {
    expanded_ = false;
    animating_open_ = false;
  }
}
