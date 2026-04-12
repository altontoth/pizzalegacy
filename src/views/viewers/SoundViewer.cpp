/*
 * SPDX-FileCopyrightText: 2024 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "views/viewers/SoundViewer.h"
#include <magic_enum/magic_enum.hpp>
#include "engine/Logger.h"
#include "engine/audio/AudioManager.h"
#include "resources/FontManager.h"
#include "resources/MFileLoader.h"

SoundViewer::SoundViewer(GameContext& context, GameState& state)
    : View(context, state), audio_(context.audio), current_index_(0),
      currently_playing_index_(-1),
      listener_(context.input->register_listener(*this)) {
  if (!audio_->is_initialized()) {
    audio_->init();
    audio_->load_default_assets();
  }
  load_all_m_files();
}

void SoundViewer::load_all_m_files() {
  // Track slot indices as they're allocated by AudioManager
  // These match the loading order in Game.cpp
  size_t sound_slot = 0;
  size_t music_slot = 0;

  for (int i = 0; i <= 11; ++i) {
    std::string filename = compat::format("sound/m{}", i);

    try {
      std::vector<AudioAsset> loaded_assets = MFileLoader::load(filename);

      for (size_t j = 0; j < loaded_assets.size(); ++j) {
        AudioAssetInfo info;
        info.file_name = compat::format("M{}", i);
        info.type = loaded_assets[j].type;
        info.asset_index = static_cast<int>(j);

        if (loaded_assets[j].type == AudioAssetType::CMF) {
          info.slot_index = music_slot++;
        }
        else {
          info.slot_index = sound_slot++;
        }

        assets_.push_back(info);
        Logger::debug(
            "SoundViewer: {} [{}] {} slot {}", info.file_name, info.asset_index,
            info.type == AudioAssetType::CMF ? "CMF" : "VOC", info.slot_index);
      }
    }
    catch (const std::exception& e) {
      Logger::warn("SoundViewer: Failed to load {}: {}", filename, e.what());
    }
  }

  Logger::info("SoundViewer: Loaded {} audio assets", assets_.size());
}

void SoundViewer::play_current() {
  if (assets_.empty() || current_index_ < 0 ||
      current_index_ >= static_cast<int>(assets_.size())) {
    return;
  }

  stop_current();

  const auto& asset = assets_[current_index_];

  if (asset.type == AudioAssetType::CMF) {
    // Play music
    auto music_id = static_cast<audio::MusicId>(asset.slot_index);
    audio_->play_music(music_id);
    Logger::info("Playing music slot {}", asset.slot_index);
  }
  else {
    // Play sound in a loop
    auto sound_id = static_cast<audio::SoundId>(asset.slot_index);
    audio_->play_sound(sound_id, true);
    Logger::info("Playing sound slot {} (looping)", asset.slot_index);
  }

  currently_playing_index_ = current_index_;
}

void SoundViewer::stop_current() {
  if (currently_playing_index_ < 0 ||
      currently_playing_index_ >= static_cast<int>(assets_.size())) {
    return;
  }

  const auto& asset = assets_[currently_playing_index_];

  if (asset.type == AudioAssetType::CMF) {
    audio_->stop_music();
  }
  else {
    auto sound_id = static_cast<audio::SoundId>(asset.slot_index);
    audio_->stop_sound(sound_id);
  }

  currently_playing_index_ = -1;
}

bool SoundViewer::on_action(const input::ActionEvent& ev) {
  if (!ev.state.pressed) {
    return false;
  }

  switch (ev.action) {
  case input::Action::CycleDown:
    if (!assets_.empty()) {
      if (++current_index_ >= static_cast<int>(assets_.size())) {
        current_index_ = 0;
      }
      mark_dirty();
    }
    break;

  case input::Action::CycleUp:
    if (!assets_.empty()) {
      if (--current_index_ < 0) {
        current_index_ = static_cast<int>(assets_.size()) - 1;
      }
      mark_dirty();
    }
    break;

  case input::Action::SubmitInput:
    if (!audio_->is_playing()) {
      play_current();
    }
    else {
      stop_current();
    }
    mark_dirty();
    break;

  default:
    return false;
  }
  return true;
}

void SoundViewer::tick() {}

void SoundViewer::render(const RenderContext& ctx) {
  ctx.surface->fill(254);
  constexpr int text_color = 1;
  constexpr int highlight_color = 4;

  auto* f = context_.font;

  int y = 10;

  // Title
  f->render_text_onto(*ctx.surface, "Sound & Music Viewer", ViewPoint{4, y},
                      FontFace::ASTANDAR, text_color);
  y += 30;

  // Instructions
  f->render_text_onto(*ctx.surface, "Arrow keys: Navigate  |  Enter: Play/Stop",
                      ViewPoint{220, y}, FontFace::PIZ_NUMB, text_color);
  y += 25;

  if (assets_.empty()) {
    f->render_text_onto(*ctx.surface, "No audio assets loaded", ViewPoint{4, y},
                        FontFace::PIZ_NUMB, text_color);
    return;
  }

  // Current asset info
  const auto& current = assets_[current_index_];
  bool is_playing = (currently_playing_index_ == current_index_);

  std::string type_str =
      current.type == AudioAssetType::CMF ? "Music (CMF)" : "Sound (VOC)";
  std::string status = is_playing ? " [PLAYING]" : "";

  // Get enum name using magic_enum
  std::string enum_name;
  if (current.type == AudioAssetType::CMF) {
    auto music_id = static_cast<audio::MusicId>(current.slot_index);
    enum_name = std::string(magic_enum::enum_name(music_id));
  }
  else {
    auto sound_id = static_cast<audio::SoundId>(current.slot_index);
    enum_name = std::string(magic_enum::enum_name(sound_id));
  }

  // M file notation (e.g., "M10[6]")
  auto m_file_notation =
      compat::format("{}[{}]", current.file_name, current.asset_index);

  auto line1 = compat::format("Asset {}/{}: {}  |  {}", current_index_ + 1,
                              assets_.size(), m_file_notation, enum_name);
  f->render_text_onto(*ctx.surface, line1, ViewPoint{4, y}, FontFace::PIZ_NUMB,
                      highlight_color);
  y += 20;

  auto line2 = compat::format("Type: {}  |  Slot: {}{}", type_str,
                              current.slot_index, status);
  f->render_text_onto(*ctx.surface, line2, ViewPoint{220, y},
                      FontFace::PIZ_NUMB, text_color);
  y += 30;

  // List some nearby assets for context
  f->render_text_onto(*ctx.surface, "Available assets:", ViewPoint{4, y},
                      FontFace::PIZ_NUMB, text_color);
  y += 20;

  // Always show 15 items (or fewer if there aren't enough)
  constexpr int visible_rows = 15;
  int total_assets = static_cast<int>(assets_.size());
  int half_visible = visible_rows / 2;

  int start = current_index_ - half_visible;
  int end = start + visible_rows;

  // Adjust if we're too close to the beginning
  if (start < 0) {
    start = 0;
    end = std::min(visible_rows, total_assets);
  }
  // Adjust if we're too close to the end
  else if (end > total_assets) {
    end = total_assets;
    start = std::max(0, end - visible_rows);
  }

  for (int i = start; i < end; ++i) {
    const auto& asset = assets_[i];
    bool is_current = (i == current_index_);
    bool asset_playing = (i == currently_playing_index_);

    std::string prefix = is_current ? "> " : "  ";
    std::string play_marker = asset_playing ? " [*]" : "";
    std::string asset_type = asset.type == AudioAssetType::CMF ? "CMF" : "VOC";

    // Get enum name
    std::string asset_enum_name;
    if (asset.type == AudioAssetType::CMF) {
      auto music_id = static_cast<audio::MusicId>(asset.slot_index);
      asset_enum_name = std::string(magic_enum::enum_name(music_id));
    }
    else {
      auto sound_id = static_cast<audio::SoundId>(asset.slot_index);
      asset_enum_name = std::string(magic_enum::enum_name(sound_id));
    }

    // M file notation
    auto asset_m_notation =
        compat::format("{}[{}]", asset.file_name, asset.asset_index);

    auto line = compat::format("{}{} {} - {}{}", prefix, asset_m_notation,
                               asset_type, asset_enum_name, play_marker);

    int color = is_current ? highlight_color : text_color;
    f->render_text_onto(*ctx.surface, line, ViewPoint{4, y}, FontFace::PIZ_NUMB,
                        color);
    y += 15;
  }
}
