/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/Intro.h"

#include "core/state/GameState.h"
#include "engine/DisplayConstants.h"
#include "engine/audio/AudioManager.h"
#include "engine/input/InputManager.h"
#include "engine/ui/InteractionEvent.h"
#include "gfx/Blit.h"
#include "gfx/ColorIndex.h"
#include "resources/FontManager.h"
#include "resources/util/Platform.h"

constexpr int ticks_per_logo = 50;

Intro::Intro(GameContext& context, GameState& state)
    : View(context, state),
      listener_(context_.input->register_listener(*this)) {

  // std::vector<std::pair<int, std::vector<int>>> credit_mapping_;
  credit_mapping_ = {
      {0, {24, 25, 26, 27, 28}}, // idea & outline
      {1, {29}},                 // programming
      {2, {30, 31}},             // graphics
      {3, {31}},                 // sound
      {4, {32, 33, 34}},         // maps
      {5, {35}},                 // characters
      {6, {36}},                 // newspaper reports
      {7, {37}},                 // special animation
      {8, {38}},                 // main tester
      {9, {39, 40, 41, 42}},     // testers
      {10, {43}},                // pc conversion
      {11, {44}},                // pc graphics
      {12, {45}},                // pc graphics
      {46, {47}},                // reimplementation
  };
  if (!platform::is_amiga()) {
    credit_mapping_.insert(credit_mapping_.end(),
                           {
                               {12, {45}},                 // distribution
                               {13, {}},                   // FOR MICROPROSE
                               {14, {47}},                 // producer
                               {15, {48, 49, 50}},         // graphic artists
                               {16, {51, 52, 53, 54, 55}}, // qa
                               {17, {56, 57, 58, 59, 60, 61}}, // qa
                               {18, {62}},         // additional design
                               {19, {63, 64, 65}}, // translation
                               {20, {66, 67, 68}}, // publishers
                               {21, {69}},         // publishers
                               {70, {71}},         // reimplementation
                           });
  }

  std::ifstream file = fileutils::open("t/credits.e");
  // Read the file line-by-line.
  std::string line;
  while (std::getline(file, line)) {
    credit_lines_.push_back(line);
  }
  credit_lines_.push_back("Open source reimplementation");
  credit_lines_.push_back("Cowomaly");
  interactions_.create_persistent_item(
      ViewRect{0, 0, DisplayConstants::SCREEN_WIDTH,
               DisplayConstants::SCREEN_HEIGHT},
      "intro_catchall", [this](views::ui::InteractionEvent ie) {
        if (ie.type == views::ui::InteractionEvent::Type::Click &&
            ie.button == views::ui::MouseButton::Left) {
          // Bit annoying but matches the behavior of the original roughly,
          // if you don't like it there is now the life improvement option
          if (passed_ticks_ > 50 ||
              context_.config->options().qol_improvements) {
            state_.ui().push<ScreenTag>(ScreenTag::Kind::MenuMain);
          }
        }
      });
}

Intro::~Intro() {
  if (context_.audio->is_playing()) {
    context_.audio->stop_music();
  }
}

bool Intro::on_action(const input::ActionEvent& ev) {
  if (!ev.state.pressed) {
    return false;
  }
  state_.ui().push<ScreenTag>(ScreenTag::Kind::MenuMain);
  return true;
}

void Intro::tick() {
  ++passed_ticks_;

  // Every ticks_per_logo ticks, update either the logo or the credits.
  if (logo_sequence_[current_logo_index_] == Logo::NONE ||
      (passed_ticks_ % ticks_per_logo == 0)) {
    // If we haven't reached the last logo in the sequence, advance to the next
    // one.
    if (current_logo_index_ < logo_sequence_.size() - 1) {
      ++current_logo_index_;
      mark_dirty();
    }
    else {
      // Once we hit the CREDITS logo (last in the sequence), update the credit
      // index.
      current_credit_line_ =
          (current_credit_line_ + 1) % credit_mapping_.size();
      mark_dirty();
    }
    // Start music after microprose logo
    if (current_logo_index_ > 0 && !context_.audio->is_playing()) {
      context_.audio->play_music(audio::MusicId::TEA_SHOPPE);
    }
  }
}

void Intro::render(const RenderContext& ctx) {
  const uint8_t text_color = 1;
  switch (logo_sequence_[current_logo_index_]) {
  case Logo::MPLOGO: {
    gfx::blit::blit_with_palette(*ctx.surface,
                                 context_.vga->get("gfx/mplogolo"));
  } break;
  case Logo::PRESENTS:
    ctx.surface->palette(PaletteHandle{39});
    ctx.surface->fill(gfx::ColorIndex{2});
    context_.font->render_text_onto(*ctx.surface, "presents", ViewPoint{0, 128},
                                    FontFace::MENU, text_color, AlignX::Center);
    break;
  case Logo::TITEL1:
    ctx.surface->fill(gfx::ColorIndex{24});
    gfx::blit::blit_with_palette(*ctx.surface, context_.vga->get("gfx/titel1"));
    break;
  case Logo::TITEL2:
    ctx.surface->fill(gfx::ColorIndex{30});
    gfx::blit::blit_with_palette(*ctx.surface, context_.vga->get("gfx/titel2"));
    break;
  case Logo::CYBER:
    ctx.surface->fill(gfx::ColorIndex{16});
    gfx::blit::blit_with_palette(*ctx.surface, context_.vga->get("gfx/cyber"));
    context_.font->render_text_onto(
        *ctx.surface, "V3.0",
        ViewPoint{6, DisplayConstants::SCREEN_HEIGHT - 14}, FontFace::CSTANDAR,
        text_color, AlignX::Left);
    break;
  case Logo::CREDITS: {
    ctx.surface->palette(PaletteHandle{39});
    ctx.surface->fill(gfx::ColorIndex{2}); // Same index for both DOS and Amiga
    auto& entry = credit_mapping_[current_credit_line_];
    int x = 0;
    int y = 128;
    context_.font->render_text_onto(*ctx.surface, credit_lines_[entry.first],
                                    ViewPoint{x, y}, FontFace::MENU, text_color,
                                    AlignX::Center);
    y += 40;
    for (auto line_nr : entry.second) {
      context_.font->render_text_onto(*ctx.surface, credit_lines_[line_nr],
                                      ViewPoint{x, y}, FontFace::MENU,
                                      text_color, AlignX::Center);
      y += 30;
    }
  } break;
  case Logo::NONE:
    break;
  default:
    break;
  }
}
