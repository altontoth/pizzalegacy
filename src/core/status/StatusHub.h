/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include "core/common/Ids.h"
#include "core/status/StatusDisplayRecipe.h"
#include "core/time/GameTime.h"
#include "core/time/GameTimestamp.h"

enum class StatusSeverity { Info, Warning, Error };
enum class StatusScope { Global, City, Restaurant, View };

// How long a message holds the panel, in clock ticks (one per game second).
constexpr int default_display_ticks = 6;

struct StatusMsg {
  // Either raw text banner or a full recipe
  std::variant<std::string, StatusDisplayRecipe> payload;

  StatusSeverity severity = StatusSeverity::Info;
  StatusScope scope = StatusScope::Global;

  uint8_t color = 0;
  std::optional<int> display_ticks{default_display_ticks};
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

  explicit StatusHub(GameTime& time) : time_(&time) {}

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
  // Direct message: goes straight to the panel, replacing whatever is there
  void post_direct(StatusMsg m);
  // Wipe all queued slots (call at day start)
  void clear_queue();
  // Force back to baseline (clears whatever is currently showing). Pending
  // queue slots survive and show when their time comes.
  void clear_overlay();

  // Suppress all posts (jail/holiday: events process but player sees nothing)
  void suppress(bool s) { suppressed_ = s; }

  // Panel interactions
  void show_previous(); // Right-click: replay the last message

  // Header freeze: hold the date/time snapshot shown in the panel header while
  // turn-boundary invoice popups are up. Money stays live via frozen_player().
  void freeze_header(std::optional<core::PlayerId> player);
  void unfreeze_header();
  void refresh_header(); // header inputs changed while frozen, redraw

  // Clear all message/freeze state for a freshly loaded game and rebind to the
  // new GameTime, keeping the current subscriber (the panel outlives the load).
  void reset_for_new_game(GameTime& time);
  bool header_frozen() const noexcept { return header_frozen_; }
  const GameTime& frozen_time() const noexcept { return frozen_time_; }
  std::optional<core::PlayerId> frozen_player() const noexcept {
    return frozen_player_;
  }

  // Counts down the message on the panel, one tick per game second.
  void tick();

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

  struct Showing {
    StatusMsg msg;
    int remaining_ticks;
  };

  void rebuild_and_push();
  void push_to_subscriber();
  // Scan queue_ for the next eligible message and start showing it.
  // Returns true if a message was found and started.
  bool scan_queue();
  void show(StatusMsg m);
  void log_ephemeral(const StatusMsg& m);

  GameTime* time_;

  // State
  BaselineProvider baseline_;
  Subscriber subscriber_;

  // 15-slot queued notification array (indices 0-14)
  static constexpr int QUEUE_SIZE = 15;
  std::array<std::optional<QueueSlot>, QUEUE_SIZE> queue_{};

  std::optional<Showing> showing_;

  StatusDisplayRecipe current_; // last published composite

  // Last message shown (for right-click replay)
  std::optional<StatusMsg> last_shown_msg_;

  // Ephemeral log (longer ring, not for overlay, for future scroll)
  static constexpr size_t log_cap = 256;
  std::deque<EphemeralEntry> ephemeral_log_;

  // Subscription bookkeeping (single-subscriber model)
  bool subscribed_ = false;
  bool suppressed_ = false;

  // Header freeze state (see freeze_header)
  bool header_frozen_ = false;
  std::optional<core::PlayerId> frozen_player_;
  GameTime frozen_time_;
};
