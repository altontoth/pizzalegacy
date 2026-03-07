/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/util/ConfigManager.h"
#include "engine/GameClock.h"
#include "engine/ui/ICursorManager.h"
#include "resources/BobManager.h"
#include "resources/VgaManager.h"

class BlitmapManager;
class PopupManager;
class CursorupManager;
class FontManager;
namespace views::ui {
class InteractionManager;
}
namespace input {
class InputManager;
}
namespace audio {
class AudioManager;
}

class GameContext {
public:
  BobManager* bob{nullptr};
  VgaManager* vga{nullptr};
  FontManager* font{nullptr};
  BlitmapManager* blitmap{nullptr};
  PopupManager* popup{nullptr};
  ICursorManager* cursor{nullptr};
  views::ui::InteractionManager* interaction{nullptr};
  GameClock clock;
  input::InputManager* input{nullptr};
  ConfigManager* config{nullptr};
  audio::AudioManager* audio{nullptr};
};
