/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/popups/JukeboxPopup.h"

#include <array>

#include "core/restaurant/Restaurant.h"
#include "engine/audio/AudioManager.h"
#include "engine/geom/Spaces.h"
#include "gfx/Draw.h"

static constexpr std::array<const char*, 7> music_titles = {
    "-------- OFF --------",
    "Tech The Tronic",
    "Jam The Scratch",
    "Heavy Banger",
    "Amore Mio",
    "Elevator Music",
    "Tea Shoppe",
};

// Popup-local rect for row i (0=OFF, 1-6=music tracks).
ViewRect JukeboxPopup::row_rect(int i) { return {46, 44 + (i * 20), 220, 22}; }

JukeboxPopup::JukeboxPopup(const GameContext& ctx, Restaurant& restaurant)
    : BasePopup(ctx), restaurant_(restaurant), audio_(ctx.audio),
      selected_music_(restaurant.jukebox_music()) {}

void JukeboxPopup::add_interactions(views::ui::InteractionContext ictx,
                                    ViewPoint origin) {
  for (int i = 0; i < 7; ++i) {
    ictx.layers.create_frame_item(
        add_origin(row_rect(i), origin), compat::format("jukebox_row_{}", i),
        [this, i,
         inv = ictx.invalidate](const views::ui::InteractionEvent& ev) {
          switch (ev.type) {
          case views::ui::InteractionEvent::Type::Enter:
            hovered_row_ = i;
            inv();
            break;

          case views::ui::InteractionEvent::Type::Leave:
            if (hovered_row_ == i) {
              hovered_row_ = -1;
              inv();
            }
            break;

          case views::ui::InteractionEvent::Type::Click:
            restaurant_.jukebox_music(i);
            if (i == 0) {
              if (audio_) {
                audio_->stop_music();
              }
            }
            else {
              if (audio_) {
                audio_->play_music(static_cast<audio::MusicId>(i - 1));
              }
            }
            finished();
            break;

          default:
            break;
          }
        });
  }
}

void JukeboxPopup::render(Surface& surface, ViewPoint origin) {
  const Size box_size{288, 226};
  const ViewRect outer{origin, box_size};
  surface.fill_rect(outer, 15);
  const int c1 = 17;
  const int c2 = 16;
  const int c3 = 18;
  gfx::draw::box(surface, outer.inset(0, 0), c1, 2);
  gfx::draw::box(surface, outer.inset(2, 2), c2, 2);
  gfx::draw::box(surface, outer.inset(4, 4), c1, 2);
  gfx::draw::box(surface, outer.inset(6, 6), c3, 2);

  const PxOffset title_offset{box_size.w / 2, 18};
  font_->render_line_onto(surface, "Music Selection", origin + title_offset,
                          FontFace::ASTANDAR, 240, AlignX::CenterPoint);

  for (int i = 0; i < static_cast<int>(music_titles.size()); ++i) {
    const ViewRect local = row_rect(i);
    const ViewRect screen_row = add_origin(local, origin);
    const bool selected = (i == selected_music_);
    const bool hovered = (i == hovered_row_);

    if (selected) {
      gfx::draw::box(surface, screen_row, 20, 1);
    }
    if (hovered) {
      surface.fill_rect(screen_row.inset(2, 2), 13);
    }

    const uint8_t text_color = 240;
    font_->render_line_onto(surface, music_titles[i],
                            origin + PxOffset{local.x + 4, local.y + 4},
                            FontFace::PIZ_NUMB, text_color);
  }

  const PxOffset please_select_offset{50, 200};
  font_->render_line_onto(surface, "Please select...",
                          origin + please_select_offset, FontFace::PIZ_NUMB,
                          14);
}
