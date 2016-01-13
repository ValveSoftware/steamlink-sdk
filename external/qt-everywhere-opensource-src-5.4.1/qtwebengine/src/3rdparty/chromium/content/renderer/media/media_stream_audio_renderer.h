// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_RENDERER_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_RENDERER_H_

#include "base/memory/ref_counted.h"
#include "base/time/time.h"

namespace content {

class MediaStreamAudioRenderer
    : public base::RefCountedThreadSafe<MediaStreamAudioRenderer> {
 public:
  // Starts rendering audio.
  virtual void Start() = 0;

  // Stops rendering audio.
  virtual void Stop() = 0;

  // Resumes rendering audio after being paused.
  virtual void Play() = 0;

  // Temporarily suspends rendering audio. The audio stream might still be
  // active but new audio data is not provided to the consumer.
  virtual void Pause() = 0;

  // Sets the output volume.
  virtual void SetVolume(float volume) = 0;

  // Time stamp that reflects the current render time. Should not be updated
  // when paused.
  virtual base::TimeDelta GetCurrentRenderTime() const = 0;

  // Returns true if the implementation is a local renderer and false
  // otherwise.
  virtual bool IsLocalRenderer() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<MediaStreamAudioRenderer>;

  MediaStreamAudioRenderer();
  virtual ~MediaStreamAudioRenderer();

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaStreamAudioRenderer);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_RENDERER_H_
