// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_ANDROID_OPENSLES_OUTPUT_H_
#define MEDIA_AUDIO_ANDROID_OPENSLES_OUTPUT_H_

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include "base/compiler_specific.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "media/audio/android/opensles_util.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"

namespace media {

class AudioManagerAndroid;

// Implements PCM audio output support for Android using the OpenSLES API.
// This class is created and lives on the Audio Manager thread but recorded
// audio buffers are given to us from an internal OpenSLES audio thread.
// All public methods should be called on the Audio Manager thread.
class OpenSLESOutputStream : public AudioOutputStream {
 public:
  static const int kMaxNumOfBuffersInQueue = 2;

  OpenSLESOutputStream(AudioManagerAndroid* manager,
                       const AudioParameters& params,
                       SLint32 stream_type);

  virtual ~OpenSLESOutputStream();

  // Implementation of AudioOutputStream.
  virtual bool Open() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void Start(AudioSourceCallback* callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void SetVolume(double volume) OVERRIDE;
  virtual void GetVolume(double* volume) OVERRIDE;

  // Set the value of |muted_|. It does not affect |volume_| which can be
  // got by calling GetVolume(). See comments for |muted_| below.
  void SetMute(bool muted);

 private:
  bool CreatePlayer();

  // Called from OpenSLES specific audio worker thread.
  static void SimpleBufferQueueCallback(
      SLAndroidSimpleBufferQueueItf buffer_queue,
      void* instance);

  // Fills up one buffer by asking the registered source for data.
  // Called from OpenSLES specific audio worker thread.
  void FillBufferQueue();

  // Called from the audio manager thread.
  void FillBufferQueueNoLock();

  // Called in Open();
  void SetupAudioBuffer();

  // Called in Close();
  void ReleaseAudioBuffer();

  // If OpenSLES reports an error this function handles it and passes it to
  // the attached AudioOutputCallback::OnError().
  void HandleError(SLresult error);

  base::ThreadChecker thread_checker_;

  // Protects |callback_|, |active_buffer_index_|, |audio_data_|,
  // |buffer_size_bytes_| and |simple_buffer_queue_|.
  base::Lock lock_;

  AudioManagerAndroid* audio_manager_;

  // Audio playback stream type.
  // See SLES/OpenSLES_Android.h for details.
  SLint32 stream_type_;

  AudioSourceCallback* callback_;

  // Shared engine interfaces for the app.
  media::ScopedSLObjectItf engine_object_;
  media::ScopedSLObjectItf player_object_;
  media::ScopedSLObjectItf output_mixer_;

  SLPlayItf player_;

  // Buffer queue recorder interface.
  SLAndroidSimpleBufferQueueItf simple_buffer_queue_;

  SLDataFormat_PCM format_;

  // Audio buffers that are allocated in the constructor based on
  // info from audio parameters.
  uint8* audio_data_[kMaxNumOfBuffersInQueue];

  int active_buffer_index_;
  size_t buffer_size_bytes_;

  bool started_;

  // Volume control coming from hardware. It overrides |volume_| when it's
  // true. Otherwise, use |volume_| for scaling.
  // This is needed because platform voice volume never goes to zero in
  // COMMUNICATION mode on Android.
  bool muted_;

  // Volume level from 0 to 1.
  float volume_;

  // Container for retrieving data from AudioSourceCallback::OnMoreData().
  scoped_ptr<AudioBus> audio_bus_;

  DISALLOW_COPY_AND_ASSIGN(OpenSLESOutputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_ANDROID_OPENSLES_OUTPUT_H_
