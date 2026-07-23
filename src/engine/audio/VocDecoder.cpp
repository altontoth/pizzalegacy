/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "engine/audio/VocDecoder.h"

#include <cstring>

#include "engine/Endian.h"
#include "engine/Fatal.h"
#include "engine/Logger.h"

namespace {
// VOC file format constants
constexpr char VOC_MAGIC[] = "Creative Voice File\x1A";
constexpr size_t VOC_MAGIC_SIZE = 20; // Including the \x1A terminator

// Block types
constexpr uint8_t BLOCK_TERMINATOR = 0;
constexpr uint8_t BLOCK_SOUND_DATA = 1;
} // anonymous namespace

VocAudioData VocDecoder::decode(const std::vector<uint8_t>& voc_data,
                                int target_sample_rate) {
  // Validate VOC header
  if (voc_data.size() < 26) {
    fatal("VOC file too small");
  }

  // Check magic string
  if (std::memcmp(voc_data.data(), VOC_MAGIC, VOC_MAGIC_SIZE) != 0) {
    fatal("Invalid VOC file magic");
  }

  // Read datablock offset (uint16_t at offset 20)
  uint16_t datablock_offset = endian::read_u16le(voc_data.data() + 20);

  if (datablock_offset >= voc_data.size()) {
    fatal("VOC datablock offset out of bounds");
  }

  VocAudioData result;
  result.output_sample_rate = target_sample_rate;
  result.channels = 2; // Always stereo output

  std::vector<uint8_t> raw_audio; // Temporary storage for unsigned 8-bit mono
  int sample_rate = 0;

  // Parse blocks
  size_t pos = datablock_offset;
  while (pos < voc_data.size()) {
    uint8_t block_type = voc_data[pos];
    pos++;

    if (block_type == BLOCK_TERMINATOR) {
      break;
    }

    // Read 24-bit block size
    if (pos + 3 > voc_data.size()) {
      fatal("VOC block size truncated");
    }
    uint32_t block_size = endian::read_u24le(voc_data.data() + pos);
    pos += 3;

    if (pos + block_size > voc_data.size()) {
      fatal("VOC block data out of bounds");
    }

    if (block_type == BLOCK_SOUND_DATA) {
      // Sound data block format:
      // - 1 byte: rate code (SR = 1000000 / (256 - rate_code))
      // - 1 byte: compression type (0 = uncompressed 8-bit)
      // - N bytes: audio data

      if (block_size < 2) {
        fatal("VOC sound data block too small");
      }

      uint8_t rate_code = voc_data[pos];
      uint8_t compression = voc_data[pos + 1];

      if (compression != 0) {
        fatal("VOC compression not supported");
      }

      // Calculate sample rate
      sample_rate = 1000000 / (256 - rate_code);
      result.original_sample_rate = sample_rate;

      // Extract audio data (skip rate and compression bytes)
      size_t audio_size = block_size - 2;
      raw_audio.insert(raw_audio.end(), voc_data.begin() + pos + 2,
                       voc_data.begin() + pos + 2 + audio_size);
    }

    pos += block_size;
  }

  if (raw_audio.empty()) {
    fatal("No audio data found in VOC file");
  }

  if (sample_rate == 0) {
    fatal("No sample rate found in VOC file");
  }

  // Convert unsigned 8-bit mono to signed 16-bit stereo
  std::vector<int16_t> stereo_samples(raw_audio.size() * 2);
  convert_u8_mono_to_s16_stereo(raw_audio.data(), stereo_samples.data(),
                                raw_audio.size());

  // Resample if needed
  if (sample_rate != target_sample_rate) {
    result.samples =
        resample_linear(stereo_samples, sample_rate, target_sample_rate);
  }
  else {
    result.samples = std::move(stereo_samples);
  }

  return result;
}

void VocDecoder::convert_u8_mono_to_s16_stereo(const uint8_t* input,
                                               int16_t* output,
                                               size_t sample_count) {

  for (size_t i = 0; i < sample_count; ++i) {
    // Convert unsigned 8-bit (0-255) to signed 16-bit (-32768 to 32767)
    // Center point is 128, so subtract 128 and scale by 256
    int16_t sample = static_cast<int16_t>((input[i] - 128) * 256);

    // Duplicate to both channels (stereo)
    output[i * 2] = sample;       // Left channel
    output[(i * 2) + 1] = sample; // Right channel
  }
}

std::vector<int16_t>
VocDecoder::resample_linear(const std::vector<int16_t>& input, int input_rate,
                            int output_rate) {

  if (input.empty() || input_rate == 0 || output_rate == 0) {
    return {};
  }

  // Input is stereo (2 channels)
  constexpr int channels = 2;
  const size_t input_frames = input.size() / channels;
  const size_t output_frames =
      (input_frames * static_cast<size_t>(output_rate)) /
      static_cast<size_t>(input_rate);

  std::vector<int16_t> output(output_frames * channels);

  // 32.32 unsigned fixed-point (UQ32.32)accumulator: avoids a FP multiply per
  // frame.
  const uint64_t step_fp = (static_cast<uint64_t>(input_rate) << 32) /
                           static_cast<uint64_t>(output_rate);
  uint64_t pos_fp = 0;

  for (size_t out_frame = 0; out_frame < output_frames; ++out_frame) {
    auto in_frame = static_cast<size_t>(pos_fp >> 32);
    auto frac_fp = static_cast<uint32_t>(pos_fp);

    if (in_frame >= input_frames - 1) {
      in_frame = input_frames - 2;
      frac_fp = 0xFFFFFFFF;
    }

    for (int ch = 0; ch < channels; ++ch) {
      int32_t s1 = input[(in_frame * channels) + static_cast<size_t>(ch)];
      int32_t s2 = input[((in_frame + 1) * channels) + static_cast<size_t>(ch)];
      output[(out_frame * channels) + static_cast<size_t>(ch)] =
          static_cast<int16_t>(
              s1 + (((s2 - s1) * static_cast<int64_t>(frac_fp)) >> 32));
    }

    pos_fp += step_fp;
  }

  return output;
}
