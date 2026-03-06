#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#if PLEGACY_ENABLE_MUSIC
struct ADL_MIDIPlayer;
#endif

using SDL_AudioDeviceID = unsigned int;

namespace audio {

enum class MusicState {
  STOPPED,
  PLAYING,
  DRAINING,
};

enum class SoundId {
  // M0
  RESTAURANT_NOISE_1 = 0,
  RESTAURANT_NOISE_2,
  RESTAURANT_NOISE_3,

  // M7
  TRAFFIC_1,
  TRAFFIC_2,

  // M8
  DIAL_TONE_0,
  DIAL_TONE_1,
  DIAL_TONE_2,
  DIAL_TONE_3,
  DIAL_TONE_4,
  DIAL_TONE_5,
  DIAL_TONE_6,
  DIAL_TONE_7,
  DIAL_TONE_8,
  DIAL_TONE_9,
  PHONE_RINGING, // m8[10]
  PHONE_PICKUP,  // m8[11]

  // M9
  CHOPPER, // m9[0]

  // M10
  WEAPON_1,     // m10[2] - First weapon sound
  WEAPON_2,     // m10[3]
  WEAPON_3,     // m10[4]
  WEAPON_4,     // m10[5]
  POLICE_SIREN, // m10[6]

  COUNT
};

enum class MusicId {
  TECH_THE_TRONIC = 0, // m1
  JAM_THE_SCRATCH,     // m2
  HEAVY_BANGER,        // m3
  AMORE_MIO,           // m4
  ELEVATOR_MUSIC,      // m5
  TEA_SHOPPE,          // m6

  // M10
  BREAK_IN_MUSIC_1, // m10[0]
  BREAK_IN_MUSIC_2, // m10[1]

  // M11
  BREAK_IN_MUSIC_3, // m11[0]

  COUNT
};

struct SoundEffect {
  // Signed 16-bit stereo at 49716 Hz:
  std::vector<int16_t> samples;
  size_t playback_position = 0;
  bool playing = false;
  bool loop = false;
};

class AudioManager {
public:
  AudioManager();
  ~AudioManager();

  AudioManager(const AudioManager&) = delete;
  AudioManager& operator=(const AudioManager&) = delete;

  void play_sound(SoundId id, bool loop = false);
  void play_music(MusicId id, bool loop = true);
  void stop_music();
  void stop_sound(SoundId id);

  // Volume control (0.0 to 1.0)
  void music_volume(float volume);
  void sound_volume(float volume);

  void init();

  // automatically distributes CMF/VOC to appropriate storage
  void load_m_container(const std::string& m_file_path);

  bool is_enabled() const { return audio_enabled_; }
  bool is_playing() const;

private:
  static void audio_callback(void* userdata, uint8_t* stream, int len);
  void fill_audio_buffer(int16_t* buffer, int sample_count);
  void mix_music(int16_t* buffer, int sample_count);
  void mix_sounds(int16_t* buffer, int sample_count);

#if PLEGACY_ENABLE_MUSIC
  struct ADLMIDIDeleter {
    void operator()(ADL_MIDIPlayer* p);
  };
  std::unique_ptr<ADL_MIDIPlayer, ADLMIDIDeleter> midi_player_;
#endif
  // Indexed by MusicId:
  std::vector<std::vector<uint8_t>> music_data_;
  std::mutex music_mutex_;

  std::vector<std::unique_ptr<SoundEffect>> sounds_;
  mutable std::mutex sound_mutex_;

  SDL_AudioDeviceID audio_device_{0};
  static constexpr int SAMPLE_RATE = 49716;
  static constexpr int CHANNELS = 2;

  // Volume
  float music_volume_ = 1.0f;
  // Pre-volume boost for OPL output (otherwise was too quiet):
  float music_gain_ = 6.0f;
  float sound_volume_ = 0.9f;
  // Prevent clipping when mixing
  float master_volume_ = 0.8f;

  bool initialized_ = false;
  bool audio_enabled_ = false;
  bool music_loop_ = false;
  MusicState music_state_ = MusicState::STOPPED;
#if PLEGACY_ENABLE_MUSIC
  int drain_samples_ = 0;
  static constexpr int MAX_DRAIN_SAMPLES = SAMPLE_RATE * 2; // 2 seconds max
#endif

  // Asset tracking
  size_t next_sound_slot_ = 0;
#if PLEGACY_ENABLE_MUSIC
  size_t next_music_slot_ = 0;
#endif
};

} // namespace audio
