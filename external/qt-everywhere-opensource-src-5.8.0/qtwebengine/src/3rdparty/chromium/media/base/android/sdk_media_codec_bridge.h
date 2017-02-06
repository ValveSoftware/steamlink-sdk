// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_SDK_MEDIA_CODEC_BRIDGE_H_
#define MEDIA_BASE_ANDROID_SDK_MEDIA_CODEC_BRIDGE_H_

#include <jni.h>
#include <stddef.h>
#include <stdint.h>

#include <set>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_export.h"
#include "media/base/video_decoder_config.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// This class implements MediaCodecBridge using android MediaCodec SDK APIs.
class MEDIA_EXPORT SdkMediaCodecBridge : public MediaCodecBridge {
 public:
  ~SdkMediaCodecBridge() override;

  // MediaCodecBridge implementations.
  bool Start() override;
  void Stop() override;
  MediaCodecStatus Flush() override;
  MediaCodecStatus GetOutputSize(gfx::Size* size) override;
  MediaCodecStatus GetOutputSamplingRate(int* sampling_rate) override;
  MediaCodecStatus GetOutputChannelCount(int* channel_count) override;
  MediaCodecStatus QueueInputBuffer(
      int index,
      const uint8_t* data,
      size_t data_size,
      const base::TimeDelta& presentation_time) override;
  using MediaCodecBridge::QueueSecureInputBuffer;
  MediaCodecStatus QueueSecureInputBuffer(
      int index,
      const uint8_t* data,
      size_t data_size,
      const std::vector<char>& key_id,
      const std::vector<char>& iv,
      const SubsampleEntry* subsamples,
      int subsamples_size,
      const base::TimeDelta& presentation_time) override;
  void QueueEOS(int input_buffer_index) override;
  MediaCodecStatus DequeueInputBuffer(const base::TimeDelta& timeout,
                                      int* index) override;
  MediaCodecStatus DequeueOutputBuffer(const base::TimeDelta& timeout,
                                       int* index,
                                       size_t* offset,
                                       size_t* size,
                                       base::TimeDelta* presentation_time,
                                       bool* end_of_stream,
                                       bool* key_frame) override;
  void ReleaseOutputBuffer(int index, bool render) override;
  MediaCodecStatus GetInputBuffer(int input_buffer_index,
                                  uint8_t** data,
                                  size_t* capacity) override;
  MediaCodecStatus GetOutputBufferAddress(int index,
                                          size_t offset,
                                          const uint8_t** addr,
                                          size_t* capacity) override;

  static bool RegisterSdkMediaCodecBridge(JNIEnv* env);

 protected:
  SdkMediaCodecBridge(const std::string& mime,
                      bool is_secure,
                      MediaCodecDirection direction,
                      bool require_software_codec);

  jobject media_codec() { return j_media_codec_.obj(); }
  MediaCodecDirection direction_;

 private:
  // Java MediaCodec instance.
  base::android::ScopedJavaGlobalRef<jobject> j_media_codec_;

  DISALLOW_COPY_AND_ASSIGN(SdkMediaCodecBridge);
};

// Class for handling audio decoding using android MediaCodec APIs.
// TODO(qinmin): implement the logic to switch between NDK and SDK
// MediaCodecBridge.
class MEDIA_EXPORT AudioCodecBridge : public SdkMediaCodecBridge {
 public:
  // Returns an AudioCodecBridge instance if |codec| is supported, or a NULL
  // pointer otherwise.
  static AudioCodecBridge* Create(const AudioCodec& codec);

  // See MediaCodecUtil::IsKnownUnaccelerated().
  static bool IsKnownUnaccelerated(const AudioCodec& codec);

  // Starts the audio codec bridge. If |play_audio| is true this method creates
  // Android AudioTrack object for the actual audio playback
  // (http://developer.android.com/reference/android/media/AudioTrack.html).
  bool ConfigureAndStart(const AudioDecoderConfig& config,
                         bool play_audio,
                         jobject media_crypto);

