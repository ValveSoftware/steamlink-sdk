// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_TRACK_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_TRACK_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "media/base/audio_parameters.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"

namespace content {

// MediaStreamTrack is a Chrome representation of blink::WebMediaStreamTrack.
// It is owned by blink::WebMediaStreamTrack as
// blink::WebMediaStreamTrack::ExtraData.
class CONTENT_EXPORT MediaStreamTrack
    : NON_EXPORTED_BASE(public blink::WebMediaStreamTrack::TrackData) {
 public:
  explicit MediaStreamTrack(bool is_local_track);
  ~MediaStreamTrack() override;

  static MediaStreamTrack* GetTrack(const blink::WebMediaStreamTrack& track);

  virtual void SetEnabled(bool enabled) = 0;

  virtual void Stop() = 0;

  // TODO(hta): Make method pure virtual when all tracks have the method.
  void getSettings(blink::WebMediaStreamTrack::Settings& settings) override {}

  bool is_local_track() const { return is_local_track_; }

 protected:
  const bool is_local_track_;

  // Used to DCHECK that we are called on Render main Thread.
  base::ThreadChecker main_render_thread_checker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaStreamTrack);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_TRACK_H_
