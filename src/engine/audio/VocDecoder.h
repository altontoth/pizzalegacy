/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

struct VocAudioData {
  std::vector<int16_t> samples; // Signed 16-bit stereo
  int original_sample_rate;
  int output_sample_rate;
  int channels;
};

class VocDecoder {
public:
  static VocAudioData decode(const std::vector<uint8_t>& voc_data,
                             int target_sample_rate = 49716);

private:
  static void convert_u8_mono_to_s16_stereo(const uint8_t* input,
                                            int16_t* output,
                                            size_t sample_count);

  static std::vector<int16_t> resample_linear(const std::vector<int16_t>& input,
                                              int input_rate, int output_rate);
};
