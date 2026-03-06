/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/ui/widgets/CharacterPhotoBox.h"
#include "engine/geom/Spaces.h"
#include "views/base/RawRenderer.h"

void CharacterPhotoBox::draw(Surface& screen, ViewPoint p, bool is_selected) {

  RawRenderer r;
  core::character::CharacterData* character = *character_;
  auto picture = r.surface(*character);
  width_ = picture.width();
  height_ = picture.height();

  screen.blit_raw(picture, p);
}

Size CharacterPhotoBox::measure() const {
  RawRenderer r;
  core::character::CharacterData* character = *character_;
  auto picture = r.surface(*character);
  return Size{picture.width(), picture.height()};
}

void CharacterPhotoBox::handle_action() {}
