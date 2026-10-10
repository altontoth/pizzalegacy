/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/layers/TutorialLayer.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "core/state/GameState.h"
#include "core/state/TurnOrchestrator.h"
#include "core/tutorial/Tutorial.h"
#include "core/tutorial/TutorialStep.h"
#include "core/util/LegacyConstants.h"
#include "engine/DisplayConstants.h"
#include "engine/Geometry.h"
#include "engine/Logger.h"
#include "engine/Size.h"
#include "engine/geom/TypedRect.h"
#include "engine/gfx/Draw.h"
#include "engine/gfx/PaletteHandle.h"
#include "engine/ui/InteractionEvent.h"
#include "engine/ui/InteractionManager.h"
#include "engine/ui/UiMode.h"
#include "resources/FontManager.h"
#include "resources/FontTypes.h"
#include "views/base/ViewDefs.h"
#include "views/ui/PopupManager.h"
#include "views/ui/popups/ConfirmPopup.h"

namespace {
constexpr int panel_edge_margin = 16;
// How long the target's item is allowed to be missing from its screen before
// we print a warning (items can appear and disappear as screens redraw and
// dropdowns open).
constexpr int missing_target_grace_ticks = 30;
constexpr int blink_period_ticks = 15;
constexpr int highlight_thickness = 3;
constexpr int highlight_gap = 3;
constexpr uint8_t highlight_color = 55;
constexpr uint8_t highlight_dim_color = 240;
constexpr std::string_view keep_playing_label = "Keep playing";
constexpr std::string_view ok_label = "OK";
constexpr std::string_view skip_label = "Skip tutorial";
constexpr std::string_view panel_item = "tutorial_panel";
constexpr std::string_view tab_item = "tutorial_tab";
constexpr std::string_view skip_item = core::tutorial::skip_item;
// In the wooden border above the status panel.
constexpr ViewRect skip_tutorial_position{views::base::STATUS_PANEL_OFFSET_X, 0,
                                          views::base::STATUS_PANEL_WIDTH, 18};
constexpr ViewRect main_view{LegacyConstants::WOOD_BORDER_WIDTH_LEFT,
                             LegacyConstants::WOOD_BORDER_HEIGHT_TOP,
                             views::base::MAIN_PANEL_WIDTH,
                             views::base::MAIN_PANEL_HEIGHT};
constexpr int right_panels_x =
    main_view.x + main_view.w + LegacyConstants::WOOD_BORDER_WIDTH_RIGHT;
constexpr ViewRect status_frame{right_panels_x, 0,
                                DisplayConstants::SCREEN_WIDTH - right_panels_x,
                                views::base::INPUT_PANEL_OFFSET_Y -
                                    views::base::INPUT_PANEL_INNER_OFFSET_Y};
constexpr uint8_t skip_color = 15;
constexpr uint8_t skip_hover_color = 55;
constexpr uint8_t skip_shadow_color = 240;

ViewRect highlight_frame(const ViewRect& target) {
  return target.inset(-highlight_gap, -highlight_gap);
}

int overlap_area(const ViewRect& a, const ViewRect& b) {
  const auto overlap = intersect_rect(a, b);
  return overlap ? overlap->w * overlap->h : 0;
}
} // namespace

TutorialLayer::TutorialLayer(GameContext& context, GameState& state)
    : View(context, state, views::ui::priority_tutorial) {
  context_.interaction->hit_filter(
      [this](std::string_view name) { return passes_filter(name); });
  context_.interaction->on_click(
      [this](std::string_view name, views::ui::MouseButton button) {
        if (button == views::ui::MouseButton::Left) {
          state_.tutorial().item_clicked(name);
        }
      });
}

TutorialLayer::~TutorialLayer() {
  context_.interaction->hit_filter(nullptr);
  context_.interaction->on_click(nullptr);
  if (&state_.tutorial_presenter() == this) {
    state_.tutorial_presenter(nullptr);
  }
}

