/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <chrono>
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "core/status/StatusDisplayRecipe.h"
#include "core/time/GameTime.h"
#include "core/time/GameTimestamp.h"

enum class StatusSeverity { Info, Warning, Error };
enum class StatusScope { Global, City, Restaurant, View };

// This is an instruction/event: show this payload right now:
struct StatusMsg {
  // Either raw text banner or a fragment recipe to append
  std::variant<std::string, StatusDisplayRecipe> payload;

  StatusSeverity severity = StatusSeverity::Info;
  StatusScope scope = StatusScope::Global;

  std::optional<std::chrono::milliseconds> ttl{6000};
  int delay_blocks{
      0}; // wait this many 10-minute game-time blocks before showing
  std::string coalesce_key; // replace existing with same key
  int priority = 0;
};

// Snapshot of an event we showed in the past:
struct EphemeralEntry {
  core::time::GameTimestamp time;
  StatusDisplayRecipe snapshot; // what was shown when posted
  StatusSeverity severity = StatusSeverity::Info;
  StatusScope scope = StatusScope::Global;
  std::string key;
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
  void rebuild_now(); // baseline inputs changed

  // Transients
  void post(StatusMsg m); // Temporary or Ephemeral
  void clear_overlay();   // Force only baseline

  // Suppress all posts (jail/holiday: events still process but player
  // does not see the messages)
  void suppress(bool s) { suppressed_ = s; }

  // Panel interactions
  void show_baseline(); // Left-click
  void show_previous(); // Right-click (one step back)

  // Game loop hook to expire TTLs
  void update(uint64_t dt_ms);

  // For future scroll list UI
  const std::deque<EphemeralEntry>& ephemeral_log() const noexcept {
    return ephemeral_log_;
  }

  [[nodiscard]] std::optional<StatusMsg> current_status_msg() const;

private:
  struct LiveMsg {
    StatusMsg msg;
    std::optional<std::chrono::milliseconds> remaining_ms;
  };

  struct PendingMsg {
    StatusMsg msg;
    int not_before_block{0}; // absolute game-time block threshold
  };

  void rebuild_and_push(); // baseline + active overlay -> current_
  void push_to_subscriber();
  void insert_or_replace(StatusMsg m);
  bool decay_overlays(std::chrono::milliseconds dt);
  bool decay_overlays(uint64_t dt_ms);
  const LiveMsg* find_best_overlay() const;

  GameTime& time_;

  // State
  BaselineProvider baseline_;
  Subscriber subscriber_;

  std::vector<LiveMsg> active_; // currently overlaying (TTL-managed)
  std::deque<PendingMsg>
      pending_;                 // queued, waiting for active_ to drain + delay
  StatusDisplayRecipe current_; // last published composite

  // Right-click "previous" history (short ring)
  static constexpr size_t prev_cap = 8;
  std::deque<StatusDisplayRecipe> prev_stack_; // snapshots of composites
  std::optional<int> prev_index_;              // => live view if not set

  // Ephemeral log (longer ring, not for overlay, for future scroll)
  static constexpr size_t log_cap = 256;
  std::deque<EphemeralEntry> ephemeral_log_;

  // Subscription bookkeeping (simple one-subscriber model; extend if needed)
  bool subscribed_ = false;
  bool suppressed_ = false;
};
