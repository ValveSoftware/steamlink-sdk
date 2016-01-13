// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_WEBAUDIO_MEDIA_CODEC_BRIDGE_H_
#define MEDIA_BASE_ANDROID_WEBAUDIO_MEDIA_CODEC_BRIDGE_H_

#include <jni.h>

#include "base/file_descriptor_posix.h"
#include "base/memory/shared_memory.h"
#include "media/base/media_export.h"

namespace media {

// This class serves as a bridge for native code to call Java
// functions in the Android MediaCodec class. See
// http://developer.android.com/reference/android/media/MediaCodec.html.
class MEDIA_EXPORT WebAudioMediaCodecBridge {
 public:
  // Create the bridge with the given file descriptors. We read from
  // |encoded_audio_handle| to get the encoded audio data. Audio file
  // information and decoded PCM samples are written to |pcm_output|.
  // We also take ownership of |pcm_output|.
  WebAudioMediaCodecBridge(base::SharedMemoryHandle encoded_audio_handle,
                           base::FileDescriptor pcm_output,
                           uint32_t data_size);
  ~WebAudioMediaCodecBridge();

  // Inform JNI about this bridge. Returns true if registration
  // succeeded.
  static bool RegisterWebAudioMediaCodecBridge(JNIEnv* env);

  // Start MediaCodec to process the encoded data in
  // |encoded_audio_handle|. The PCM samples are sent to |pcm_output|.
  static void RunWebAudioMediaCodec(
      base::SharedMemoryHandle encoded_audio_handle,
      base::FileDescriptor pcm_output,
      uint32_t data_size);

  void OnChunkDecoded(JNIEnv* env,
                      jobject /*java object*/,
                      jobject buf,
                      jint buf_size,
                      jint input_channel_count,
                      jint output_channel_count);

  void InitializeDestination(JNIEnv* env,
                             jobject /*java object*/,
                             jint channel_count,
                             jint sample_rate,
                             jlong duration_us);

 private:
  // Handles MediaCodec processing of the encoded data in
  // |encoded_audio_handle_| and sends the pcm data to |pcm_output_|.
  // Returns true if decoding was successful.
  bool DecodeInMemoryAudioFile();

  // Save encoded audio data to a temporary file and return the file
  // descriptor to that file.  -1 is returned if the audio data could
  // not be saved for any reason.
  int SaveEncodedAudioToFile(JNIEnv*, jobject);

  // The encoded audio data is read from this file descriptor for the
  // shared memory that holds the encoded data.
  base::SharedMemoryHandle encoded_audio_handle_;

  // The audio file information and decoded pcm data are written to
  // this file descriptor. We take ownership of this descriptor.
  int pcm_output_;

  // The length of the encoded data.
  uint32_t data_size_;

  DISALLOW_COPY_AND_ASSIGN(WebAudioMediaCodecBridge);
};

}  // namespace media
#endif  // MEDIA_BASE_ANDROID_WEBAUDIO_MEDIA_CODEC_BRIDGE_H_
