/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/audio/AudioManager.h"

#include <algorithm>
#include <cstring>

#include "core/util/ConfigManager.h"
#include "engine/Logger.h"
#include "engine/SDL_include.h"
#include "engine/audio/VocDecoder.h"
#include "resources/MFileLoader.h"

#if PLEGACY_ENABLE_MUSIC
#include <adlmidi.h>
#endif

namespace audio {

#if PLEGACY_ENABLE_MUSIC
void AudioManager::ADLMIDIDeleter::operator()(ADL_MIDIPlayer* p) {
  if (p) {
    adl_close(p);
  }
}
#endif

AudioManager::AudioManager()
    : music_data_(static_cast<size_t>(MusicId::COUNT)) {}

AudioManager::~AudioManager() {
  if (audio_device_ != 0) {
    SDL_CloseAudioDevice(audio_device_);
  }
}

void AudioManager::init() {
  Logger::info("AudioManager initializing...");
  if (initialized_) {
    Logger::warn("AudioManager already initialized");
    return;
  }

#if PLEGACY_ENABLE_MUSIC
  // Initialize libADLMIDI
  midi_player_.reset(adl_init(SAMPLE_RATE));
  if (!midi_player_) {
    Logger::warn("Failed to initialize libADLMIDI");
    audio_enabled_ = false;
    initialized_ = true;
    return;
  }

  adl_setVolumeRangeModel(midi_player_.get(), ADLMIDI_VolumeModel_CMF);
  adl_setNumChips(midi_player_.get(), 1);
#endif

  // Set up SDL audio specification
  SDL_AudioSpec want;
  SDL_AudioSpec have;
  SDL_zero(want);
  want.freq = SAMPLE_RATE;
  want.format = AUDIO_S16SYS;
  want.channels = CHANNELS;
  want.samples = 2048;
  want.callback = audio_callback;
  want.userdata = this;

  // Open audio device
  audio_device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
  if (audio_device_ == 0) {
    Logger::warn("Failed to open SDL audio device: {}", SDL_GetError());
    audio_enabled_ = false;
    initialized_ = true;
    return;
  }

  // Verify we got the format we wanted
  if (have.format != AUDIO_S16SYS || have.channels != CHANNELS ||
      have.freq != SAMPLE_RATE) {
    Logger::warn("SDL audio format mismatch: got {}Hz, {}ch, format {}",
                 have.freq, have.channels, have.format);
  }

  audio_enabled_ = true;
  initialized_ = true;

  // Start playback
  SDL_PauseAudioDevice(audio_device_, 0);

  Logger::info("AudioManager initialized: {}Hz, {} channels", SAMPLE_RATE,
               CHANNELS);
}

void AudioManager::load_m_container(const std::string& m_file_path) {
  if (!audio_enabled_) {
    Logger::debug("Audio disabled, skipping load of {}", m_file_path);
    return;
  }

  std::vector<AudioAsset> assets = MFileLoader::load(m_file_path);

  for (auto& asset : assets) {
    if (asset.type == AudioAssetType::CMF) {
#if PLEGACY_ENABLE_MUSIC
      if (next_music_slot_ >= static_cast<size_t>(MusicId::COUNT)) {
        Logger::warn("Too many music tracks, ignoring CMF from {}",
                     m_file_path);
        continue;
      }

      music_data_[next_music_slot_] = std::move(asset.data);
      Logger::debug("Loaded music slot {} from {}", next_music_slot_,
                    m_file_path);
      next_music_slot_++;
#endif
    }
    else if (asset.type == AudioAssetType::VOC) {
      // Decode and store VOC sound effect
      VocAudioData voc_audio = VocDecoder::decode(asset.data, SAMPLE_RATE);

      auto sound = std::make_unique<SoundEffect>();
      sound->samples = std::move(voc_audio.samples);
      sound->playback_position = 0;
      sound->playing = false;
      sound->loop = false;

      sounds_.push_back(std::move(sound));

      // Trim annoying static from the beginning of the chopper sound
      if (next_sound_slot_ == static_cast<size_t>(SoundId::CHOPPER)) {
        // Skip first 500 frames; sample * channels * seconds is how
        // you calculate the number of frames but by trial and error
        // I found that 500 is enough for the noise at the start of
        // this file.
        constexpr size_t skip_samples = 500;
        auto& samples = sounds_.back()->samples;
        if (samples.size() > skip_samples) {
          samples.erase(samples.begin(), samples.begin() + skip_samples);
          Logger::debug("Trimmed {} samples from CHOPPER sound", skip_samples);
        }
      }

      Logger::debug("Loaded sound slot {} from {} ({} samples)",
                    next_sound_slot_, m_file_path,
                    sounds_.back()->samples.size());
      next_sound_slot_++;
    }
  }
}

void AudioManager::play_sound(SoundId id, bool loop) {
  if (!audio_enabled_) {
    return;
  }

  auto slot = static_cast<size_t>(id);
  if (slot >= sounds_.size()) {
    Logger::warn("Sound slot {} out of range (have {} sounds)", slot,
                 sounds_.size());
    return;
  }

  if (!sounds_[slot]) {
    Logger::warn("Sound slot {} is null", slot);
    return;
  }

  std::lock_guard<std::mutex> lock(sound_mutex_);
  sounds_[slot]->playback_position = 0;
  sounds_[slot]->playing = true;
  sounds_[slot]->loop = loop;
}

void AudioManager::stop_sound(SoundId id) {
  if (!audio_enabled_) {
    return;
  }

  auto slot = static_cast<size_t>(id);
  if (slot >= sounds_.size() || !sounds_[slot]) {
    return;
  }

  std::lock_guard<std::mutex> lock(sound_mutex_);
  sounds_[slot]->playing = false;
}

void AudioManager::play_music(MusicId id, bool loop) {
#if PLEGACY_ENABLE_MUSIC
  if (!audio_enabled_) {
    return;
  }

  auto* config = ConfigManager::instance();
  if (config && !config->options().music) {
    return;
  }

  auto slot = static_cast<size_t>(id);
  if (slot >= music_data_.size() || music_data_[slot].empty()) {
    Logger::warn("Music {} not loaded", slot);
    return;
  }

  std::lock_guard<std::mutex> lock(music_mutex_);

  adl_reset(midi_player_.get());

  int result =
      adl_openData(midi_player_.get(), music_data_[slot].data(),
                   static_cast<unsigned long>(music_data_[slot].size()));

  if (result < 0) {
    Logger::warn("Failed to load music data: {}", adl_errorString());
    music_state_ = MusicState::STOPPED;
  }
  else {
    Logger::debug("Playing music slot {} (loop={})", slot, loop);
    music_loop_ = loop;
    music_state_ = MusicState::PLAYING;
  }
#else
  (void)id;
  (void)loop;
#endif
}

void AudioManager::stop_music() {
#if PLEGACY_ENABLE_MUSIC
  if (!audio_enabled_) {
    return;
  }
  std::lock_guard<std::mutex> lock(music_mutex_);

  if (music_state_ == MusicState::PLAYING) {
    // Trigger the release phase of the active notes so FM tails fade out
    adl_panic(midi_player_.get());

    music_state_ = MusicState::DRAINING;
    drain_samples_ = 0;
  }
#endif
}

void AudioManager::music_volume(float volume) {
  music_volume_ = std::clamp(volume, 0.0f, 1.0f);
}

void AudioManager::sound_volume(float volume) {
  sound_volume_ = std::clamp(volume, 0.0f, 1.0f);
}

bool AudioManager::is_playing() const {
  if (!audio_enabled_) {
    return false;
  }

  if (music_state_ == MusicState::PLAYING) {
    return true;
  }

  std::lock_guard<std::mutex> lock(sound_mutex_);
  for (const auto& sound_ptr : sounds_) {
    if (sound_ptr && sound_ptr->playing) {
      return true;
    }
  }

  return false;
}

void AudioManager::audio_callback(void* userdata, uint8_t* stream, int len) {
  auto* manager = static_cast<AudioManager*>(userdata);
  if (!manager || !manager->audio_enabled_) {
    std::memset(stream, 0, static_cast<size_t>(len));
    return;
  }

  auto* buffer = reinterpret_cast<int16_t*>(stream);
  int sample_count = len / (sizeof(int16_t) * CHANNELS);

  manager->fill_audio_buffer(buffer, sample_count);
}

void AudioManager::fill_audio_buffer(int16_t* buffer, int sample_count) {
  // Create floating-point mixing buffer
  std::vector<float> mix_buffer(static_cast<size_t>(sample_count * CHANNELS),
                                0.0f);

  mix_music(buffer, sample_count);
  for (int i = 0; i < sample_count * CHANNELS; ++i) {
    mix_buffer[i] += (buffer[i] / 32768.0f) * music_volume_;
  }

  mix_sounds(buffer, sample_count);
  for (int i = 0; i < sample_count * CHANNELS; ++i) {
    mix_buffer[i] += (buffer[i] / 32768.0f) * sound_volume_;
  }

  // Apply master volume and clamp to prevent overflow
  for (int i = 0; i < sample_count * CHANNELS; ++i) {
    float sample = mix_buffer[i] * master_volume_;
    sample = std::clamp(sample, -1.0f, 1.0f);
    buffer[i] = static_cast<int16_t>(sample * 32767.0f);
  }
}

void AudioManager::mix_music(int16_t* buffer, int sample_count) {
#if PLEGACY_ENABLE_MUSIC
  std::lock_guard<std::mutex> lock(music_mutex_);

  int generated = 0;
  if (music_state_ == MusicState::PLAYING) {
    generated = adl_play(midi_player_.get(), sample_count * CHANNELS,
                         reinterpret_cast<short*>(buffer));

    // Detect end-of-song (adl_play returns fewer samples than requested)
    if (generated < sample_count * CHANNELS) {
      if (music_loop_) {
        adl_positionRewind(midi_player_.get());
        int remaining = sample_count * CHANNELS - generated;
        generated += adl_play(midi_player_.get(), remaining,
                              reinterpret_cast<short*>(buffer + generated));
      }
      else {
        adl_panic(midi_player_.get());
        music_state_ = MusicState::DRAINING;
        drain_samples_ = 0;
      }
    }

    // Boost raw OPL output to get the audio louder (too soft otherwise)
    if (music_gain_ != 1.0f) {
      for (int i = 0; i < generated; ++i) {
        float boosted = buffer[i] * music_gain_;
        buffer[i] =
            static_cast<int16_t>(std::clamp(boosted, -32768.0f, 32767.0f));
      }
    }
  }
  else if (music_state_ == MusicState::DRAINING) {
    // Process the OPL3 chip's release envelopes without advancing the
    // MIDI sequencer or triggering new notes.
    // This makes it sound the same as the original
    // TODO: Probably during drain we should still apply the music_gain_ from
    // above
    generated = adl_generate(midi_player_.get(), sample_count * CHANNELS,
                             reinterpret_cast<short*>(buffer));

    drain_samples_ += sample_count;

    // Check for silence to end the drain
    bool is_silent = true;
    for (int i = 0; i < generated; ++i) {
      if (std::abs(buffer[i]) > 15) { // Threshold for FM noise floor
        is_silent = false;
        break;
      }
    }

    if (is_silent || drain_samples_ > MAX_DRAIN_SAMPLES) {
      music_state_ = MusicState::STOPPED;
      adl_reset(midi_player_.get());
    }
  }

  // Zero out any samples not written by the emulator
  if (generated < sample_count * CHANNELS) {
    std::memset(buffer + generated, 0,
                (sample_count * CHANNELS - generated) * sizeof(int16_t));
  }
#else
  std::memset(buffer, 0,
              static_cast<size_t>(sample_count * CHANNELS * sizeof(int16_t)));
#endif
}

void AudioManager::mix_sounds(int16_t* buffer, int sample_count) {
  std::lock_guard<std::mutex> lock(sound_mutex_);

  // Clear buffer first
  std::memset(buffer, 0,
              static_cast<size_t>(sample_count * CHANNELS * sizeof(int16_t)));

  // Mix all active sounds
  for (auto& sound_ptr : sounds_) {
    if (!sound_ptr || !sound_ptr->playing) {
      continue;
    }

    SoundEffect& sound = *sound_ptr;
    size_t samples_to_copy = static_cast<size_t>(sample_count * CHANNELS);
    size_t remaining = sound.samples.size() - sound.playback_position;

    if (remaining == 0 && sound.loop) {
      // Loop back to start
      sound.playback_position = 0;
      remaining = sound.samples.size();
    }

    if (remaining == 0) {
      // Sound finished
      sound.playing = false;
      continue;
    }

    // Copy what we can
    size_t to_copy = std::min(samples_to_copy, remaining);
    for (size_t i = 0; i < to_copy; ++i) {
      // Simple additive mixing (will be handled by float mixing buffer in
      // fill_audio_buffer)
      buffer[i] = sound.samples[sound.playback_position + i];
    }

    sound.playback_position += to_copy;
  }
}

} // namespace audio
