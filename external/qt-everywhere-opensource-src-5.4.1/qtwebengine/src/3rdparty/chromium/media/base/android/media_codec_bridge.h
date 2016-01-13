// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_CODEC_BRIDGE_H_
#define MEDIA_BASE_ANDROID_MEDIA_CODEC_BRIDGE_H_

#include <jni.h>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/time/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"
#include "ui/gfx/size.h"

namespace media {

struct SubsampleEntry;

// These must be in sync with MediaCodecBridge.MEDIA_CODEC_XXX constants in
// MediaCodecBridge.java.
enum MediaCodecStatus {
  MEDIA_CODEC_OK,
  MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER,
  MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER,
  MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED,
  MEDIA_CODEC_OUTPUT_FORMAT_CHANGED,
  MEDIA_CODEC_INPUT_END_OF_STREAM,
  MEDIA_CODEC_OUTPUT_END_OF_STREAM,
  MEDIA_CODEC_NO_KEY,
  MEDIA_CODEC_STOPPED,
  MEDIA_CODEC_ERROR
};

// Codec direction.  Keep this in sync with MediaCodecBridge.java.
enum MediaCodecDirection {
  MEDIA_CODEC_DECODER,
  MEDIA_CODEC_ENCODER,
};

// This class serves as a bridge for native code to call java functions inside
// Android MediaCodec class. For more information on Android MediaCodec, check
// http://developer.android.com/reference/android/media/MediaCodec.html
// Note: MediaCodec is only available on JB and greater.
// Use AudioCodecBridge or VideoCodecBridge to create an instance of this
// object.
//
// TODO(fischman,xhwang): replace this (and the enums that go with it) with
// chromium's JNI auto-generation hotness.
class MEDIA_EXPORT MediaCodecBridge {
 public:
  // Returns true if MediaCodec is available on the device.
  // All other static methods check IsAvailable() internally. There's no need
  // to check IsAvailable() explicitly before calling them.
  static bool IsAvailable();

  // Returns true if MediaCodec.setParameters() is available on the device.
  static bool SupportsSetParameters();

  // Returns whether MediaCodecBridge has a decoder that |is_secure| and can
  // decode |codec| type.
  static bool CanDecode(const std::string& codec, bool is_secure);

  // Represents supported codecs on android.
  // TODO(qinmin): Currently the codecs string only contains one codec. Do we
  // need to support codecs separated by comma. (e.g. "vp8" -> "vp8, vp8.0")?
  struct CodecsInfo {
    std::string codecs;  // E.g. "vp8" or "avc1".
    std::string name;    // E.g. "OMX.google.vp8.decoder".
    MediaCodecDirection direction;
  };

  // Get a list of supported codecs.
  static std::vector<CodecsInfo> GetCodecsInfo();

  virtual ~MediaCodecBridge();

  // Resets both input and output, all indices previously returned in calls to
  // DequeueInputBuffer() and DequeueOutputBuffer() become invalid.
  // Please note that this clears all the inputs in the media codec. In other
  // words, there will be no outputs until new input is provided.
  // Returns MEDIA_CODEC_ERROR if an unexpected error happens, or Media_CODEC_OK
  // otherwise.
  MediaCodecStatus Reset();

  // Finishes the decode/encode session. The instance remains active
  // and ready to be StartAudio/Video()ed again. HOWEVER, due to the buggy
  // vendor's implementation , b/8125974, Stop() -> StartAudio/Video() may not
  // work on some devices. For reliability, Stop() -> delete and recreate new
  // instance -> StartAudio/Video() is recommended.
  void Stop();

  // Used for getting output format. This is valid after DequeueInputBuffer()
  // returns a format change by returning INFO_OUTPUT_FORMAT_CHANGED
  void GetOutputFormat(int* width, int* height);

  // Returns the number of input buffers used by the codec.
  int GetInputBuffersCount();

  // Submits a byte array to the given input buffer. Call this after getting an
  // available buffer from DequeueInputBuffer().  If |data| is NULL, assume the
  // input buffer has already been populated (but still obey |size|).
  // |data_size| must be less than kint32max (because Java).
  MediaCodecStatus QueueInputBuffer(int index,
                                    const uint8* data,
                                    size_t data_size,
                                    const base::TimeDelta& presentation_time);

  // Similar to the above call, but submits a buffer that is encrypted.  Note:
  // NULL |subsamples| indicates the whole buffer is encrypted.  If |data| is
  // NULL, assume the input buffer has already been populated (but still obey
  // |data_size|).  |data_size| must be less than kint32max (because Java).
  MediaCodecStatus QueueSecureInputBuffer(
      int index,
      const uint8* data,
      size_t data_size,
      const uint8* key_id,
      int key_id_size,
      const uint8* iv,
      int iv_size,
      const SubsampleEntry* subsamples,
      int subsamples_size,
      const base::TimeDelta& presentation_time);

  // Submits an empty buffer with a EOS (END OF STREAM) flag.
  void QueueEOS(int input_buffer_index);

