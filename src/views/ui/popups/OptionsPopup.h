/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <string>
#include <vector>

#include "views/ui/popups/BasePopup.h"
#include "views/ui/popups/PopupButton.h"

struct Option {
  std::string text;
  std::function<void()> callback;
};

class OptionsPopup : public BasePopup {
public:
  OptionsPopup(const GameContext& context, std::vector<Option> options);
  void add_interactions(views::ui::InteractionContext ictx,
                        ViewPoint origin) override;
  void render(Surface& surface, ViewPoint origin) override;
  Size popup_size() const override { return {popup_width_, popup_height_}; }

private:
  std::vector<Option> options_;
  std::vector<PopupButton> buttons_;
  static constexpr int popup_width_ = 374;
  int popup_height_;
};