void TutorialLayer::show_step(const core::tutorial::Step& /*step*/,
                              std::function<void()> on_dismissed) {
  on_dismissed_ = std::move(on_dismissed);
  rebuild_panel_ = true;
  target_missing_ticks_ = 0;
  target_missing_reported_ = false;
  refresh();
}

void TutorialLayer::tutorial_ended() {
  on_dismissed_ = nullptr;
  refresh();
}

void TutorialLayer::refresh() {
  mark_dirty();
  request_recompose();
}

void TutorialLayer::sync() {
  const auto* step = state_.tutorial().current_step();
  if (step == panel_step_ && !rebuild_panel_) {
    return;
  }
  panel_step_ = step;
  rebuild_panel_ = false;
  panel_.reset();
  target_.reset();
  minimized_ = false;
  dragged_origin_.reset();
  skip_hovered_ = false;
  if (!step) {
    hide();
    return;
  }

  using core::tutorial::Completion;
  const bool last = state_.tutorial().on_last_step();
  std::string ok;
  if (step->completion == Completion::Dismissed) {
    ok = last ? keep_playing_label : ok_label;
  }
  panel_ = std::make_unique<TutorialPopup>(
      context_, step->text, std::move(ok),
      TutorialPopup::Callbacks{
          .on_ok = [this] { dismiss(); },
          .on_hide =
              [this] {
                minimized_ = true;
                refresh();
              },
          .on_skip = [this] { ask_to_skip(); },
          .takes_input =
              [this] {
                return !popup_up() &&
                       !state_.turn().is_pipeline_blocking_input();
              },
          .minimized = [this] { return minimized_; },
          .filtered = [this] { return filter_active(); },
      });
  target_ = find_target();
  show();
}

void TutorialLayer::dismiss() {
  // Copy because the callback shows the next step, which replaces
  // on_dismissed_.
  if (auto on_dismissed = on_dismissed_) {
    on_dismissed();
  }
}

void TutorialLayer::ask_to_skip() {
  if (!context_.popup) {
    return;
  }
  confirming_skip_ = true;
  refresh();
  context_.popup->create<ConfirmPopup>(
      "Leave the tutorial? You keep playing this game on your own.",
      [this](bool leave) {
        confirming_skip_ = false;
        refresh();
        if (leave) {
          state_.tutorial().finish();
        }
      });
}

bool TutorialLayer::popup_up() const {
  return context_.popup && context_.popup->freezes_view();
}

bool TutorialLayer::game_popup_up() const {
  return popup_up() && !confirming_skip_;
}

bool TutorialLayer::on_step_screen() const {
  const auto* step = state_.tutorial().current_step();
  return step && step->on_its_screen(state_.ui().top());
}

bool TutorialLayer::filter_active() const {
  const auto* step = state_.tutorial().current_step();
  if (!step || !step->click_filter.active || !on_step_screen()) {
    return false;
  }
  return !popup_up() && !state_.turn().is_pipeline_blocking_input();
}

bool TutorialLayer::passes_filter(std::string_view name) const {
  if (!filter_active()) {
    return true;
  }
  if (name == panel_item || name == tab_item || name == skip_item ||
      name == TutorialPopup::ok_item || name == TutorialPopup::hide_item) {
    return true;
  }
  const auto* step = state_.tutorial().current_step();
  if (const auto* item = target_item(*step); item && *item == name) {
    return true;
  }
  return std::ranges::any_of(
      step->click_filter.allowed_prefixes,
      [name](const std::string& prefix) { return name.starts_with(prefix); });
}

const std::string*
TutorialLayer::target_item(const core::tutorial::Step& step) {
  return step.target ? std::get_if<std::string>(&*step.target) : nullptr;
}