  // Returns:
  // MEDIA_CODEC_OK if an input buffer is ready to be filled with valid data,
  // MEDIA_CODEC_ENQUEUE_INPUT_AGAIN_LATER if no such buffer is available, or
  // MEDIA_CODEC_ERROR if unexpected error happens.
  // Note: Never use infinite timeout as this would block the decoder thread and
  // prevent the decoder job from being released.
  MediaCodecStatus DequeueInputBuffer(const base::TimeDelta& timeout,
                                      int* index);

  // Dequeues an output buffer, block at most timeout_us microseconds.
  // Returns the status of this operation. If OK is returned, the output
  // parameters should be populated. Otherwise, the values of output parameters
  // should not be used.  Output parameters other than index/offset/size are
  // optional and only set if not NULL.
  // Note: Never use infinite timeout as this would block the decoder thread and
  // prevent the decoder job from being released.
  // TODO(xhwang): Can we drop |end_of_stream| and return
  // MEDIA_CODEC_OUTPUT_END_OF_STREAM?
  MediaCodecStatus DequeueOutputBuffer(const base::TimeDelta& timeout,
                                       int* index,
                                       size_t* offset,
                                       size_t* size,
                                       base::TimeDelta* presentation_time,
                                       bool* end_of_stream,
                                       bool* key_frame);

  // Returns the buffer to the codec. If you previously specified a surface when
  // configuring this video decoder you can optionally render the buffer.
  void ReleaseOutputBuffer(int index, bool render);

  // Returns the number of output buffers used by the codec.
  int GetOutputBuffersCount();

  // Returns the capacity of each output buffer used by the codec.
  size_t GetOutputBuffersCapacity();

  // Gets output buffers from media codec and keeps them inside the java class.
  // To access them, use DequeueOutputBuffer(). Returns whether output buffers
  // were successfully obtained.
  bool GetOutputBuffers() WARN_UNUSED_RESULT;

  // Returns an input buffer's base pointer and capacity.
  void GetInputBuffer(int input_buffer_index, uint8** data, size_t* capacity);

  // Copy |dst_size| bytes from output buffer |index|'s |offset| onwards into
  // |*dst|.
  bool CopyFromOutputBuffer(int index, size_t offset, void* dst, int dst_size);

  static bool RegisterMediaCodecBridge(JNIEnv* env);

 protected:
  // Returns true if |mime_type| is known to be unaccelerated (i.e. backed by a
  // software codec instead of a hardware one).
  static bool IsKnownUnaccelerated(const std::string& mime_type,
                                   MediaCodecDirection direction);

  MediaCodecBridge(const std::string& mime,
                   bool is_secure,
                   MediaCodecDirection direction);

  // Calls start() against the media codec instance. Used in StartXXX() after
  // configuring media codec. Returns whether media codec was successfully
  // started.
  bool StartInternal() WARN_UNUSED_RESULT;

  jobject media_codec() { return j_media_codec_.obj(); }
  MediaCodecDirection direction_;

 private:
  // Fills a particular input buffer; returns false if |data_size| exceeds the
  // input buffer's capacity (and doesn't touch the input buffer in that case).
  bool FillInputBuffer(int index,
                       const uint8* data,
                       size_t data_size) WARN_UNUSED_RESULT;

  // Java MediaCodec instance.
  base::android::ScopedJavaGlobalRef<jobject> j_media_codec_;

  DISALLOW_COPY_AND_ASSIGN(MediaCodecBridge);
};

class AudioCodecBridge : public MediaCodecBridge {
 public:
  // Returns an AudioCodecBridge instance if |codec| is supported, or a NULL
  // pointer otherwise.
  static AudioCodecBridge* Create(const AudioCodec& codec);

  // See MediaCodecBridge::IsKnownUnaccelerated().
  static bool IsKnownUnaccelerated(const AudioCodec& codec);

  // Start the audio codec bridge.
  bool Start(const AudioCodec& codec, int sample_rate, int channel_count,
             const uint8* extra_data, size_t extra_data_size,
             bool play_audio, jobject media_crypto) WARN_UNUSED_RESULT;

  // Play the output buffer. This call must be called after
  // DequeueOutputBuffer() and before ReleaseOutputBuffer. Returns the playback
  // head position expressed in frames.
  int64 PlayOutputBuffer(int index, size_t size);

  // Set the volume of the audio output.
  void SetVolume(double volume);

 private:
  explicit AudioCodecBridge(const std::string& mime);

  // Configure the java MediaFormat object with the extra codec data passed in.
  bool ConfigureMediaFormat(jobject j_format, const AudioCodec& codec,
                            const uint8* extra_data, size_t extra_data_size);
};

class MEDIA_EXPORT VideoCodecBridge : public MediaCodecBridge {
 public:
  // See MediaCodecBridge::IsKnownUnaccelerated().
  static bool IsKnownUnaccelerated(const VideoCodec& codec,
                                   MediaCodecDirection direction);

  // Create, start, and return a VideoCodecBridge decoder or NULL on failure.
  static VideoCodecBridge* CreateDecoder(
      const VideoCodec& codec,  // e.g. media::kCodecVP8
      bool is_secure,
      const gfx::Size& size,  // Output frame size.
      jobject surface,        // Output surface, optional.
      jobject media_crypto);  // MediaCrypto object, optional.

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
                   MediaCodecDirection direction);

  int adaptive_playback_supported_for_testing_;
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_CODEC_BRIDGE_H_
