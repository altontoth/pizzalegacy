/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string_view>
#include <utility>
#include <vector>

#include "engine/geom/Transform.h"
#include "engine/ui/InteractionEvent.h"
#include "engine/ui/InteractionManager.h"

namespace views::ui {

class InteractionLayers {
public:
  using LayerHandle = InteractionManager::LayerHandle;
  using InteractionToken = InteractionManager::InteractionToken;
  using Callback = InteractionManager::Callback;

  // default_priority applies to every item that does not name one, so a whole
  // screen's worth of items can be ranked together.
  InteractionLayers(LayerHandle per_frame, LayerHandle persistent,
                    int default_priority = priority_default,
                    ViewPoint origin = {0, 0})
      : per_frame_(per_frame), persistent_(persistent), origin_(origin),
        default_priority_(default_priority) {}

  void begin_frame() { frame_items_.clear(); }
  void clear_persistent() { persistent_items_.clear(); }

  void clear_persistent(std::string_view id) {
    const auto target = InteractionManager::item_id(id);
    std::erase_if(persistent_items_, [target](const InteractionToken& token) {
      return token.id() == target;
    });
  }

  void origin(ViewPoint o) { origin_ = o; }

  void cancel_drag() { persistent_.manager()->cancel_drag(); }

  InteractionToken& create_frame_item(ViewRect local, std::string_view id,
                                      Callback cb, ItemOptions opts = {}) {
    apply_default_priority(opts);
    frame_items_.push_back(per_frame_.create(local_to_view(local, origin_), id,
                                             std::move(cb), std::move(opts)));
    return frame_items_.back();
  }

  InteractionToken& create_persistent_item(ViewRect local, std::string_view id,
                                           Callback cb, ItemOptions opts = {}) {
    apply_default_priority(opts);
    persistent_items_.push_back(persistent_.create(
        local_to_view(local, origin_), id, std::move(cb), std::move(opts)));
    return persistent_items_.back();
  }

private:
  void apply_default_priority(ItemOptions& opts) const {
    if (!opts.priority) {
      opts.priority = default_priority_;
    }
  }

  LayerHandle per_frame_;
  LayerHandle persistent_;
  ViewPoint origin_;
  int default_priority_;

  std::vector<InteractionToken> frame_items_;
  std::vector<InteractionToken> persistent_items_;
};

struct InteractionContext {
  views::ui::InteractionLayers& layers;
  // call to mark the parent/view dirty:
  std::function<void()> invalidate;
};

} // namespace views::ui
