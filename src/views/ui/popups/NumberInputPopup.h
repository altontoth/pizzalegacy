/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/input/InputListener.h"
#include "engine/input/TextSession.h"
#include "resources/BobManager.h"
#include "views/ui/popups/BasePopup.h"
#include "views/ui/popups/PopupButton.h"

class NumberInputPopup : public BasePopup {
public:
  using callback_t = std::function<void(int)>;

  NumberInputPopup(const GameContext& context, const std::string& text,
                   const std::string& append_text, int value, int min, int max,
                   int step_size, callback_t cb = nullptr);
  void render(Surface& surface, ViewPoint origin) override;
  Size popup_size() const override { return {popup_width_, popup_height_}; }
  void add_interactions(views::ui::InteractionContext, ViewPoint) override;

  bool on_text(const input::TextEvent& ev) override;
  bool on_action(const input::ActionEvent& ev) override;

private:
  static constexpr ViewRect up_rect = {186, 40, 30, 30};
  static constexpr ViewRect down_rect = {186, 80, 30, 30};
  static constexpr ViewRect ok_rect{250, 58, 72, 36};
  static constexpr ViewRect number_display_rect = {67, 60, 102, 24};

  const GameContext& context_;
  std::string text_;
  std::string append_text_;
  int value_;
  int min_;
  int max_;
  int step_size_;
  callback_t callback_;
  static constexpr int popup_width_ = 362;
  static constexpr int popup_height_ = 136;
  PopupButton btn_up_;
  PopupButton btn_down_;
  PopupButton btn_ok_;

  bool is_editing_ = false;
  std::string edit_buffer_;
  input::TextSession text_session_;

  void start_editing();
  void end_editing(bool accept_value);
  int max_digit_count() const;
  int clamp_input_value(int val) const;
};
