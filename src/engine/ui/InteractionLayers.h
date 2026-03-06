/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <utility>

#include "engine/geom/Transform.h"
#include "engine/ui/InteractionManager.h"

namespace views::ui {

class InteractionLayers {
public:
  using LayerHandle = InteractionManager::LayerHandle;
  using InteractionToken = InteractionManager::InteractionToken;
  using Callback = InteractionManager::Callback;
  using TooltipCb = std::function<TooltipRequest(const InteractionEvent&)>;

  InteractionLayers(LayerHandle per_frame, LayerHandle persistent,
                    ViewPoint origin = {0, 0})
      : per_frame_(per_frame), persistent_(persistent), origin_(origin) {}

  // Clears out all the per-frame tokens
  void begin_frame() { frame_items_.clear(); }
  //
  void clear_persistent() { persistent_items_.clear(); }

  void origin(ViewPoint o) { origin_ = o; }

  InteractionToken&
  create_frame_item(ViewRect local, std::string_view id, Callback cb,
                    TooltipCb ttcb = {},
                    InteractionLayer layer_type = InteractionLayer::Normal) {
    frame_items_.push_back(per_frame_.create(local_to_view(local, origin_), id,
                                             std::move(cb), std::move(ttcb),
                                             layer_type));
    return frame_items_.back();
  }

  InteractionToken& create_persistent_item(
      ViewRect local, std::string_view id, Callback cb, TooltipCb ttcb = {},
      InteractionLayer layer_type = InteractionLayer::Normal) {
    persistent_items_.push_back(
        persistent_.create(local_to_view(local, origin_), id, std::move(cb),
                           std::move(ttcb), layer_type));
    return persistent_items_.back();
  }

private:
  LayerHandle per_frame_;
  LayerHandle persistent_;
  ViewPoint origin_;

  std::vector<InteractionToken> frame_items_;
  std::vector<InteractionToken> persistent_items_;
};

struct InteractionContext {
  views::ui::InteractionLayers& layers;
  // call to mark the parent/view dirty:
  std::function<void()> invalidate;
};

} // namespace views::ui