  // An overloaded variant used by AudioDecoderJob and AudioMediaCodecDecoder.
  // TODO(timav): Modify the above mentioned classes to pass parameters as
  // AudioDecoderConfig and remove this method.
  bool ConfigureAndStart(const AudioCodec& codec,
                         int sample_rate,
                         int channel_count,
                         const uint8_t* extra_data,
                         size_t extra_data_size,
                         int64_t codec_delay_ns,
                         int64_t seek_preroll_ns,
                         bool play_audio,
                         jobject media_crypto) WARN_UNUSED_RESULT;

  // Creates AudioTrack object for |sampling_rate| and |channel_count|
  // (http://developer.android.com/reference/android/media/AudioTrack.html).
  // Returns true in the case of success, false otherwise.
  bool CreateAudioTrack(int sampling_rate, int channel_count);

  // Plays the output buffer right away or save for later playback if |postpone|
  // is set to true. This call must be called after DequeueOutputBuffer() and
  // before ReleaseOutputBuffer. The data is extracted from the output buffers
  // using |index|, |size| and |offset|. The playback head position in frames is
  // output in |*playback_pos|.
  // When |postpone| is set to true, the next PlayOutputBuffer() should have
  // postpone == false, and it will play two buffers: the postponed one and
  // the one identified by |index|.
  // Returns MEDIA_CODEC_ERROR if an error occurs, or MEDIA_CODEC_OK otherwise.
  MediaCodecStatus PlayOutputBuffer(int index,
                                    size_t size,
                                    size_t offset,
                                    bool postpone,
                                    int64_t* playback_pos);

  // Set the volume of the audio output.
  void SetVolume(double volume);

 private:
  explicit AudioCodecBridge(const std::string& mime);

  // Configure the java MediaFormat object with the extra codec data passed in.
  bool ConfigureMediaFormat(jobject j_format,
                            const AudioCodec& codec,
                            const uint8_t* extra_data,
                            size_t extra_data_size,
                            int64_t codec_delay_ns,
                            int64_t seek_preroll_ns);
};

// Class for handling video encoding/decoding using android MediaCodec APIs.
// TODO(qinmin): implement the logic to switch between NDK and SDK
// MediaCodecBridge.
class MEDIA_EXPORT VideoCodecBridge : public SdkMediaCodecBridge {
 public:
  // See MediaCodecUtil::IsKnownUnaccelerated().
  static bool IsKnownUnaccelerated(const VideoCodec& codec,
                                   MediaCodecDirection direction);

  // Create, start, and return a VideoCodecBridge decoder or NULL on failure.
  static VideoCodecBridge* CreateDecoder(
      const VideoCodec& codec,  // e.g. media::kCodecVP8
      bool is_secure,           // Will be used with encrypted content.
      const gfx::Size& size,    // Output frame size.
      jobject surface,          // Output surface, optional.
      jobject media_crypto,     // MediaCrypto object, optional.
      bool allow_adaptive_playback =
          true,  // Should adaptive playback be allowed if supported.
      bool require_software_codec = false);  // Require software decoder?

  // Create, start, and return a VideoCodecBridge encoder or NULL on failure.
  static VideoCodecBridge* CreateEncoder(
      const VideoCodec& codec,  // e.g. media::kCodecVP8
      const gfx::Size& size,    // input frame size
      int bit_rate,             // bits/second
      int frame_rate,           // frames/second
      int i_frame_interval,     // count
      int color_format);        // MediaCodecInfo.CodecCapabilities.

  void SetVideoBitrate(int bps);
  void RequestKeyFrameSoon();

  // Returns whether adaptive playback is supported for this object given
  // the new size.
  bool IsAdaptivePlaybackSupported(int width, int height);

  // Test-only method to set the return value of IsAdaptivePlaybackSupported().
  // Without this function, the return value of that function will be device
  // dependent. If |adaptive_playback_supported| is equal to 0, the return value
  // will be false. If |adaptive_playback_supported| is larger than 0, the
  // return value will be true.
  void set_adaptive_playback_supported_for_testing(
      int adaptive_playback_supported) {
    adaptive_playback_supported_for_testing_ = adaptive_playback_supported;
  }

 private:
  VideoCodecBridge(const std::string& mime,
                   bool is_secure,
                   MediaCodecDirection direction,
                   bool require_software_codec);

  int adaptive_playback_supported_for_testing_;
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_SDK_MEDIA_CODEC_BRIDGE_H_
