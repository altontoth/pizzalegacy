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
#include "resources/util/FileUtils.h"
#include "views/ui/PopupManager.h"

constexpr int ticks_per_logo = 50;

Intro::Intro(GameContext& context, GameState& state)
    : View(context, state), busy_cursor_(context),
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
  credit_mapping_.insert(credit_mapping_.end(),
                         {
                             {12, {45}},                     // distribution
                             {13, {}},                       // FOR MICROPROSE
                             {14, {47}},                     // producer
                             {15, {48, 49, 50}},             // graphic artists
                             {16, {51, 52, 53, 54, 55}},     // qa
                             {17, {56, 57, 58, 59, 60, 61}}, // qa
                             {18, {62}},         // additional design
                             {19, {63, 64, 65}}, // translation
                             {20, {66, 67, 68}}, // publishers
                             {21, {69}},         // publishers
                             {70, {71}},         // reimplementation
                         });

  credit_lines_ = fileutils::load_text_lines("t/credits.e");
  credit_lines_.push_back("Open source reimplementation");
  credit_lines_.push_back("Cowomaly");
  interactions_.create_persistent_item(
      ViewRect{0, 0, DisplayConstants::SCREEN_WIDTH,
               DisplayConstants::SCREEN_HEIGHT},
      "intro_catchall", [this](views::ui::InteractionEvent ie) {
        if (ie.type == views::ui::InteractionEvent::Type::Click &&
            ie.button == views::ui::MouseButton::Left) {

          // You have to wait for audio to be initialized to be able to skip
          // the intro.
          if (audio_initialized_) {
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
  // Skipping the intro swallows every key, so ignore input while a popup owns
  // the keyboard.
  if (context_.popup->freezes_view()) {
    return false;
  }
  if (!ev.state.released) {
    return false;
  }
  if (!audio_initialized_) {
    return true; // consume but ignore until audio is ready
  }
  state_.ui().push<ScreenTag>(ScreenTag::Kind::MenuMain);
  return true;
}

void Intro::tick() {
  ++passed_ticks_;

  busy_cursor_.tick();

  // Request on the first tick so the Microprose logo has been rendered and
  // presented at least once before audio initialization stalls the loop.
  if (passed_ticks_ == 1) {
    busy_cursor_.run(
        [this] {
          context_.audio->init();
          context_.audio->load_default_assets();
          audio_initialized_ = true;
        },
        "Loading...");
    mark_dirty();
  }

  // Hold on the first logo until audio is ready.
  if (!audio_initialized_) {
    return;
  }

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
    // Make sure we have a black background
    ctx.surface->fill(gfx::ColorIndex{251});
    gfx::blit::blit_with_palette(*ctx.surface,
                                 context_.vga->get("gfx/mplogolo"));
  } break;
  case Logo::PRESENTS:
    ctx.surface->palette(PaletteHandle{39});
    ctx.surface->fill(gfx::ColorIndex{2});
    context_.font->render_text_onto(
        *ctx.surface, "presents", ViewPoint{ctx.surface->width() / 2, 128},
        FontFace::MENU, text_color, TextAnchor::Middle);
    break;
  case Logo::TITEL1:
    ctx.surface->fill(gfx::ColorIndex{31});
    gfx::blit::blit_with_palette(*ctx.surface, context_.vga->get("gfx/titel1"));
    break;
  case Logo::TITEL2:
    ctx.surface->fill(gfx::ColorIndex{30});
    gfx::blit::blit_with_palette(*ctx.surface, context_.vga->get("gfx/titel2"));
    break;
  case Logo::CYBER:
    ctx.surface->fill(gfx::ColorIndex{15});
    gfx::blit::blit_with_palette(*ctx.surface, context_.vga->get("gfx/cyber"));
    context_.font->render_text_onto(
        *ctx.surface, "V3.0",
        ViewPoint{6, DisplayConstants::SCREEN_HEIGHT - 14}, FontFace::CSTANDAR,
        text_color, TextAnchor::Start);
    break;
  case Logo::CREDITS: {
    ctx.surface->palette(PaletteHandle{39});
    ctx.surface->fill(gfx::ColorIndex{2});
    auto& entry = credit_mapping_[current_credit_line_];
    int x = ctx.surface->width() / 2;
    int y = 128;
    context_.font->render_text_onto(*ctx.surface, credit_lines_[entry.first],
                                    ViewPoint{x, y}, FontFace::MENU, text_color,
                                    TextAnchor::Middle);
    y += 40;
    for (auto line_nr : entry.second) {
      context_.font->render_text_onto(*ctx.surface, credit_lines_[line_nr],
                                      ViewPoint{x, y}, FontFace::MENU,
                                      text_color, TextAnchor::Middle);
      y += 30;
    }
  } break;
  case Logo::NONE:
    break;
  default:
    break;
  }
  busy_cursor_.render_onto(*ctx.surface);
}
