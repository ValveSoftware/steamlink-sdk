// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mpeg/adts_header_parser.h"

#include <algorithm>
#include <vector>
#include "base/logging.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_util.h"
#include "media/base/sample_format.h"
#include "media/formats/mpeg/adts_constants.h"

namespace media {

namespace {

size_t ExtractAdtsFrequencyIndex(const uint8_t* adts_header) {
  return ((adts_header[2] >> 2) & 0xf);
}

size_t ExtractAdtsChannelConfig(const uint8_t* adts_header) {
  return (((adts_header[3] >> 6) & 0x3) | ((adts_header[2] & 0x1) << 2));
}

}  // namespace (anonymous)

bool ParseAdtsHeader(const uint8_t* adts_header,
                     bool is_sbr,
                     AudioDecoderConfig* config,
                     size_t* orig_sample_freq) {
  DCHECK(adts_header);
  DCHECK(orig_sample_freq);

  size_t frequency_index = ExtractAdtsFrequencyIndex(adts_header);
  if (frequency_index >= kADTSFrequencyTableSize) {
    // Frequency index 13 & 14 are reserved
    // while 15 means that the frequency is explicitly written
    // (not supported).
    return false;
  }

  size_t channel_configuration = ExtractAdtsChannelConfig(adts_header);
  if (channel_configuration == 0 ||
      channel_configuration >= kADTSChannelLayoutTableSize) {
    // TODO(damienv): Add support for inband channel configuration.
    return false;
  }

  // TODO(damienv): support HE-AAC frequency doubling (SBR)
  // based on the incoming ADTS profile.
  int samples_per_second = kADTSFrequencyTable[frequency_index];
  int adts_profile = (adts_header[2] >> 6) & 0x3;

  // The following code is written according to ISO 14496 Part 3 Table 1.11 and
  // Table 1.22. (Table 1.11 refers to the capping to 48000, Table 1.22 refers
  // to SBR doubling the AAC sample rate.)
  // TODO(damienv) : Extend sample rate cap to 96kHz for Level 5 content.
  int extended_samples_per_second =
      is_sbr ? std::min(2 * samples_per_second, 48000) : samples_per_second;
  *orig_sample_freq = samples_per_second;

  // The following code is written according to ISO 14496 Part 3 Table 1.13 -
  // Syntax of AudioSpecificConfig.
  uint16_t extra_data_int = static_cast<uint16_t>(
      // Note: adts_profile is in the range [0,3], since the ADTS header only
      // allows two bits for its value.
      ((adts_profile + 1) << 11) +
      // frequency_index is [0..13], per early out above.
      (frequency_index << 7) +
      // channel_configuration is [0..7], per early out above.
      (channel_configuration << 3));
  std::vector<uint8_t> extra_data;
  extra_data.push_back(static_cast<uint8_t>(extra_data_int >> 8));
  extra_data.push_back(static_cast<uint8_t>(extra_data_int & 0xff));

  DCHECK(config);
  *config = AudioDecoderConfig(kCodecAAC, kSampleFormatS16,
                               kADTSChannelLayoutTable[channel_configuration],
                               extended_samples_per_second, extra_data,
                               Unencrypted());

  return true;
}

}  // namespace media
