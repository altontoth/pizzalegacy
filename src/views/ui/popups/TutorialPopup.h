/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#include "engine/Size.h"
#include "engine/geom/Spaces.h"
#include "engine/input/Action.h"
#include "engine/ui/InteractionLayers.h"
#include "views/ui/popups/BasePopup.h"
#include "views/ui/popups/PopupButton.h"

class TutorialPopup : public BasePopup {
public:
  static constexpr std::string_view ok_item = "tutorial_ok";
  static constexpr std::string_view hide_item = "tutorial_hide";

  struct Callbacks {
    std::function<void()> on_ok;
    std::function<void()> on_hide;
    std::function<void()> on_skip;
    std::function<bool()> takes_input;
    std::function<bool()> minimized;
    // Whether the step's click filter is active. Keys that would act on the
    // screen behind are dropped while it is.
    std::function<bool()> filtered;
  };

  // An empty ok_label means no OK button.
  TutorialPopup(const GameContext& context, std::string text,
                std::string ok_label, Callbacks callbacks);

  void render(Surface& surface, ViewPoint origin) override;
  Size popup_size() const override { return {width_, height_}; }
  void add_interactions(views::ui::InteractionContext ictx,
                        ViewPoint origin) override;
  bool on_action(const input::ActionEvent& ev) override;

  [[nodiscard]] Size tab_size() const;
  void render_tab(Surface& surface, ViewPoint origin);

  static constexpr int title_height = 20;

private:
  enum class Chevron { Down, Up };

  static bool reaches_screen_while_filtered(input::Action action);
  int button_width(const std::string& label) const;
  int fit_width(int buttons_width) const;
  Surface chrome_surface(Size size, Chevron chevron) const;
  [[nodiscard]] static ViewRect chevron_rect(int width);

  // Minimum and maximum widths.
  static constexpr int min_width = 280;
  static constexpr int max_width = 400;
  static constexpr int width_step = 40;
  static constexpr int max_lines = 4;
  static constexpr int border = 2;
  static constexpr int text_margin = 10;
  static constexpr int text_to_button_gap = 6;
  static constexpr int button_height = 26;
  static constexpr int min_button_width = 48;
  static constexpr int button_padding = 16;
  static constexpr int button_margin = 10;
  static constexpr int title_margin = 8;
  static constexpr int text_color = 9;
  static constexpr int fill_color = 14;
  static constexpr int border_color = 11;
  static constexpr uint8_t title_color = 15;
  static constexpr uint8_t chevron_hover_color = 55;

  std::string text_;
  bool has_ok_;
  Callbacks callbacks_;
  PopupButton ok_;
  bool chevron_hovered_{false};
  int width_{max_width};
  int height_{0};
};
