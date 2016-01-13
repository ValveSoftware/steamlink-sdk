// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/media/media_param_traits.h"

#include "base/strings/stringprintf.h"
#include "media/audio/audio_parameters.h"
#include "media/base/limits.h"
#include "media/video/capture/video_capture_types.h"

using media::AudioParameters;
using media::ChannelLayout;
using media::VideoCaptureFormat;
using media::VideoPixelFormat;

namespace IPC {

void ParamTraits<AudioParameters>::Write(Message* m,
                                         const AudioParameters& p) {
  m->WriteInt(static_cast<int>(p.format()));
  m->WriteInt(static_cast<int>(p.channel_layout()));
  m->WriteInt(p.sample_rate());
  m->WriteInt(p.bits_per_sample());
  m->WriteInt(p.frames_per_buffer());
  m->WriteInt(p.channels());
  m->WriteInt(p.input_channels());
  m->WriteInt(p.effects());
}

bool ParamTraits<AudioParameters>::Read(const Message* m,
                                        PickleIterator* iter,
                                        AudioParameters* r) {
  int format, channel_layout, sample_rate, bits_per_sample,
      frames_per_buffer, channels, input_channels, effects;

  if (!m->ReadInt(iter, &format) ||
      !m->ReadInt(iter, &channel_layout) ||
      !m->ReadInt(iter, &sample_rate) ||
      !m->ReadInt(iter, &bits_per_sample) ||
      !m->ReadInt(iter, &frames_per_buffer) ||
      !m->ReadInt(iter, &channels) ||
      !m->ReadInt(iter, &input_channels) ||
      !m->ReadInt(iter, &effects))
    return false;
  AudioParameters params(static_cast<AudioParameters::Format>(format),
         static_cast<ChannelLayout>(channel_layout), channels,
         input_channels, sample_rate, bits_per_sample, frames_per_buffer,
         effects);
  *r = params;
  if (!r->IsValid())
    return false;
  return true;
}

void ParamTraits<AudioParameters>::Log(const AudioParameters& p,
                                       std::string* l) {
  l->append(base::StringPrintf("<AudioParameters>"));
}

void ParamTraits<VideoCaptureFormat>::Write(Message* m,
                                            const VideoCaptureFormat& p) {
  // Crash during Send rather than have a failure at the message handler.
  m->WriteInt(p.frame_size.width());
  m->WriteInt(p.frame_size.height());
  m->WriteFloat(p.frame_rate);
  m->WriteInt(static_cast<int>(p.pixel_format));
}

bool ParamTraits<VideoCaptureFormat>::Read(const Message* m,
                                           PickleIterator* iter,
                                           VideoCaptureFormat* r) {
  int frame_size_width, frame_size_height, pixel_format;
  if (!m->ReadInt(iter, &frame_size_width) ||
      !m->ReadInt(iter, &frame_size_height) ||
      !m->ReadFloat(iter, &r->frame_rate) ||
      !m->ReadInt(iter, &pixel_format))
    return false;

  r->frame_size.SetSize(frame_size_width, frame_size_height);
  r->pixel_format = static_cast<VideoPixelFormat>(pixel_format);
  if (!r->IsValid())
    return false;
  return true;
}

void ParamTraits<VideoCaptureFormat>::Log(const VideoCaptureFormat& p,
                                          std::string* l) {
  l->append(base::StringPrintf("<VideoCaptureFormat>"));
}

}
