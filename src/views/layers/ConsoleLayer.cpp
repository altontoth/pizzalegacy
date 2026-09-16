/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/layers/ConsoleLayer.h"

#include <algorithm>
#include "engine/DisplayConstants.h"
#include "engine/gfx/Draw.h"
#include "engine/input/InputManager.h"
#include "engine/ui/InteractionEvent.h"
#include "resources/FontManager.h"
#include "resources/util/TextEncoding.h"
#include "views/base/ViewDefs.h"

ConsoleLayer::ConsoleLayer(GameContext& ctx, GameState& st, DebugConsole& dc)
    : View(ctx, st), console_(dc),
      listener_(ctx.input->register_listener(*this)) {
  console_.log(LogLevel::Info, "Debug Console Initialized.");
}

void ConsoleLayer::render(const RenderContext& ctx) {
  if (!is_active()) {
    return;
  }

  // Draw console background and border
  ViewRect rect{0, 0, DisplayConstants::SCREEN_WIDTH, console_height_};
  ctx.surface->fill_rect(rect, 240);
  gfx::draw::line(*ctx.surface, {0, console_height_},
                  {DisplayConstants::SCREEN_WIDTH - 1, console_height_}, 246);

  constexpr uint8_t command_text_color = 139;             // blue-ish
  constexpr uint8_t result_text_color = 15;               // white-ish
  constexpr uint8_t output_text_color = 69;               // yellow-ish
  constexpr uint8_t typed_text_color = result_text_color; // white-ish

  // Input line is always at a fixed position near the bottom
  constexpr int bottom_margin = 4;
  const int input_y = console_height_ - font_line_height_ - bottom_margin;
  context_.font->render_text_onto(*ctx.surface, " > " + input_,
                                  ViewPoint{4, input_y}, FontFace::MINI,
                                  typed_text_color);

  const auto& lines = console_.lines();
  if (lines.empty()) {
    return;
  }

  // Available space for output lines (above the input line with a small gap)
  constexpr int top_margin = 4;
  constexpr int gap_above_input = 2;
  const int output_top = top_margin;
  const int output_bottom = input_y - gap_above_input;
  const int available_height = output_bottom - output_top;
  const int text_width =
      DisplayConstants::SCREEN_WIDTH - 8; // 4px margin each side

  auto line_height = [&](std::size_t i) {
    int num_wrapped = context_.font->count_wrapped_lines(
        lines[i].text, FontFace::MINI, text_width);
    return std::max(1, num_wrapped) * font_line_height_;
  };

  // How many of the oldest lines fill the screen, which caps how far back
  // the view can scroll.
  std::size_t oldest_page = 0;
  for (int height = 0; oldest_page < lines.size(); ++oldest_page) {
    height += line_height(oldest_page);
    if (height > available_height && oldest_page > 0) {
      break;
    }
  }
  max_scroll_ = lines.size() - oldest_page;
  scroll_visible_ = static_cast<int>(oldest_page);

  // scroll_ = 0 means viewing newest content (bottom of history)
  // scroll_ > 0 means scrolled up into older content
  scroll_ = std::min(scroll_, max_scroll_);

  // The anchor line is the newest line we want visible at the bottom
  const std::size_t anchor_index = lines.size() - 1 - scroll_;

  // Work backwards from anchor, measuring each line to find what fits
  struct LineLayout {
    std::size_t index;
    int height;
  };
  std::vector<LineLayout> visible_lines;
  int accumulated_height = 0;

  for (std::size_t i = anchor_index + 1; i > 0;) {
    --i;
    const int line_h = line_height(i);
    // The line that reaches past the top is still drawn, cut off.
    accumulated_height += line_h;
    visible_lines.push_back({i, line_h});

    if (accumulated_height >= available_height) {
      break;
    }
  }

  // Reverse so we render oldest first (top to bottom)
  std::ranges::reverse(visible_lines);

  // Drawn on a surface of its own so the topmost line is clipped
  Surface output(DisplayConstants::SCREEN_WIDTH, available_height);
  output.fill(240);

  // Calculate y positions - align content to the bottom of the output area
  int y = available_height - accumulated_height;

  for (const auto& vl : visible_lines) {
    uint8_t col = typed_text_color;
    switch (lines[vl.index].type) {
    case ConsoleLineType::Command:
      col = command_text_color;
      break;
    case ConsoleLineType::Result:
      col = result_text_color;
      break;
    case ConsoleLineType::Output:
      col = output_text_color;
      break;
    }
    context_.font->render_text_onto(output, lines[vl.index].text,
                                    ViewPoint{4, y}, FontFace::MINI, col);
    y += vl.height;
  }
  ctx.surface->blit_raw(output, ViewPoint{0, output_top});

  scroll_track_ = ViewRect{0, output_top, DisplayConstants::SCREEN_WIDTH,
                           output_bottom - output_top};
  scroll_bar_.draw(*ctx.surface, scroll_track_, scroll_extent());
}

