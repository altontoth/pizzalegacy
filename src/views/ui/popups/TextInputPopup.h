/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/DebugLabel.h"
#include "engine/input/InputListener.h"
#include "engine/input/ListenerToken.h"
#include "engine/input/TextSession.h"
#include "views/ui/popups/BasePopup.h"

class TextInputPopup : public BasePopup,
                       public input::InputListener,
                       public debug::AutoLabel<TextInputPopup> {
public:
  using callback_t = std::function<void(std::string)>;

  TextInputPopup(const GameContext& context, std::string text,
                 int max_characters, callback_t cb = nullptr);
  ~TextInputPopup() = default;

  void render(Surface& surface, ViewPoint origin) override;
  Size popup_size() const override { return {popup_width_, popup_height_}; }
  void add_interactions(views::ui::InteractionContext, ViewPoint) override;
  bool on_text(const input::TextEvent& ev) override;
  bool on_action(const input::ActionEvent& ev) override;

private:
  void submit();
  void cancel();
  void backspace();

  std::string prompt_text_{};
  std::string input_text_{};
  int max_characters_{0};
  callback_t callback_;
  static constexpr int popup_width_ = 362;
  static constexpr int popup_height_ = 136;

  input::ListenerToken input_;
  input::TextSession text_;
};
