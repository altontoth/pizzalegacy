/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cassert>
#include <cstdint>
#include <deque>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/util/GameContext.h"
#include "engine/DebugConsole.h"
#include "engine/DisplayConstants.h"
#include "engine/FormatCompat.h"
#include "engine/geom/Spaces.h"
#include "engine/ui/InteractionEvent.h"
#include "engine/ui/TooltipManager.h"
#include "engine/ui/TooltipRequest.h"

union SDL_Event;
class DebugLayer;
class DebugCommands;
class GameState;

/*
 InteractionManager - centralized pointer-interaction system.

 * Register ViewRects via LayerHandle::create(), with anything beyond the rect,
   id and callback named in an ItemOptions.
 * Feed SDL events to process().
 * Per rectangle you receive InteractionEvent callbacks:
    Enter, Leave, Hover,
    ButtonDown / ButtonUp / Click / Cancelled
 * Handles multiple buttons (left / right) and simultaneous presses.
 * InteractionToken is RAII: destroying it deregisters the item.
 * Items are ranked by priority, then by creation order within a priority.
 * InteractionLayer:
    - Normal items block events from reaching items below them
    - Background items always receive events, even if Normal items are on top
*/

namespace views::ui {

enum class InteractionLayer {
  Normal,     // blocks events to Normal items below
  Background, // always receives events, doesn't block others
  Exclusive   // blocks ALL events to items below (including Background)
};

// Interaction priorities; higher sits on top. An item only has to name one of
// these when it needs to outrank items registered after it. An Exclusive item
// that has to survive another view re-registering its items needs a priority
// above priority_default.
constexpr int priority_default = 0;
constexpr int priority_turn_lock = 100;
constexpr int priority_popup = 200;

using TooltipCb = std::function<TooltipRequest(const InteractionEvent&)>;

// Everything about an item beyond its rect, id and callback.
struct ItemOptions {
  TooltipCb tooltip;
  InteractionLayer layer = InteractionLayer::Normal;
  // Which button is allowed to start a drag, if any.
  std::optional<MouseButton> drag_button;
  // Unset takes the priority of the InteractionLayers that creates the item.
  std::optional<int> priority;
};

class InteractionManager {
public:
  using Callback = std::function<void(const InteractionEvent&)>;
  using LayerID = uint32_t;
  using ItemID = uint32_t;

  [[nodiscard]] static ItemID item_id(std::string_view id_str) noexcept {
    return static_cast<ItemID>(std::hash<std::string_view>{}(id_str));
  }

  InteractionManager(TooltipManager& tt_mgr,
                     int display_height = DisplayConstants::LOGICAL_HEIGHT)
      : tt_mgr_(tt_mgr), display_height_(display_height) {}
  void update_mouse(ScreenPoint sp);
  void set_display_height(int height) { display_height_ = height; }
  void clear() {
    items_.clear();
    current_hover_ = 0;
    hot_item_name_.clear();
  }
  void dump_layers(std::ostream& os) const;

  class InteractionToken {
  public:
    InteractionToken() = default;
    InteractionToken(InteractionToken&& other) noexcept {
      *this = std::move(other);
    }
    InteractionToken& operator=(InteractionToken&& other) noexcept {
      release();
      mgr_ = other.mgr_;
      id_ = other.id_;
      other.mgr_ = nullptr;
      return *this;
    }
    InteractionToken(const InteractionToken&) = delete;
    InteractionToken& operator=(const InteractionToken&) = delete;
    [[nodiscard]] ItemID id() const noexcept { return id_; }

    ~InteractionToken() { release(); }

    void reset() noexcept { release(); }

  private:
    friend class InteractionManager;
    InteractionToken(InteractionManager* m, ItemID id) noexcept
        : mgr_{m}, id_{id} {}

    void release() noexcept {
      if (mgr_) {
        mgr_->erase_item(id_);
      }
      mgr_ = nullptr;
    }

    InteractionManager* mgr_{nullptr};
    ItemID id_{0};
  };

  class LayerHandle {
  public:
    LayerHandle() = default;
    LayerHandle(const LayerHandle&) = default;
    LayerHandle& operator=(const LayerHandle&) = default;

    [[nodiscard]]
    InteractionToken create(ViewRect r, std::string_view id_str, Callback cb,
                            ItemOptions opts = {}) const {
      return mgr_->create_item(r, id_str, std::move(cb), id_, std::move(opts));
    }

    [[nodiscard]] LayerID id() const noexcept { return id_; }

  private:
    friend class InteractionManager;
    // InteractionLayers need to be able to forward drag-control calls to the
    // manager.
    friend class InteractionLayers;
    [[nodiscard]] InteractionManager* manager() const noexcept { return mgr_; }
    LayerHandle(InteractionManager* m, LayerID id) : mgr_{m}, id_{id} {}

