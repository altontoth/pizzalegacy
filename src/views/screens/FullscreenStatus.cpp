/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/screens/FullscreenStatus.h"
#include "core/events/LeventId.h"
#include "core/state/GameState.h"
#include "engine/DisplayConstants.h"
#include "engine/FormatCompat.h"
#include "engine/input/InputManager.h"
#include "engine/ui/InteractionEvent.h"
#include "gfx/Blit.h"
#include "gfx/Color.h"
#include "resources/FontManager.h"

#include <array>
#include <cstdint>

namespace {

struct BgColorEntry {
  bool override_palette;
  uint8_t fg;
  uint8_t shadow;
};

// Indexed by ZwBackground cast to int (0-11). Entry 0 = NONE (unused).
// When override_palette is true, palette slots 0xFD and 0xFE are programmed
// at render time to near-white (RGB 60,60,60) and near-black (RGB 12,12,12).
// This matches the original's sub_3B95E calls with 6-bit values 15/15/15 and
// 3/3/3.
constexpr std::array<BgColorEntry, 12> BG_COLOR_TABLE = {{
    {.override_palette = false, .fg = 0x00, .shadow = 0x00}, // 0: NONE (unused)
    {.override_palette = true, .fg = 0xFD, .shadow = 0xFE},  // 1: BEGGING
    {.override_palette = false, .fg = 0x1F, .shadow = 0x28}, // 2: PRISON
    {.override_palette = true, .fg = 0xFD, .shadow = 0xFE},  // 3: FORCED_LABOR
    {.override_palette = true, .fg = 0xFD, .shadow = 0xFE},  // 4: EXECUTED
    {.override_palette = true, .fg = 0xFD, .shadow = 0xFE},  // 5: BALANCE
    {.override_palette = false, .fg = 0x01, .shadow = 0x02}, // 6: RESTAURANT
    {.override_palette = false, .fg = 0x01, .shadow = 0x0B}, // 7: POSITION
    {.override_palette = true, .fg = 0xFD, .shadow = 0xFE},  // 8: KIDNAPPED
    {.override_palette = true,
     .fg = 0xFD,
     .shadow = 0xFE}, // 9: SWIM_WITH_FISHES
    {.override_palette = true, .fg = 0xFD, .shadow = 0xFE},  // 10: RICH
    {.override_palette = false, .fg = 0x01, .shadow = 0x02}, // 11: PIZZA
}};

} // namespace

FullscreenStatus::FullscreenStatus(GameContext& context, GameState& state,
                                   core::events::LeventId msg_id,
                                   std::function<void()> on_dismiss)
    : View(context, state), typewriter_(context_.font), message_id_(msg_id),
      listener_(context_.input->register_listener(*this)),
      on_dismiss_(std::move(on_dismiss)), background_([&]() -> const Surface& {
        const auto& msg_data = state_.message_manager().get_data(msg_id);
        assert(msg_data.background != core::events::ZwBackground::NONE);
        return context_.vga->get(
            compat::format("gfx/zw{}", static_cast<int>(msg_data.background)));
      }()) {

  text_ = state_.message_manager().get_levent_text(msg_id);

  const auto& msg_data = state_.message_manager().get_data(msg_id);
  const auto bg_index = static_cast<int>(msg_data.background);
  if (bg_index > 0 && bg_index < static_cast<int>(BG_COLOR_TABLE.size())) {
    const auto& entry = BG_COLOR_TABLE[bg_index];
    fg_color_ = entry.fg;
    shadow_color_ = entry.shadow;
    override_palette_ = entry.override_palette;
  }

  constexpr int wrap_width = DisplayConstants::SCREEN_WIDTH - (2 * 30);
  TextStyle ts{.color = fg_color_,
               .align = AlignX::Left,
               .shadow = ShadowStyle{
                   .offset = {-2, 2},
                   .color = shadow_color_,
               }};
  typewriter_.set_text(text_, ViewPoint{30, 320}, FontFace::ASTANDAR, ts,
                       wrap_width);
  typewriter_.set_char_delay(1);
  typewriter_.start();

  interactions_.create_persistent_item(
      ViewRect{0, 0, DisplayConstants::SCREEN_WIDTH,
               DisplayConstants::SCREEN_HEIGHT},
      "fullscreen_status_dismiss", [this](views::ui::InteractionEvent ie) {
        if (ie.type == views::ui::InteractionEvent::Type::Click &&
            ie.button == views::ui::MouseButton::Left) {
          if (typewriter_.is_complete()) {
            dismiss();
          }
          else {
            typewriter_.skip_to_end();
            mark_dirty();
          }
        }
      });
}

void FullscreenStatus::tick() {
  if (typewriter_.tick()) {
    mark_dirty();
  }
}

void FullscreenStatus::render(const RenderContext& ctx) {
  gfx::blit::blit_raw_with_palette(*ctx.surface, background_);
  if (override_palette_) {
    auto& pal = ctx.surface->colors();
    pal[0xFD] = gfx::Color{.r = 240, .g = 240, .b = 240};
    pal[0xFE] = gfx::Color{.r = 48, .g = 48, .b = 48};
  }
  typewriter_.render(*ctx.surface);
}

bool FullscreenStatus::on_action(const input::ActionEvent& ev) {
  if (!ev.state.pressed) {
    return false;
  }

  if (typewriter_.is_complete()) {
    dismiss();
  }
  else {
    typewriter_.skip_to_end();
    mark_dirty();
  }
  return true;
}

void FullscreenStatus::dismiss() {
  state_.ui().pop();
  if (on_dismiss_) {
    on_dismiss_();
  }
}
