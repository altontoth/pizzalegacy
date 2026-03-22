/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/status/StatusHub.h"

#include <algorithm>
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
  clear_overlay();
  rebuild_and_push();
}

void StatusHub::rebuild_now() {
  if (!prev_index_.has_value()) {
    return; // in "previous" view, don't rebuild
  }
  rebuild_and_push();
}

void StatusHub::insert_or_replace(StatusMsg m) {
  if (!m.coalesce_key.empty()) {
    // replace older one with same key
    /*
    for (auto& lm : active_) {
      if (lm.msg.coalesce_key == m.coalesce_key) {
        lm.msg = std::move(m);
        lm.remaining_ms = lm.msg.ttl;
        return;
      }
    }
    */
  }
  active_.push_back({std::move(m), m.ttl});
}

bool StatusHub::decay_overlays(std::chrono::milliseconds dt) {
  using ms = std::chrono::milliseconds;
  if (dt <= ms::zero() || active_.empty()) {
    return false;
  }

  for (auto& live : active_) {
    if (!live.remaining_ms) {
      // sticky (no TTL)
      continue;
    }
    live.remaining_ms =
        (*live.remaining_ms > dt) ? (*live.remaining_ms - dt) : ms::zero();
  }

  const auto before = active_.size();
  std::erase_if(active_, [](const LiveMsg& l) {
    return l.remaining_ms && *l.remaining_ms <= ms::zero();
  });
  return active_.size() != before;
}

bool StatusHub::decay_overlays(uint64_t dt_ms) {
  return decay_overlays(std::chrono::milliseconds{
      static_cast<std::chrono::milliseconds::rep>(dt_ms)});
}

void StatusHub::post(StatusMsg m) {
  if (suppressed_) {
    return;
  }
  insert_or_replace(std::move(m));

  // Compose a snapshot for prev_stack_ and (optionally) for ephemeral_log_
  rebuild_and_push(); // updates current_

  // Push snapshot into prev_stack_ (ring)
  prev_stack_.push_front(current_);
  if (prev_stack_.size() > prev_cap) {
    prev_stack_.pop_back();
  }
  prev_index_.reset(); // remain in live view unless the UI requested otherwise

  // If ephemeral, also store into the log
  if (m.type == TransientType::Ephemeral) {
    EphemeralEntry e{
        .time = core::time::make_stamp(time_),
        .snapshot = current_,
        .severity = m.severity,
        .scope = m.scope,
        .key = m.coalesce_key,
    };
    ephemeral_log_.push_front(std::move(e));
    if (ephemeral_log_.size() > log_cap) {
      ephemeral_log_.pop_back();
    }
  }
}

void StatusHub::clear_overlay() {
  active_.clear();
  prev_index_.reset();
  rebuild_and_push();
}

void StatusHub::show_baseline() {
  prev_index_.reset(); // return to live composite
  active_.clear();
  rebuild_and_push();
}

void StatusHub::show_previous() {
  if (prev_stack_.empty()) {
    return;
  }
  // First right-click shows the most recent snapshot (index 0)
  prev_index_ = 0;
  if (subscribed_) {
    subscriber_(prev_stack_.front());
  }
}

void StatusHub::update(uint64_t dt_ms) {
  if (prev_index_.has_value()) {
    return; // frozen in history view
  }
  if (decay_overlays(dt_ms)) {
    // Soemthing expired, return to live
    prev_index_.reset();
    rebuild_and_push();
  }
}

void StatusHub::rebuild_and_push() {
  // pick top overlay
  const LiveMsg* best = find_best_overlay();

  if (best) {
    StatusDisplayRecipe overlay;
    const auto& m = best->msg;
    if (std::holds_alternative<std::string>(m.payload)) {
      overlay.add_header(std::get<std::string>(m.payload));
    }
    else {
      overlay = std::get<StatusDisplayRecipe>(m.payload);
    }
    current_ = std::move(overlay); // replace baseline
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

const StatusHub::LiveMsg* StatusHub::find_best_overlay() const {
  if (active_.empty()) {
    return nullptr;
  }

  size_t best_idx = 0;
  for (size_t i = 0; i < active_.size(); ++i) {
    const auto& current = active_[i];
    const auto& best = active_[best_idx];
    if (current.msg.priority > best.msg.priority ||
        (current.msg.priority == best.msg.priority && i > best_idx)) {
      best_idx = i;
    }
  }
  return &active_[best_idx];
}

std::optional<StatusMsg> StatusHub::current_status_msg() const {
  // If we're looking at history, panel is not showing a live StatusMsg
  if (prev_index_.has_value()) {
    return std::nullopt;
  }
  if (active_.empty()) {
    return std::nullopt; // pure baseline
  }

  // Pick the top overlay by priority; tie-break by recency (later wins)
  const LiveMsg* best = find_best_overlay();

  return best ? std::optional<StatusMsg>(best->msg) : std::nullopt;
}
