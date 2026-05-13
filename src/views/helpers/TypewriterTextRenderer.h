#pragma once

#include <functional>
#include <string>

#include "resources/FontManager.h"

// Renders text character-by-character with configurable timing
class TypewriterTextRenderer {
public:
  using CharCallback = std::function<void(char c, int index)>;
  using CompleteCallback = std::function<void()>;

  explicit TypewriterTextRenderer(FontManager* font_manager);

  void set_text(const std::string& text, ViewPoint position, FontFace font,
                uint8_t color, CharCallback on_char = nullptr);

  // Set text with explicit wrap width for proper line breaking
  void set_text(const std::string& text, ViewPoint position, FontFace font,
                TextStyle text_style, int wrap_width,
                CharCallback on_char = nullptr);

  void set_char_delay(int ticks);

  void set_complete_callback(CompleteCallback callback);
  void start();
  void skip_to_end();
  void clear();
  bool tick();
  void render(Surface& surface);
  bool is_complete() const;
  bool is_animating() const;

private:
  enum class State {
    IDLE,
    RENDERING_CHARACTERS,
    COMPLETE,
  };

  FontManager* font_manager_;
  State state_{State::IDLE};

  std::string text_;
  std::string pre_wrapped_text_;
  ViewPoint position_;
  FontFace font_;

  // If set, pre-wrap text at this width
  std::optional<int> wrap_width_;
  TextStyle text_style_;

  CharCallback on_char_rendered_;
  CompleteCallback on_complete_;

  size_t current_char_index_{0};
  int delay_timer_{0};

  int char_delay_ticks_{1};

  // Cached rendering state
  std::string rendered_text_;

  void transition_to_state(State new_state);
  void advance_character();
};
