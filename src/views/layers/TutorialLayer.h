/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "core/state/TutorialPresenter.h"
#include "core/util/GameContext.h"
#include "engine/geom/Spaces.h"
#include "engine/ui/InteractionEvent.h"
#include "views/base/OverlayBehavior.h"
#include "views/base/View.h"
#include "views/ui/popups/TutorialPopup.h"
#include "views/ui/widgets/PanelDrag.h"

class GameState;

namespace core::tutorial {
struct Step;
}

// Shows the tutorial's current step and limits clicks to what the step allows.
class TutorialLayer : public View,
                      public OverlayBehavior<TutorialLayer>,
                      public TutorialPresenter {
public:
  TutorialLayer(GameContext& context, GameState& state);
  ~TutorialLayer() override;
  TutorialLayer(const TutorialLayer&) = delete;
  TutorialLayer& operator=(const TutorialLayer&) = delete;

  void tick() override;
  void render(const RenderContext& ctx) override;
  bool needs_force_recompose() const override { return pull_recompose_flag(); }

  void show_step(const core::tutorial::Step& step,
                 std::function<void()> on_dismissed) override;
  void tutorial_ended() override;

private:
  void sync();
  void refresh();
  [[nodiscard]] bool popup_up() const;
  // Any popup other than our own skip confirmation.
  [[nodiscard]] bool game_popup_up() const;
  [[nodiscard]] std::optional<ViewRect> game_popup_rect() const;
  [[nodiscard]] bool on_step_screen() const;
  [[nodiscard]] bool filter_active() const;
  [[nodiscard]] bool passes_filter(std::string_view name) const;
  [[nodiscard]] static const std::string*
  target_item(const core::tutorial::Step& step);
  [[nodiscard]] std::optional<ViewRect> find_target() const;
  [[nodiscard]] ViewPoint panel_origin() const;
  [[nodiscard]] ViewPoint tab_origin() const;
  [[nodiscard]] bool offers_skip() const;
  [[nodiscard]] ViewRect skip_rect() const;
  void render_skip(const RenderContext& ctx);
  void on_panel_event(const views::ui::InteractionEvent& ev);
  void dismiss();
  void ask_to_skip();

  std::unique_ptr<TutorialPopup> panel_;
  const core::tutorial::Step* panel_step_{nullptr};
  // Set by show_step, so showing the same step again rebuilds the panel.
  bool rebuild_panel_{false};
  std::function<void()> on_dismissed_;
  std::optional<ViewRect> target_;
  // Number of ticks the target's item has been missing from its own screen.
  int target_missing_ticks_{0};
  bool target_missing_reported_{false};
  int blink_ticks_{0};
  bool blink_on_{true};
  bool minimized_{false};
  std::optional<ViewPoint> dragged_origin_;
  ViewPoint panel_at_;
  PanelDrag panel_drag_;
  bool skip_hovered_{false};
  // Whether a game popup was up at the last tick. The panel moves clear of
  // one.
  bool behind_popup_{false};
  // Our own skip confirmation is up; the panel and target hide behind it.
  bool confirming_skip_{false};
};
