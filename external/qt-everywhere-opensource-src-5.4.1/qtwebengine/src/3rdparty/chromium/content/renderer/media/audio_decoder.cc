// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_decoder.h"

#include <vector>

#include "base/basictypes.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/limits.h"
#include "media/filters/audio_file_reader.h"
#include "media/filters/in_memory_url_protocol.h"
#include "third_party/WebKit/public/platform/WebAudioBus.h"

using media::AudioBus;
using media::AudioFileReader;
using media::InMemoryUrlProtocol;
using std::vector;
using blink::WebAudioBus;

namespace content {

// Decode in-memory audio file data.
bool DecodeAudioFileData(
    blink::WebAudioBus* destination_bus,
    const char* data, size_t data_size) {
  DCHECK(destination_bus);
  if (!destination_bus)
    return false;

  // Uses the FFmpeg library for audio file reading.
  InMemoryUrlProtocol url_protocol(reinterpret_cast<const uint8*>(data),
                                   data_size, false);
  AudioFileReader reader(&url_protocol);

  if (!reader.Open())
    return false;

  size_t number_of_channels = reader.channels();
  double file_sample_rate = reader.sample_rate();
  size_t number_of_frames = static_cast<size_t>(reader.GetNumberOfFrames());

  // Apply sanity checks to make sure crazy values aren't coming out of
  // FFmpeg.
  if (!number_of_channels ||
      number_of_channels > static_cast<size_t>(media::limits::kMaxChannels) ||
      file_sample_rate < media::limits::kMinSampleRate ||
      file_sample_rate > media::limits::kMaxSampleRate)
    return false;

  // Allocate and configure the output audio channel data.
  destination_bus->initialize(number_of_channels,
                              number_of_frames,
                              file_sample_rate);

  // Wrap the channel pointers which will receive the decoded PCM audio.
  vector<float*> audio_data;
  audio_data.reserve(number_of_channels);
  for (size_t i = 0; i < number_of_channels; ++i) {
    audio_data.push_back(destination_bus->channelData(i));
  }

  scoped_ptr<AudioBus> audio_bus = AudioBus::WrapVector(
      number_of_frames, audio_data);

  // Decode the audio file data.
  // TODO(crogers): If our estimate was low, then we still may fail to read
  // all of the data from the file.
  size_t actual_frames = reader.Read(audio_bus.get());

  // Adjust WebKit's bus to account for the actual file length
  // and valid data read.
  if (actual_frames != number_of_frames) {
    DCHECK_LE(actual_frames, number_of_frames);
    destination_bus->resizeSmaller(actual_frames);
  }

  double duration = actual_frames / file_sample_rate;

  DVLOG(1) << "Decoded file data -"
           << " data: " << data
           << " data size: " << data_size
           << " duration: " << duration
           << " number of frames: " << actual_frames
           << " sample rate: " << file_sample_rate
           << " number of channels: " << number_of_channels;

  return actual_frames > 0;
}

}  // namespace content