std::optional<ViewRect> TutorialLayer::find_target() const {
  const auto* step = state_.tutorial().current_step();
  if (!step || !step->target || !on_step_screen()) {
    return std::nullopt;
  }
  if (const auto* item = target_item(*step)) {
    // The skip item is only registered during render, so use its rect
    // directly.
    if (*item == skip_item) {
      return offers_skip() ? std::optional{skip_rect()} : std::nullopt;
    }
    return context_.interaction->find_item_rect(*item);
  }
  if (const auto* tile = std::get_if<core::tutorial::MapTile>(&*step->target)) {
    return context_.interaction->find_item_rect(tile->item_name());
  }
  return game_popup_rect();
}

std::optional<ViewRect> TutorialLayer::game_popup_rect() const {
  if (!game_popup_up()) {
    return std::nullopt;
  }
  return context_.popup->active_rect();
}

ViewPoint TutorialLayer::panel_origin() const {
  const Size size = panel_->popup_size();
  const int top = panel_edge_margin;
  const int bottom =
      DisplayConstants::SCREEN_HEIGHT - size.h - panel_edge_margin;

  using core::tutorial::PanelPlace;
  const auto popup = game_popup_rect();
  std::vector<int> rows;
  switch (popup ? PanelPlace::Auto : panel_step_->panel) {
  case PanelPlace::Top:
    rows = {top};
    break;
  case PanelPlace::Bottom:
    rows = {bottom};
    break;
  case PanelPlace::Auto:
    if (popup) {
      rows = {top, bottom};
      break;
    }
    if (!target_) {
      return {(right_panels_x - size.w) / 2,
              (DisplayConstants::SCREEN_HEIGHT - size.h) / 2};
    }
    if (target_->y + target_->h / 2 < DisplayConstants::SCREEN_HEIGHT / 2) {
      rows = {bottom, top};
    }
    else {
      rows = {top, bottom};
    }
    break;
  }
  std::vector<int> columns = {panel_edge_margin, right_panels_x - size.w,
                              DisplayConstants::SCREEN_WIDTH - size.w -
                                  panel_edge_margin};
  if (target_ && target_->x + target_->w / 2 < main_view.x + main_view.w / 2) {
    std::ranges::reverse(columns);
  }

  // The position that hides the least of the target and any popup, then of
  // the status panel, then of the main view.
  const auto frame =
      target_ ? std::optional{highlight_frame(*target_)} : std::nullopt;
  const auto hidden = [&](const ViewRect& rect) {
    std::array<int, 3> area{};
    if (frame) {
      area[0] += overlap_area(rect, *frame);
    }
    if (popup) {
      area[0] += overlap_area(rect, *popup);
    }
    area[1] = overlap_area(rect, status_frame);
    area[2] = overlap_area(rect, main_view);
    return area;
  };
  std::optional<ViewPoint> best;
  std::array<int, 3> best_hidden{};
  for (const int y : rows) {
    for (const int x : columns) {
      const auto area = hidden(ViewRect{x, y, size.w, size.h});
      if (!best || area < best_hidden) {
        best = ViewPoint{x, y};
        best_hidden = area;
      }
    }
  }
  return *best;
}

ViewPoint TutorialLayer::tab_origin() const {
  return {LegacyConstants::WOOD_BORDER_WIDTH_LEFT,
          DisplayConstants::SCREEN_HEIGHT - panel_->tab_size().h};
}

bool TutorialLayer::offers_skip() const {
  // On the last step the panel's Keep playing button already ends the
  // tutorial.
  return state_.tutorial().active() && !state_.tutorial().on_last_step();
}

ViewRect TutorialLayer::skip_rect() const {
  const auto metrics = context_.font->size_text(skip_label, FontFace::CSTANDAR);
  const Size size{metrics.width, metrics.height};
  return {align_in_rect(skip_tutorial_position, size, HAlign::Center,
                        VAlign::Center),
          size};
}

