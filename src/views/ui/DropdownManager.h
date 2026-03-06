/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <optional>
#include <unordered_map>
#include "engine/geom/Spaces.h"
#include "engine/ui/InteractionLayers.h"
#include "views/ui/DropdownList.h"
#include "views/ui/widgets/IAddsInteractions.h"

template <typename DropdownType>
class DropdownManager : public IAddsInteractions {
  struct DropdownContext {
    views::ui::InteractionContext dd_ctx;
    std::function<void()> invalidate;
    std::function<void()> request_toggle;
    std::function<void()> request_close_active;
  };

public:
  void add_dropdown(
      DropdownType type, ViewRect header, ViewPoint origin,
      std::optional<std::function<void()>> /*on_activate*/ = std::nullopt);
  void add_item(DropdownType type, std::string_view text,
                std::function<void()> on_click);
  void render(Surface& target_surface, FontManager* font);
  bool tick();
  void set_expanded(DropdownList* expanded);
  void add_interactions(views::ui::InteractionContext&);

private:
  std::unordered_map<DropdownType, DropdownList> dropdowns_;
  std::optional<DropdownType> active_;
};

#include "engine/geom/Spaces.h"
#include "views/ui/DropdownManager.inl"
