/*
 * SPDX-FileCopyrightText: 2010 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include <vector>
#include "engine/DebugLabel.h"
#include "engine/audio/AudioManager.h"
#include "engine/input/InputListener.h"
#include "engine/input/InputManager.h"
#include "engine/input/ListenerToken.h"
#include "resources/MFileLoader.h"
#include "views/base/View.h"

namespace audio {
class AudioManager;
}

struct AudioAssetInfo {
  std::string file_name;
  AudioAssetType type;
  int asset_index;   // Index within the M file
  size_t slot_index; // Slot in AudioManager (for playback)
};

class SoundViewer final : public View,
                          public input::InputListener,
                          public debug::AutoLabel<SoundViewer> {
public:
  SoundViewer(GameContext& context, GameState& state);
  void tick() override;
  void render(const RenderContext& ctx) override;
  bool on_action(const input::ActionEvent& ev) override;

private:
  void load_all_m_files();
  void play_current();
  void stop_current();

  audio::AudioManager* audio_;
  std::vector<AudioAssetInfo> assets_;
  int current_index_;
  int currently_playing_index_; // -1 if nothing playing
  input::ListenerToken listener_;
};
