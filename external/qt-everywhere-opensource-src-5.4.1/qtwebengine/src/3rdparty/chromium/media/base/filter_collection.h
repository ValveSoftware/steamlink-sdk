// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_FILTER_COLLECTION_H_
#define MEDIA_BASE_FILTER_COLLECTION_H_

#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"

namespace media {

class AudioRenderer;
class Demuxer;
class TextRenderer;
class VideoRenderer;

// Represents a set of uninitialized demuxer and audio/video decoders and
// renderers. Used to start a Pipeline object for media playback.
//
// TODO(scherkus): Replace FilterCollection with something sensible, see
// http://crbug.com/110800
class MEDIA_EXPORT FilterCollection {
 public:
  FilterCollection();
  ~FilterCollection();

  void SetDemuxer(Demuxer* demuxer);
  Demuxer* GetDemuxer();

  void SetAudioRenderer(scoped_ptr<AudioRenderer> audio_renderer);
  scoped_ptr<AudioRenderer> GetAudioRenderer();

  void SetVideoRenderer(scoped_ptr<VideoRenderer> video_renderer);
  scoped_ptr<VideoRenderer> GetVideoRenderer();

  void SetTextRenderer(scoped_ptr<TextRenderer> text_renderer);
  scoped_ptr<TextRenderer> GetTextRenderer();

 private:
  Demuxer* demuxer_;
  scoped_ptr<AudioRenderer> audio_renderer_;
  scoped_ptr<VideoRenderer> video_renderer_;
  scoped_ptr<TextRenderer> text_renderer_;

  DISALLOW_COPY_AND_ASSIGN(FilterCollection);
};

}  // namespace media

#endif  // MEDIA_BASE_FILTER_COLLECTION_H_
