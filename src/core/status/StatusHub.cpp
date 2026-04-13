/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/status/StatusHub.h"

#include <chrono>

using namespace std::chrono;

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

void StatusHub::log_ephemeral(const StatusMsg& m) {
  EphemeralEntry e{
      .time = core::time::make_stamp(time_),
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
      const int not_before = time_.time_blocks() + m.delay_blocks;
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
  last_shown_msg_ = m;

  // Cancel 'currently showing' tracking for any queued message but don't
  // free its slot: it will redisplay once this direct message expires.
  showing_queued_ = std::nullopt;

  const auto ttl = m.ttl.value_or(milliseconds{6000});
  showing_direct_ = ShowingDirect{.msg = std::move(m), .remaining_ms = ttl};
  rebuild_and_push();
}

void StatusHub::clear_queue() {
  queue_.fill(std::nullopt);
  showing_queued_ = std::nullopt;
  rebuild_and_push();
}

void StatusHub::clear_overlay() {
  if (showing_queued_) {
    queue_[showing_queued_->slot_idx] = std::nullopt;
    showing_queued_ = std::nullopt;
  }
  showing_direct_ = std::nullopt;
  rebuild_and_push();
}

void StatusHub::show_baseline() {
  // Free the slot of the currently-showing queued message (it was dismissed)
  if (showing_queued_) {
    queue_[showing_queued_->slot_idx] = std::nullopt;
    showing_queued_ = std::nullopt;
  }
  showing_direct_ = std::nullopt;
  // Other pending queue slots are left intact, they will show when their time
  // comes
  rebuild_and_push();
}

void StatusHub::show_previous() {
  if (!last_shown_msg_) {
    return;
  }
  // Re-show as a direct message with a fresh TTL, without adding to history
  StatusMsg m = *last_shown_msg_;
  const auto ttl = m.ttl.value_or(milliseconds{6000});
  showing_queued_ = std::nullopt;
  showing_direct_ = ShowingDirect{.msg = std::move(m), .remaining_ms = ttl};
  rebuild_and_push();
}

bool StatusHub::scan_queue() {
  // Direct takes priority, don't start a queued message while one is showing
  if (showing_direct_.has_value()) {
    return false;
  }
  // Already showing a queued message
  if (showing_queued_.has_value()) {
    return false;
  }
  const int current_block = time_.time_blocks();
  for (int i = 0; i < QUEUE_SIZE; ++i) {
    auto& slot = queue_[i];
    if (slot.has_value() && slot->not_before_block <= current_block) {
      const auto ttl = slot->msg.ttl.value_or(milliseconds{6000});
      last_shown_msg_ = slot->msg;
      showing_queued_ = ShowingQueued{i, ttl};
      rebuild_and_push();
      return true;
    }
  }
  return false;
}

void StatusHub::update(uint64_t dt_ms) {
  const auto dt = milliseconds{static_cast<milliseconds::rep>(dt_ms)};
  bool changed = false;

  if (showing_direct_) {
    showing_direct_->remaining_ms -= dt;
    if (showing_direct_->remaining_ms <= milliseconds::zero()) {
      showing_direct_ = std::nullopt;
      scan_queue();
      changed = true;
    }
  }
  else if (showing_queued_) {
    showing_queued_->remaining_ms -= dt;
    if (showing_queued_->remaining_ms <= milliseconds::zero()) {
      queue_[showing_queued_->slot_idx] = std::nullopt;
      showing_queued_ = std::nullopt;
      scan_queue();
      changed = true;
    }
  }
  else {
    changed = scan_queue();
  }

  if (changed) {
    rebuild_and_push();
  }
}

void StatusHub::rebuild_and_push() {
  if (showing_direct_) {
    const auto& m = showing_direct_->msg;
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
  else if (showing_queued_) {
    const auto& m = queue_[showing_queued_->slot_idx]->msg;
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
