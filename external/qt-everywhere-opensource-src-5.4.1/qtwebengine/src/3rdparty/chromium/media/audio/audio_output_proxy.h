// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_OUTPUT_PROXY_H_
#define MEDIA_AUDIO_AUDIO_OUTPUT_PROXY_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"

namespace media {

class AudioOutputDispatcher;

// AudioOutputProxy is an audio otput stream that uses resources more
// efficiently than a regular audio output stream: it opens audio
// device only when sound is playing, i.e. between Start() and Stop()
// (there is still one physical stream per each audio output proxy in
// playing state).
//
// AudioOutputProxy uses AudioOutputDispatcher to open and close
// physical output streams.
class MEDIA_EXPORT AudioOutputProxy
  : public AudioOutputStream,
    public NON_EXPORTED_BASE(base::NonThreadSafe) {
 public:
  // Caller keeps ownership of |dispatcher|.
  explicit AudioOutputProxy(AudioOutputDispatcher* dispatcher);

  // AudioOutputStream interface.
  virtual bool Open() OVERRIDE;
  virtual void Start(AudioSourceCallback* callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void SetVolume(double volume) OVERRIDE;
  virtual void GetVolume(double* volume) OVERRIDE;
  virtual void Close() OVERRIDE;

 private:
  enum State {
    kCreated,
    kOpened,
    kPlaying,
    kClosed,
    kOpenError,
    kStartError,
  };

  virtual ~AudioOutputProxy();

  scoped_refptr<AudioOutputDispatcher> dispatcher_;
  State state_;

  // Need to save volume here, so that we can restore it in case the stream
  // is stopped, and then started again.
  double volume_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputProxy);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_OUTPUT_PROXY_H_
