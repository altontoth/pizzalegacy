#include "TypewriterTextRenderer.h"

#include <utility>

TypewriterTextRenderer::TypewriterTextRenderer(FontManager* font_manager)
    : font_manager_(font_manager) {}

void TypewriterTextRenderer::set_text(const std::string& text,
                                      ViewPoint position, FontFace font,
                                      uint8_t color, CharCallback on_char) {
  text_ = text;
  position_ = position;
  font_ = font;
  text_style_ = TextStyle{.color = color};
  on_char_rendered_ = std::move(on_char);
  rendered_text_.clear();
  current_char_index_ = 0;
  pre_wrapped_text_.clear();
}

void TypewriterTextRenderer::set_text(const std::string& text,
                                      ViewPoint position, FontFace font,
                                      TextStyle style, int wrap_width,
                                      CharCallback on_char) {
  text_ = text;
  position_ = position;
  font_ = font;
  text_style_ = style;
  wrap_width_ = wrap_width;
  on_char_rendered_ = std::move(on_char);
  rendered_text_.clear();
  current_char_index_ = 0;

  // Pre-wrap the full text
  auto& font_obj = font_manager_->get(font);
  auto wrapped_lines = font_obj.split_and_wrap_text(text, wrap_width);

  // Reconstruct text with explicit line breaks
  pre_wrapped_text_.clear();
  for (size_t i = 0; i < wrapped_lines.size(); ++i) {
    pre_wrapped_text_ += wrapped_lines[i];
    if (i < wrapped_lines.size() - 1) {
      pre_wrapped_text_ += '|'; // line break
    }
  }
}

void TypewriterTextRenderer::set_char_delay(int ticks) {
  char_delay_ticks_ = ticks;
}

void TypewriterTextRenderer::set_complete_callback(CompleteCallback callback) {
  on_complete_ = std::move(callback);
}

void TypewriterTextRenderer::start() {
  const auto& source_text = wrap_width_ ? pre_wrapped_text_ : text_;
  if (source_text.empty()) {
    state_ = State::COMPLETE;
    return;
  }

  current_char_index_ = 0;
  rendered_text_.clear();
  delay_timer_ = char_delay_ticks_;
  transition_to_state(State::RENDERING_CHARACTERS);
}

void TypewriterTextRenderer::skip_to_end() {
  if (state_ == State::COMPLETE || state_ == State::IDLE) {
    return;
  }
  const auto& source_text = wrap_width_ ? pre_wrapped_text_ : text_;
  rendered_text_ = source_text;
  current_char_index_ = source_text.size();
  transition_to_state(State::COMPLETE);
}

void TypewriterTextRenderer::clear() {
  text_.clear();
  rendered_text_.clear();
  current_char_index_ = 0;
  delay_timer_ = 0;
  on_char_rendered_ = nullptr;
  on_complete_ = nullptr;
  state_ = State::IDLE;
}

bool TypewriterTextRenderer::tick() {
  if (state_ == State::IDLE || state_ == State::COMPLETE) {
    return false;
  }

  bool dirty = false;

  // Handle delay timer
  if (delay_timer_ > 0) {
    --delay_timer_;
  }

  // Process state when timer expires
  if (delay_timer_ == 0 && state_ == State::RENDERING_CHARACTERS) {
    advance_character();
    dirty = true;
  }

  return dirty;
}

void TypewriterTextRenderer::render(Surface& surface) {
  if (!rendered_text_.empty()) {
    // Simply render to the surface - if we pre-wrapped, the '|' characters
    // will cause line breaks at the correct positions
    font_manager_->render_text_onto(surface, rendered_text_, position_, font_,
                                    text_style_);
  }
}

bool TypewriterTextRenderer::is_complete() const {
  return state_ == State::COMPLETE;
}

bool TypewriterTextRenderer::is_animating() const {
  return state_ == State::RENDERING_CHARACTERS;
}

void TypewriterTextRenderer::transition_to_state(State new_state) {
  state_ = new_state;
}

void TypewriterTextRenderer::advance_character() {
  const auto& source_text = wrap_width_ ? pre_wrapped_text_ : text_;

  if (current_char_index_ < source_text.size()) {
    char c = source_text[current_char_index_];
    rendered_text_ += c;

    // Call per-character callback
    if (on_char_rendered_) {
      on_char_rendered_(c, static_cast<int>(current_char_index_));
    }

    ++current_char_index_;
    delay_timer_ = char_delay_ticks_;

    // Check if complete
    if (current_char_index_ >= source_text.size()) {
      transition_to_state(State::COMPLETE);

      if (on_complete_) {
        on_complete_();
      }
    }
  }
}