    InteractionManager* mgr_{nullptr};
    LayerID id_{0};
  };

  [[nodiscard]] LayerHandle make_layer() {
    return LayerHandle{this, next_layer_id_++};
  }

  void process(const SDL_Event& e);
  void on_hot_changed(std::function<void()> cb) {
    on_hot_changed_ = std::move(cb);
  }

  bool is_dragging() const { return drag_active_; }

  void cancel_drag() {
    drag_active_ = false;
    drag_source_ = 0;
  }

  void end_active_drag() {
    if (drag_active_) {
      finish_drag(drag_last_pos_);
    }
  }

private:
  friend class ::DebugLayer;
  friend void register_debug_commands(DebugConsole&, GameState&, GameContext&);

  struct InteractionItem {
    ItemID id;
    LayerID layer;
    ViewRect rect;
    std::string name;
    Callback callback;
    TooltipCb tooltip_cb = {};
    InteractionLayer layer_type = InteractionLayer::Normal;
    // Which button is allowed to start a drag (if any):
    std::optional<MouseButton> drag_button;
    int priority = priority_default;
    // Creation order, breaking ties within a priority.
    uint64_t seq = 0;

    bool hovered = false;
    bool hovered_prev = false;
    bool active_left = false;
    bool active_right = false;
  };

  // Per-button tracking for Click/Cancelled logic
  ItemID pressed_left_id_{0};
  ItemID pressed_right_id_{0};

  [[nodiscard]]
  InteractionToken create_item(ViewRect r, std::string_view id_str, Callback cb,
                               LayerID layer, ItemOptions opts = {}) {
    ItemID id = item_id(id_str);
#ifdef DEBUG
    for (const auto& item : items_) {
      if (item.id == id) {
        std::cerr << compat::format("Duplicate interaction id {} (name='{}')\n",
                                    id, item.name);
        assert(false && "Duplicate interaction id");
      }
    }
#endif
    items_.push_front({id, layer, r, std::string{id_str}, std::move(cb),
                       std::move(opts.tooltip), opts.layer, opts.drag_button,
                       opts.priority.value_or(priority_default), next_seq_++});
    return InteractionToken{this, id};
  }

  void erase_item(ItemID id) noexcept {
    auto it = std::find_if(items_.begin(), items_.end(),
                           [id](const auto& itm) { return itm.id == id; });
    if (it != items_.end()) {
      items_.erase(it);
    }
  }

  // Where an item sits in the stacking order.
  struct Rank {
    int priority = priority_default;
    uint64_t seq = 0;

    [[nodiscard]] bool outranks(Rank other) const noexcept {
      if (priority != other.priority) {
        return priority > other.priority;
      }
      return seq > other.seq;
    }
  };
  [[nodiscard]] static Rank rank_of(const InteractionItem& it) noexcept {
    return Rank{.priority = it.priority, .seq = it.seq};
  }

  // What sits under the pointer.
  struct HitResult {
    // Topmost Normal or Exclusive item.
    ItemID normal = 0;
    // Background items ranked above any Exclusive item, topmost first.
    std::vector<ItemID> background;
    // Rank of the topmost Exclusive item under the pointer.
    std::optional<Rank> blocker;
  };
  [[nodiscard]] HitResult hit_test(ViewPoint p) const;

  [[nodiscard]] static MouseButton to_button(const SDL_Event& e) noexcept;
  void handle_tooltip(const InteractionItem& it, const InteractionEvent& ev);
  void dispatch(const InteractionItem& item, InteractionEvent::Type type,
                MouseButton btn, ViewPoint p, ViewPoint delta = {}) const;
  void finish_drag(ViewPoint p);
  const std::deque<InteractionItem>& items() const { return items_; }
  void dispatch_hover_enter_leave(InteractionItem& it, ViewPoint p);
  InteractionItem* lookup_item(ItemID id) {
    for (auto& it : items_) {
      if (it.id == id) {
        return &it;
      }
    }
    return nullptr;
  }
  std::string hot_item_name() const { return hot_item_name_; }
  ViewPoint screen_to_view_point(ScreenPoint sp) const;

  TooltipManager& tt_mgr_;
  std::deque<InteractionItem> items_;
  LayerID next_layer_id_{1};
  uint64_t next_seq_{1};
  ItemID current_hover_{0};
  std::string hot_item_name_;
  std::function<void()> on_hot_changed_;
  int display_height_{DisplayConstants::LOGICAL_HEIGHT};
  ViewPoint last_logical_mouse_pos_;

  static constexpr int DRAG_THRESHOLD = 4;
  ItemID drag_source_{0};
  MouseButton drag_button_{MouseButton::Left};
  bool drag_active_{false};
  ViewPoint drag_start_pos_;
  ViewPoint drag_last_pos_;
};

} // namespace views::ui
