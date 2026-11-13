/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <algorithm>
#include <array>
#include <functional>
#include "core/time/TimeOfDay.h"
#include "views/ui/popups/BasePopup.h"
#include "views/ui/popups/PopupButton.h"

class BusinessHoursPopup : public BasePopup {
public:
  using Time = core::time::TimeOfDay;
  using Apply = std::function<void(Time, Time)>;
  BusinessHoursPopup(const GameContext& context, Time opening, Time closing,
                     bool enabled, Apply apply, std::function<void()> toggle)
      : BasePopup(context), opening_(opening.to_block()),
        closing_(closing.to_block()), apply_(std::move(apply)),
        toggle_(std::move(toggle)),
        open_earlier_({24, 52, 46, 32}, "-", [this] { adjust_open(-3); }),
        open_later_({292, 52, 46, 32}, "+", [this] { adjust_open(3); }),
        close_earlier_({24, 100, 46, 32}, "-", [this] { adjust_close(-3); }),
        close_later_({292, 100, 46, 32}, "+", [this] { adjust_close(3); }),
        save_({24, 176, 92, 36}, "Apply", [this] {
          apply_(Time::from_block(opening_), Time::from_block(closing_));
          finished();
        }),
        cancel_({130, 176, 92, 36}, "Cancel", [this] { finished(); }),
        switch_({236, 176, 102, 36}, enabled ? "Close" : "Open", [this] {
          toggle_();
          finished();
        }) {
    open_earlier_.id("hours_open_earlier");
    open_later_.id("hours_open_later");
    close_earlier_.id("hours_close_earlier");
    close_later_.id("hours_close_later");
    add_shortcut(input::Action::SubmitInput, &save_);
    add_shortcut(input::Action::Exit, &cancel_);
  }

  Size popup_size() const override { return {362, 230}; }
  void add_interactions(views::ui::InteractionContext ctx, ViewPoint origin) override {
    for (auto* button : buttons()) button->add_interactions(ctx, origin);
  }
  void render(Surface& surface, ViewPoint origin) override {
    auto pop = popup_surface(362, 230);
    auto line = [&](const std::string& text, int y, FontFace face = FontFace::ASTANDAR) {
      font_->render_line_onto(pop, text, {181, y}, face, 6, TextAnchor::Middle);
    };
    line("Business hours", 18);
    line(compat::format("Open: {}", Time::from_block(opening_)), 59);
    line(compat::format("Close: {}", Time::from_block(closing_)), 107);
    const int minutes = (closing_ - opening_) * 10;
    line(compat::format("{}h {}m per day", minutes / 60, minutes % 60), 148);
    for (auto* button : buttons()) button->render_with(*this, pop);
    surface.blit_raw(pop, origin);
  }

private:
  // Same-day schedules only. The original game clock is in ten-minute blocks.
  // Preserve a minimum one-hour business day and allow 06:00 to 23:00.
  void adjust_open(int delta) {
    opening_ = std::clamp(opening_ + delta, 36, closing_ - 6);
    mark_dirty();
  }
  void adjust_close(int delta) {
    closing_ = std::clamp(closing_ + delta, opening_ + 6, 138);
    mark_dirty();
  }
  std::array<PopupButton*, 7> buttons() {
    return {&open_earlier_, &open_later_, &close_earlier_, &close_later_,
            &save_, &cancel_, &switch_};
  }
  int opening_, closing_;
  Apply apply_;
  std::function<void()> toggle_;
  PopupButton open_earlier_, open_later_, close_earlier_, close_later_;
  PopupButton save_, cancel_, switch_;
};
