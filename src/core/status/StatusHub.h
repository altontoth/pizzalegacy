/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <chrono>
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include "core/status/StatusDisplayRecipe.h"
#include "core/time/GameTime.h"
#include "core/time/GameTimestamp.h"

enum class StatusSeverity { Info, Warning, Error };
enum class StatusScope { Global, City, Restaurant, View };

struct StatusMsg {
  // Either raw text banner or a full recipe
  std::variant<std::string, StatusDisplayRecipe> payload;

  StatusSeverity severity = StatusSeverity::Info;
  StatusScope scope = StatusScope::Global;

  int color = 0;
  std::optional<std::chrono::milliseconds> ttl{6000};
  // For queued messages: wait this many 10-minute blocks before showing.
  // Converted to an absolute not_before_block at enqueue time.
  int delay_blocks{0};
};

// Snapshot of an event for the future scroll-list UI:
struct EphemeralEntry {
  core::time::GameTimestamp time;
  StatusDisplayRecipe snapshot;
  StatusSeverity severity = StatusSeverity::Info;
  StatusScope scope = StatusScope::Global;
};

class StatusHub {
public:
  using BaselineProvider = std::function<StatusDisplayRecipe()>;
  using Subscriber = std::function<void(StatusDisplayRecipe)>;

  explicit StatusHub(GameTime& time) : time_(time) {}

  class Subscription {
  public:
    Subscription() = default;
    explicit Subscription(std::function<void()> disconnect)
        : disconnect_(std::move(disconnect)) {}

    Subscription(Subscription&& o) noexcept
        : disconnect_(std::move(o.disconnect_)) {
      o.disconnect_ = {};
    }
    Subscription& operator=(Subscription&& o) noexcept {
      if (this != &o) {
        if (disconnect_) {
          disconnect_();
        }
        disconnect_ = std::move(o.disconnect_);
        o.disconnect_ = {};
      }
      return *this;
    }

    ~Subscription() {
      if (disconnect_) {
        disconnect_();
      }
    }

    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;

  private:
    std::function<void()> disconnect_;
  };

  Subscription subscribe(Subscriber s);

  // View lifecycle
  void set_baseline(BaselineProvider p);
  void rebuild_now(); // baseline inputs changed, re-push current composite

  // Queued notification (15-slot array, time-block threshold, yellow)
  void post(StatusMsg m);
  // Direct message (immediate, preempts queued without consuming its slot)
  void post_direct(StatusMsg m);
  // Wipe all queued slots (call at day start)
  void clear_queue();
  // Force back to baseline (clears whatever is currently showing)
  void clear_overlay();

  // Suppress all posts (jail/holiday: events process but player sees nothing)
  void suppress(bool s) { suppressed_ = s; }

  // Panel interactions
  void show_baseline(); // Left-click: dismiss current, show baseline
  void show_previous(); // Right-click: replay last message with fresh TTL

  // Game loop hook
  void update(uint64_t dt_ms);

  // For future scroll list UI
  const std::deque<EphemeralEntry>& ephemeral_log() const noexcept {
    return ephemeral_log_;
  }

private:
  // One slot in the 15-entry queued notification array
  struct QueueSlot {
    StatusMsg msg;
    int not_before_block{0};
  };

  // Currently-showing direct message (preempts queued)
  struct ShowingDirect {
    StatusMsg msg;
    std::chrono::milliseconds remaining_ms;
  };

  // Currently-showing queued message + which slot it occupies
  struct ShowingQueued {
    int slot_idx;
    std::chrono::milliseconds remaining_ms;
  };

  void rebuild_and_push();
  void push_to_subscriber();
  // Scan queue_ for the next eligible message and start showing it.
  // Returns true if a message was found and started.
  bool scan_queue();
  void log_ephemeral(const StatusMsg& m);

  GameTime& time_;

  // State
  BaselineProvider baseline_;
  Subscriber subscriber_;

  // 15-slot queued notification array (indices 0-14)
  static constexpr int QUEUE_SIZE = 15;
  std::array<std::optional<QueueSlot>, QUEUE_SIZE> queue_{};

  std::optional<ShowingDirect> showing_direct_;
  std::optional<ShowingQueued> showing_queued_;

  StatusDisplayRecipe current_; // last published composite

  // Last message shown (for right-click replay)
  std::optional<StatusMsg> last_shown_msg_;

  // Ephemeral log (longer ring, not for overlay, for future scroll)
  static constexpr size_t log_cap = 256;
  std::deque<EphemeralEntry> ephemeral_log_;

  // Subscription bookkeeping (single-subscriber model)
  bool subscribed_ = false;
  bool suppressed_ = false;
};
