/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>
#include <ranges>
#include <vector>
#include "engine/LayerInfo.h"
#include "engine/Renderable.h"
#include "views/base/OverlayBehavior.h"

class OverlayManager {
public:
  template <class T, class... Args>
  T& push(Args&&... args) // T must be Renderable
  {
    static_assert(std::is_base_of_v<Renderable, T>);
    auto obj = std::make_unique<T>(std::forward<Args>(args)...);
    auto& ref = *obj;
    stack_.push_back(std::move(obj));
    return ref;
  }

  void tick_all() {
    for (auto it = stack_.begin(); it != stack_.end();) {
      auto& r = **it;
      r.tick();

      if (auto* ov = dynamic_cast<IOverlay*>(&r)) {
        if (ov->is_finished()) { // remove and restart loop
          it = stack_.erase(it);
          continue;
        }
      }
      ++it;
    }
  }

  void collect_layers(std::vector<LayerInfo>& out) const {
    for (auto& r : stack_) {
      auto layers = r->get_layers();
      out.insert(out.end(), layers.begin(), layers.end());
    }
  }

  bool game_blocked() const {
    return std::any_of(
        stack_.cbegin(), stack_.cend(), [](auto const& p) noexcept {
          if (const IOverlay* ov = dynamic_cast<const IOverlay*>(p.get())) {
            return ov->blocks(); // Overlay vetoes game logic
          }
          return false; // plain Renderable: ignore
        });
  }

  auto renderables() const -> std::vector<Renderable*> {
    using namespace std::views; // transform, ...
    auto ptr_view = stack_ | transform([](auto const& p) { return p.get(); });

    // turn the view into an owning container
    return {ptr_view.begin(), ptr_view.end()};
  }

private:
  std::vector<std::unique_ptr<Renderable>> stack_;
};
