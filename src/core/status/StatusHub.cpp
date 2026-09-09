/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/status/StatusHub.h"

StatusHub::Subscription StatusHub::subscribe(Subscriber s) {
  subscriber_ = std::move(s);
  subscribed_ = true;
  return Subscription{[this] {
    subscribed_ = false;
    subscriber_ = {};
  }};
}

void StatusHub::set_baseline(BaselineProvider p) {
  baseline_ = std::move(p);
  rebuild_and_push();
}

void StatusHub::rebuild_now() { rebuild_and_push(); }

void StatusHub::freeze_header(std::optional<core::PlayerId> player) {
  header_frozen_ = true;
  frozen_player_ = player;
  frozen_time_ = *time_;
  push_to_subscriber();
}

void StatusHub::unfreeze_header() {
  header_frozen_ = false;
  frozen_player_.reset();
  push_to_subscriber();
}

void StatusHub::refresh_header() { push_to_subscriber(); }

void StatusHub::reset_for_new_game(GameTime& time) {
  auto subscriber = std::move(subscriber_);
  bool subscribed = subscribed_;
  *this = StatusHub{time};
  subscriber_ = std::move(subscriber);
  subscribed_ = subscribed;
}

void StatusHub::log_ephemeral(const StatusMsg& m) {
  EphemeralEntry e{
      .time = core::time::make_stamp(*time_),
      .snapshot = current_,
      .severity = m.severity,
      .scope = m.scope,
  };
  ephemeral_log_.push_front(std::move(e));
  if (ephemeral_log_.size() > log_cap) {
    ephemeral_log_.pop_back();
  }
}

void StatusHub::post(StatusMsg m) {
  if (suppressed_) {
    return;
  }
  log_ephemeral(m);

  // Find a free slot; silently drop if the queue is full
  for (auto& slot : queue_) {
    if (!slot.has_value()) {
      const int not_before = time_->time_blocks() + m.delay_blocks;
      slot = QueueSlot{.msg = std::move(m), .not_before_block = not_before};
      return;
    }
  }
}

void StatusHub::post_direct(StatusMsg m) {
  if (suppressed_) {
    return;
  }
  log_ephemeral(m);
  show(std::move(m));
  rebuild_and_push();
}

void StatusHub::clear_queue() {
  queue_.fill(std::nullopt);
  rebuild_and_push();
}

void StatusHub::clear_overlay() {
  showing_ = std::nullopt;
  rebuild_and_push();
}

void StatusHub::show_previous() {
  if (!last_shown_msg_) {
    return;
  }
  // Replays without adding to the ephemeral log.
  show(*last_shown_msg_);
  rebuild_and_push();
}

void StatusHub::show(StatusMsg m) {
  last_shown_msg_ = m;
  const int ticks = m.display_ticks.value_or(default_display_ticks);
  showing_ = Showing{.msg = std::move(m), .remaining_ticks = ticks};
}

bool StatusHub::scan_queue() {
  if (showing_.has_value()) {
    return false;
  }
  const int current_block = time_->time_blocks();
  for (auto& slot : queue_) {
    if (slot.has_value() && slot->not_before_block <= current_block) {
      StatusMsg m = std::move(slot->msg);
      slot = std::nullopt;
      show(std::move(m));
      return true;
    }
  }
  return false;
}

void StatusHub::tick() {
  bool changed = false;

  if (showing_) {
    --showing_->remaining_ticks;
    if (showing_->remaining_ticks <= 0) {
      showing_ = std::nullopt;
      changed = true;
    }
  }

  changed = scan_queue() || changed;

  if (changed) {
    rebuild_and_push();
  }
}

void StatusHub::rebuild_and_push() {
  if (showing_) {
    const auto& m = showing_->msg;
    if (std::holds_alternative<std::string>(m.payload)) {
      StatusDisplayRecipe r;
      r.add_header(std::get<std::string>(m.payload), StatusAlignment::Left,
                   m.color);
      current_ = std::move(r);
    }
    else {
      current_ = std::get<StatusDisplayRecipe>(m.payload);
    }
  }
  else {
    current_ = baseline_ ? baseline_() : StatusDisplayRecipe{};
  }

  push_to_subscriber();
}

void StatusHub::push_to_subscriber() {
  if (subscribed_) {
    subscriber_(current_);
  }
}
