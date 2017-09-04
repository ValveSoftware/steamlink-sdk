// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_REMOTING_INTERSTITIAL_UI_H_
#define MEDIA_REMOTING_REMOTING_INTERSTITIAL_UI_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/base/pipeline_metadata.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace media {

class VideoFrame;
class VideoRendererSink;

class RemotingInterstitialUI {
 public:
  RemotingInterstitialUI(VideoRendererSink* video_renderer_sink,
                         const PipelineMetadata& pipeline_metadata);
  ~RemotingInterstitialUI();

  void ShowInterstitial(bool is_remoting_successful);

 private:
  // Gets an 'interstitial' VideoFrame to paint on the media player when the
  // video is being played remotely.
  scoped_refptr<VideoFrame> GetInterstitial(const SkBitmap& background_image,
                                            bool is_remoting_successful);

  VideoRendererSink* const video_renderer_sink_;  // Outlives this class.
  PipelineMetadata pipeline_metadata_;

  DISALLOW_COPY_AND_ASSIGN(RemotingInterstitialUI);
};

}  // namespace media

#endif  // MEDIA_REMOTING_REMOTING_INTERSTITIAL_UI_H_