ScrollBar::Extent ConsoleLayer::scroll_extent() const {
  return {.offset = static_cast<int>(scroll_),
          .visible = scroll_visible_,
          .total = static_cast<int>(console_.lines().size())};
}

bool ConsoleLayer::on_text(const input::TextEvent& ev) {
  if (!is_active()) {
    return false;
  }

  input_ += ev.text;
  text::truncate_utf8(input_, MAX_INPUT_BYTES);
  mark_dirty();
  return true;
}

bool ConsoleLayer::on_action(const input::ActionEvent& ev) {
  if (!ev.state.pressed ||
      (!is_active() && ev.action != input::Action::ToggleConsole)) {
    return false;
  }

  switch (ev.action) {
  // Toggle console visibility even when hidden
  case input::Action::ToggleConsole:
    if (is_active()) {
      hide();
      interactions_.begin_frame(); // clear frame items
      text_session_.reset();
    }
    else {
      show();
      text_session_.emplace(context_.input->start_text_input(*this));
    }
    scroll_ = 0;
    mark_dirty();
    return true;

  default:
    if (!is_active()) {
      return false;
    }
    break;
  }

  // Console is active: handle edit/navigation
  switch (ev.action) {
  case input::Action::SubmitInput:
    if (!input_.empty()) {
      commit_input();
    }
    mark_dirty();
    return true;

  case input::Action::Backspace:
    if (!input_.empty()) {
      text::pop_back_utf8(input_);
      mark_dirty();
    }
    return true;

  case input::Action::EraseLine:
    if (!input_.empty()) {
      input_.clear();
      mark_dirty();
    }
    return true;

  case input::Action::PageUp:
    scroll_page(-1);
    mark_dirty();
    return true;

  case input::Action::PageDown:
    scroll_page(+1);
    mark_dirty();
    return true;

  case input::Action::MoveUp:
    navigate_history(-1);
    mark_dirty();
    return true;

  case input::Action::MoveDown:
    navigate_history(+1);
    mark_dirty();
    return true;

  default:
    return false;
  }
}

void ConsoleLayer::commit_input() {
  console_.execute_command(input_);
  cmd_history_.push_back(input_);
  cmd_hist_index_ = cmd_history_.size();
  input_.clear();

  // Reset scroll to show newest content
  scroll_ = 0;
}

void ConsoleLayer::navigate_history(int delta) {
  if (cmd_history_.empty()) {
    return;
  }

  const std::size_t size = cmd_history_.size();
  std::size_t new_idx = cmd_hist_index_;

  if (delta < 0 && new_idx > 0) {
    new_idx--;
  }
  else if (delta > 0 && new_idx < size) {
    new_idx++;
  }

  cmd_hist_index_ = new_idx;
  input_ =
      (cmd_hist_index_ == size) ? std::string{} : cmd_history_[cmd_hist_index_];
}

void ConsoleLayer::scroll_page(int delta) {
  // Scroll by roughly a "page" worth of lines, TODO calculate this
  scroll_line(delta * 31);
}

void ConsoleLayer::scroll_line(int delta) {
  const auto num_lines = console_.lines().size();
  if (num_lines == 0) {
    scroll_ = 0;
    return;
  }

  // scroll_ = 0 means at bottom (newest), increasing scrolls up into history
  // delta > 0 means scroll down (towards newer, decrease scroll_)
  // delta < 0 means scroll up (towards older, increase scroll_)
  int new_scroll = static_cast<int>(scroll_) - delta;
  new_scroll = std::clamp(new_scroll, 0, static_cast<int>(max_scroll_));
  scroll_ = static_cast<std::size_t>(new_scroll);
}

bool ConsoleLayer::freezes_view() const { return is_active(); }

void ConsoleLayer::tick() {
  if (!is_active()) {
    return;
  }
  interactions_.begin_frame();
  ViewRect console_rect{0, 0, DisplayConstants::SCREEN_WIDTH, console_height_};
  interactions_.create_frame_item(
      console_rect, "console_scroll",
      [this](views::ui::InteractionEvent ie) {
        int offset = static_cast<int>(scroll_);
        if (scroll_bar_.on_interaction(ie, ie.local, scroll_track_,
                                       scroll_extent(), offset)) {
          scroll_ = static_cast<std::size_t>(offset);
          mark_dirty();
          return;
        }
        if (ie.type != views::ui::InteractionEvent::Type::Scroll) {
          return;
        }
        scroll_line(-ie.scroll_delta);
        mark_dirty();
      },
      {.drag_button = views::ui::MouseButton::Left});
}
