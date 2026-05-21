/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/base/RawRenderable.h"
#include "views/ui/popups/BasePopup.h"
#include "views/ui/popups/PopupButton.h"

class AlertPopup : public BasePopup {
public:
  using callback_t = std::function<void(void)>;

  AlertPopup(const GameContext& ctx, std::string text, callback_t cb = {},
             const RawRenderable* renderable = nullptr);
  void add_interactions(views::ui::InteractionContext ictx,
                        ViewPoint origin) override;
  void render(Surface& surface, ViewPoint origin) override;
  Size popup_size() const override { return {popup_width_, popup_height_}; }

private:
  std::string text_;
  callback_t cb_;
  const RawRenderable* renderable_;
  PopupButton btn_ok_;
  static constexpr int popup_width_ = 380;
  static constexpr int default_popup_height_ = 136;
  static constexpr int text_left_margin_ = 6;
  static constexpr int text_top_margin_ = 16;
  static constexpr int button_bottom_margin_ = 20;
  static constexpr int text_area_width_ = 360;
  static constexpr int button_height_ = 38;
  static constexpr int button_width_ = 74;
  static constexpr int button_x_ = 30;
  static constexpr int text_to_button_gap_ = 8;
  static constexpr int text_color_ = 9;
  int popup_height_{default_popup_height_};
};