void TutorialLayer::render_skip(const RenderContext& ctx) {
  const ViewRect rect = skip_rect();
  context_.font->render_line_onto(
      *ctx.surface, skip_label, rect.point(), FontFace::CSTANDAR,
      TextStyle{.color = skip_hovered_ ? skip_hover_color : skip_color,
                .shadow =
                    ShadowStyle{.offset = {1, 1}, .color = skip_shadow_color}});
  interactions_.create_frame_item(
      rect, skip_item, [this](const views::ui::InteractionEvent& ev) {
        using Type = views::ui::InteractionEvent::Type;
        if (ev.type == Type::Enter || ev.type == Type::Leave) {
          skip_hovered_ = ev.type == Type::Enter;
          refresh();
        }
        else if (ev.type == Type::Click &&
                 ev.button == views::ui::MouseButton::Left) {
          ask_to_skip();
        }
      });
}

void TutorialLayer::tick() {
  sync();
  state_.tutorial().item_hovered(context_.interaction->hot_item_name());
  if (!panel_) {
    return;
  }

  if (game_popup_up() != behind_popup_) {
    behind_popup_ = !behind_popup_;
    refresh();
  }

  const auto target = find_target();
  if (target != target_) {
    target_ = target;
    refresh();
  }

  // A map tile can be scrolled out of sight and a step without a screen can
  // have its target anywhere, so only warn for the rest.
  const auto* step = state_.tutorial().current_step();
  const auto* item = target_item(*step);
  if (step->screen && item && !target_ && on_step_screen()) {
    ++target_missing_ticks_;
    if (target_missing_ticks_ >= missing_target_grace_ticks &&
        !target_missing_reported_) {
      target_missing_reported_ = true;
      Logger::warn("Tutorial step {} points at '{}', which is not on screen",
                   step->name, *item);
    }
  }
  else {
    target_missing_ticks_ = 0;
  }

  if (target_ && ++blink_ticks_ >= blink_period_ticks) {
    blink_ticks_ = 0;
    blink_on_ = !blink_on_;
    refresh();
  }
}

void TutorialLayer::render(const RenderContext& ctx) {
  sync();
  interactions_.begin_frame();
  if (!panel_ || confirming_skip_) {
    return;
  }
  target_ = find_target();

  const auto palette = context_.ui_mode == UiMode::Menu ? gfx::PaletteHandle{38}
                                                        : gfx::PaletteHandle{0};
  ctx.surface->palette(palette);

  if (target_) {
    gfx::draw::box(*ctx.surface, highlight_frame(*target_),
                   blink_on_ ? highlight_color : highlight_dim_color,
                   highlight_thickness);
  }

  if (offers_skip()) {
    render_skip(ctx);
  }

  if (minimized_) {
    const ViewPoint origin = tab_origin();
    panel_->render_tab(*ctx.surface, origin);
    interactions_.create_frame_item(
        ViewRect{origin, panel_->tab_size()}, tab_item,
        [this](const views::ui::InteractionEvent& ev) {
          if (ev.type == views::ui::InteractionEvent::Type::Click &&
              ev.button == views::ui::MouseButton::Left) {
            minimized_ = false;
            refresh();
          }
        });
    return;
  }

  const ViewPoint origin = dragged_origin_.value_or(panel_origin());
  panel_at_ = origin;
  panel_->render(*ctx.surface, origin);
  interactions_.create_frame_item(
      ViewRect{origin, panel_->popup_size()}, panel_item,
      [this](const views::ui::InteractionEvent& ev) { on_panel_event(ev); },
      {.drag_button = views::ui::MouseButton::Left});
  views::ui::InteractionContext ui{.layers = interactions_,
                                   .invalidate = [this] { refresh(); }};
  panel_->add_interactions(ui, origin);
}

void TutorialLayer::on_panel_event(const views::ui::InteractionEvent& ev) {
  if (!panel_) {
    return;
  }
  const auto to =
      panel_drag_.on_interaction(ev, ViewRect{panel_at_, panel_->popup_size()});
  if (!to) {
    return;
  }
  dragged_origin_ = to;
  refresh();
}
