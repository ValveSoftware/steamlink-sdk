// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_ANDROID_AUDIO_RECORD_INPUT_H_
#define MEDIA_AUDIO_ANDROID_AUDIO_RECORD_INPUT_H_

#include "base/android/jni_android.h"
#include "base/threading/thread_checker.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"

namespace media {

class AudioBus;
class AudioManagerAndroid;

// Implements PCM audio input support for Android using the Java AudioRecord
// interface. Most of the work is done by its Java counterpart in
// AudioRecordInput.java. This class is created and lives on the Audio Manager
// thread but recorded audio buffers are delivered on a thread managed by
// the Java class.
//
// The Java class makes use of AudioEffect features which are first available
// in Jelly Bean. It should not be instantiated running against earlier SDKs.
class MEDIA_EXPORT AudioRecordInputStream : public AudioInputStream {
 public:
  AudioRecordInputStream(AudioManagerAndroid* manager,
                         const AudioParameters& params);

  virtual ~AudioRecordInputStream();

  // Implementation of AudioInputStream.
  virtual bool Open() OVERRIDE;
  virtual void Start(AudioInputCallback* callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual double GetMaxVolume() OVERRIDE;
  virtual void SetVolume(double volume) OVERRIDE;
  virtual double GetVolume() OVERRIDE;
  virtual void SetAutomaticGainControl(bool enabled) OVERRIDE;
  virtual bool GetAutomaticGainControl() OVERRIDE;

  static bool RegisterAudioRecordInput(JNIEnv* env);

  // Called from Java when data is available.
  void OnData(JNIEnv* env, jobject obj, jint size, jint hardware_delay_bytes);

  // Called from Java so that we can cache the address of the Java-managed
  // |byte_buffer| in |direct_buffer_address_|.
  void CacheDirectBufferAddress(JNIEnv* env, jobject obj, jobject byte_buffer);

 private:
  base::ThreadChecker thread_checker_;
  AudioManagerAndroid* audio_manager_;

  // Java AudioRecordInput instance.
  base::android::ScopedJavaGlobalRef<jobject> j_audio_record_;

  // This is the only member accessed by both the Audio Manager and Java
  // threads. Explanations for why we do not require explicit synchronization
  // are given in the implementation.
  AudioInputCallback* callback_;

  // Owned by j_audio_record_.
  uint8* direct_buffer_address_;

  scoped_ptr<media::AudioBus> audio_bus_;
  int bytes_per_sample_;

  DISALLOW_COPY_AND_ASSIGN(AudioRecordInputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_ANDROID_AUDIO_RECORD_INPUT_H_
