/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/input/InputListener.h"
#include "engine/input/ListenerToken.h"
#include "views/base/RawRenderable.h"
#include "views/ui/popups/BasePopup.h"
#include "views/ui/popups/PopupButton.h"

using gfx::Surface;

class ConfirmPopup : public BasePopup, public input::InputListener {
public:
  using callback_t = std::function<void(bool)>;

  ConfirmPopup(const GameContext& ctx, std::string text, callback_t cb = {},
               const RawRenderable* renderable = nullptr);
  void add_interactions(views::ui::InteractionContext ictx,
                        ViewPoint origin) override;
  void render(Surface& surface, ViewPoint origin) override;
  Size popup_size() const override { return {popup_width_, popup_height_}; }
  bool on_action(const input::ActionEvent& ev) override;

private:
  void confirm(bool yes);

  std::string text_;
  callback_t cb_;
  const RawRenderable* renderable_;
  PopupButton btn_yes_;
  PopupButton btn_no_;
  input::ListenerToken input_;
  static constexpr int popup_width_ = 362;
  static constexpr int popup_height_ = 136;
  static constexpr int text_left_margin_ = 20;
  static constexpr int button_bottom_margin_ = 20;
};
