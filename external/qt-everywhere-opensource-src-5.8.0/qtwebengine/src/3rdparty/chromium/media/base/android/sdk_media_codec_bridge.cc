// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/sdk_media_codec_bridge.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "jni/MediaCodecBridge_jni.h"
#include "media/base/bit_reader.h"
#include "media/base/decrypt_config.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaIntArrayToIntVector;
using base::android::ScopedJavaLocalRef;

#define RETURN_ON_ERROR(condition)                             \
  do {                                                         \
    if (!(condition)) {                                        \
      LOG(ERROR) << "Unable to parse AAC header: " #condition; \
      return false;                                            \
    }                                                          \
  } while (0)

namespace media {

enum {
  kBufferFlagSyncFrame = 1,    // BUFFER_FLAG_SYNC_FRAME
  kBufferFlagEndOfStream = 4,  // BUFFER_FLAG_END_OF_STREAM
  kConfigureFlagEncode = 1,    // CONFIGURE_FLAG_ENCODE
};

static ScopedJavaLocalRef<jintArray>
ToJavaIntArray(JNIEnv* env, std::unique_ptr<jint[]> native_array, int size) {
  ScopedJavaLocalRef<jintArray> j_array(env, env->NewIntArray(size));
  env->SetIntArrayRegion(j_array.obj(), 0, size, native_array.get());
  return j_array;
}

static const std::string AudioCodecToAndroidMimeType(const AudioCodec& codec) {
  switch (codec) {
    case kCodecMP3:
      return "audio/mpeg";
    case kCodecVorbis:
      return "audio/vorbis";
    case kCodecOpus:
      return "audio/opus";
    case kCodecAAC:
      return "audio/mp4a-latm";
    default:
      return std::string();
  }
}

static const std::string VideoCodecToAndroidMimeType(const VideoCodec& codec) {
  switch (codec) {
    case kCodecH264:
      return "video/avc";
    case kCodecHEVC:
      return "video/hevc";
    case kCodecVP8:
      return "video/x-vnd.on2.vp8";
    case kCodecVP9:
      return "video/x-vnd.on2.vp9";
    default:
      return std::string();
  }
}

SdkMediaCodecBridge::SdkMediaCodecBridge(const std::string& mime,
                                         bool is_secure,
                                         MediaCodecDirection direction,
                                         bool require_software_codec) {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);
  DCHECK(!mime.empty());
  ScopedJavaLocalRef<jstring> j_mime = ConvertUTF8ToJavaString(env, mime);
  j_media_codec_.Reset(Java_MediaCodecBridge_create(
      env, j_mime.obj(), is_secure, direction, require_software_codec));
}

SdkMediaCodecBridge::~SdkMediaCodecBridge() {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);
  if (j_media_codec_.obj())
    Java_MediaCodecBridge_release(env, j_media_codec_.obj());
}

bool SdkMediaCodecBridge::Start() {
  JNIEnv* env = AttachCurrentThread();
  return Java_MediaCodecBridge_start(env, j_media_codec_.obj());
}

void SdkMediaCodecBridge::Stop() {
  JNIEnv* env = AttachCurrentThread();
  Java_MediaCodecBridge_stop(env, j_media_codec_.obj());
}

MediaCodecStatus SdkMediaCodecBridge::Flush() {
  JNIEnv* env = AttachCurrentThread();
  return static_cast<MediaCodecStatus>(
      Java_MediaCodecBridge_flush(env, j_media_codec_.obj()));
}

MediaCodecStatus SdkMediaCodecBridge::GetOutputSize(gfx::Size* size) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> result =
      Java_MediaCodecBridge_getOutputFormat(env, j_media_codec_.obj());
  MediaCodecStatus status = static_cast<MediaCodecStatus>(
      Java_GetOutputFormatResult_status(env, result.obj()));
  if (status == MEDIA_CODEC_OK) {
    size->SetSize(Java_GetOutputFormatResult_width(env, result.obj()),
                  Java_GetOutputFormatResult_height(env, result.obj()));
  }
  return status;
}

MediaCodecStatus SdkMediaCodecBridge::GetOutputSamplingRate(
    int* sampling_rate) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> result =
      Java_MediaCodecBridge_getOutputFormat(env, j_media_codec_.obj());
  MediaCodecStatus status = static_cast<MediaCodecStatus>(
      Java_GetOutputFormatResult_status(env, result.obj()));
  if (status == MEDIA_CODEC_OK)
    *sampling_rate = Java_GetOutputFormatResult_sampleRate(env, result.obj());
  return status;
}

MediaCodecStatus SdkMediaCodecBridge::GetOutputChannelCount(
    int* channel_count) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> result =
      Java_MediaCodecBridge_getOutputFormat(env, j_media_codec_.obj());
  MediaCodecStatus status = static_cast<MediaCodecStatus>(
      Java_GetOutputFormatResult_status(env, result.obj()));
  if (status == MEDIA_CODEC_OK)
    *channel_count = Java_GetOutputFormatResult_channelCount(env, result.obj());
  return status;
}

MediaCodecStatus SdkMediaCodecBridge::QueueInputBuffer(
    int index,
    const uint8_t* data,
    size_t data_size,
    const base::TimeDelta& presentation_time) {
  DVLOG(3) << __PRETTY_FUNCTION__ << index << ": " << data_size;
  if (data_size >
      base::checked_cast<size_t>(std::numeric_limits<int32_t>::max())) {
    return MEDIA_CODEC_ERROR;
  }
  if (data && !FillInputBuffer(index, data, data_size))
    return MEDIA_CODEC_ERROR;
  JNIEnv* env = AttachCurrentThread();
  return static_cast<MediaCodecStatus>(Java_MediaCodecBridge_queueInputBuffer(
      env, j_media_codec_.obj(), index, 0, data_size,
      presentation_time.InMicroseconds(), 0));
}

// TODO(timav): Combine this and above methods together keeping only the first
// interface after we switch to Spitzer pipeline.
MediaCodecStatus SdkMediaCodecBridge::QueueSecureInputBuffer(
    int index,
    const uint8_t* data,
    size_t data_size,
    const std::vector<char>& key_id,
    const std::vector<char>& iv,
    const SubsampleEntry* subsamples,
    int subsamples_size,
    const base::TimeDelta& presentation_time) {
  DVLOG(3) << __PRETTY_FUNCTION__ << index << ": " << data_size;
  if (data_size >
      base::checked_cast<size_t>(std::numeric_limits<int32_t>::max())) {
    return MEDIA_CODEC_ERROR;
  }
  if (data && !FillInputBuffer(index, data, data_size))
    return MEDIA_CODEC_ERROR;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_key_id = base::android::ToJavaByteArray(
      env, reinterpret_cast<const uint8_t*>(key_id.data()), key_id.size());
  ScopedJavaLocalRef<jbyteArray> j_iv = base::android::ToJavaByteArray(
      env, reinterpret_cast<const uint8_t*>(iv.data()), iv.size());

  // MediaCodec.CryptoInfo documentations says passing NULL for |clear_array|
  // to indicate that all data is encrypted. But it doesn't specify what
  // |cypher_array| and |subsamples_size| should be in that case. Passing
  // one subsample here just to be on the safe side.
  int new_subsamples_size = subsamples_size == 0 ? 1 : subsamples_size;

  std::unique_ptr<jint[]> native_clear_array(new jint[new_subsamples_size]);
  std::unique_ptr<jint[]> native_cypher_array(new jint[new_subsamples_size]);

  if (subsamples_size == 0) {
    DCHECK(!subsamples);
    native_clear_array[0] = 0;
    native_cypher_array[0] = data_size;
  } else {
    DCHECK_GT(subsamples_size, 0);
    DCHECK(subsamples);
    for (int i = 0; i < subsamples_size; ++i) {
      DCHECK(subsamples[i].clear_bytes <= std::numeric_limits<uint16_t>::max());
      if (subsamples[i].cypher_bytes >
          static_cast<uint32_t>(std::numeric_limits<jint>::max())) {
        return MEDIA_CODEC_ERROR;
      }

      native_clear_array[i] = subsamples[i].clear_bytes;
      native_cypher_array[i] = subsamples[i].cypher_bytes;
    }
  }

  ScopedJavaLocalRef<jintArray> clear_array =
      ToJavaIntArray(env, std::move(native_clear_array), new_subsamples_size);
  ScopedJavaLocalRef<jintArray> cypher_array =
      ToJavaIntArray(env, std::move(native_cypher_array), new_subsamples_size);

  return static_cast<MediaCodecStatus>(
      Java_MediaCodecBridge_queueSecureInputBuffer(
          env, j_media_codec_.obj(), index, 0, j_iv.obj(), j_key_id.obj(),
          clear_array.obj(), cypher_array.obj(), new_subsamples_size,
          presentation_time.InMicroseconds()));
}

void SdkMediaCodecBridge::QueueEOS(int input_buffer_index) {
  DVLOG(3) << __PRETTY_FUNCTION__ << ": " << input_buffer_index;
  JNIEnv* env = AttachCurrentThread();
  Java_MediaCodecBridge_queueInputBuffer(env, j_media_codec_.obj(),
                                         input_buffer_index, 0, 0, 0,
                                         kBufferFlagEndOfStream);
}

MediaCodecStatus SdkMediaCodecBridge::DequeueInputBuffer(
    const base::TimeDelta& timeout,
    int* index) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> result = Java_MediaCodecBridge_dequeueInputBuffer(
      env, j_media_codec_.obj(), timeout.InMicroseconds());
  *index = Java_DequeueInputResult_index(env, result.obj());
  MediaCodecStatus status = static_cast<MediaCodecStatus>(
      Java_DequeueInputResult_status(env, result.obj()));
  DVLOG(3) << __PRETTY_FUNCTION__ << ": status: " << status
           << ", index: " << *index;
  return status;
}

MediaCodecStatus SdkMediaCodecBridge::DequeueOutputBuffer(
    const base::TimeDelta& timeout,
    int* index,
    size_t* offset,
    size_t* size,
    base::TimeDelta* presentation_time,
    bool* end_of_stream,
    bool* key_frame) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> result =
      Java_MediaCodecBridge_dequeueOutputBuffer(env, j_media_codec_.obj(),
                                                timeout.InMicroseconds());
  *index = Java_DequeueOutputResult_index(env, result.obj());
  *offset = base::checked_cast<size_t>(
      Java_DequeueOutputResult_offset(env, result.obj()));
  *size = base::checked_cast<size_t>(
      Java_DequeueOutputResult_numBytes(env, result.obj()));
  if (presentation_time) {
    *presentation_time = base::TimeDelta::FromMicroseconds(
        Java_DequeueOutputResult_presentationTimeMicroseconds(env,
                                                              result.obj()));
  }
  int flags = Java_DequeueOutputResult_flags(env, result.obj());
  if (end_of_stream)
    *end_of_stream = flags & kBufferFlagEndOfStream;
  if (key_frame)
    *key_frame = flags & kBufferFlagSyncFrame;
  MediaCodecStatus status = static_cast<MediaCodecStatus>(
      Java_DequeueOutputResult_status(env, result.obj()));
  DVLOG(3) << __PRETTY_FUNCTION__ << ": status: " << status
           << ", index: " << *index << ", offset: " << *offset
           << ", size: " << *size << ", flags: " << flags;
  return status;
}

void SdkMediaCodecBridge::ReleaseOutputBuffer(int index, bool render) {
  DVLOG(3) << __PRETTY_FUNCTION__ << ": " << index;
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  Java_MediaCodecBridge_releaseOutputBuffer(env, j_media_codec_.obj(), index,
                                            render);
}

MediaCodecStatus SdkMediaCodecBridge::GetInputBuffer(int input_buffer_index,
                                                     uint8_t** data,
                                                     size_t* capacity) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_buffer(Java_MediaCodecBridge_getInputBuffer(
      env, j_media_codec_.obj(), input_buffer_index));
  if (j_buffer.is_null())
    return MEDIA_CODEC_ERROR;

  *data = static_cast<uint8_t*>(env->GetDirectBufferAddress(j_buffer.obj()));
  *capacity =
      base::checked_cast<size_t>(env->GetDirectBufferCapacity(j_buffer.obj()));
  return MEDIA_CODEC_OK;
}

MediaCodecStatus SdkMediaCodecBridge::GetOutputBufferAddress(
    int index,
    size_t offset,
    const uint8_t** addr,
    size_t* capacity) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_buffer(
      Java_MediaCodecBridge_getOutputBuffer(env, j_media_codec_.obj(), index));
  if (j_buffer.is_null())
    return MEDIA_CODEC_ERROR;
  const size_t total_capacity = env->GetDirectBufferCapacity(j_buffer.obj());
  CHECK_GE(total_capacity, offset);
  *addr = reinterpret_cast<const uint8_t*>(
              env->GetDirectBufferAddress(j_buffer.obj())) +
          offset;
  *capacity = total_capacity - offset;
  return MEDIA_CODEC_OK;
}

// static
bool SdkMediaCodecBridge::RegisterSdkMediaCodecBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
AudioCodecBridge* AudioCodecBridge::Create(const AudioCodec& codec) {
  if (!MediaCodecUtil::IsMediaCodecAvailable())
    return nullptr;

  const std::string mime = AudioCodecToAndroidMimeType(codec);
  if (mime.empty())
    return nullptr;

  std::unique_ptr<AudioCodecBridge> bridge(new AudioCodecBridge(mime));
  if (!bridge->media_codec())
    return nullptr;

  return bridge.release();
}

// static
bool AudioCodecBridge::IsKnownUnaccelerated(const AudioCodec& codec) {
  return MediaCodecUtil::IsKnownUnaccelerated(
      AudioCodecToAndroidMimeType(codec), MEDIA_CODEC_DECODER);
}

AudioCodecBridge::AudioCodecBridge(const std::string& mime)
    // Audio codec doesn't care about security level and there is no need for
    // audio encoding yet.
    : SdkMediaCodecBridge(mime, false, MEDIA_CODEC_DECODER, false) {}

bool AudioCodecBridge::ConfigureAndStart(const AudioDecoderConfig& config,
                                         bool play_audio,
                                         jobject media_crypto) {
  const int channel_count =
      ChannelLayoutToChannelCount(config.channel_layout());
  const int64_t codec_delay_ns = base::Time::kNanosecondsPerSecond *
                                 config.codec_delay() /
                                 config.samples_per_second();
  const int64_t seek_preroll_ns =
      1000LL * config.seek_preroll().InMicroseconds();

  return ConfigureAndStart(config.codec(), config.samples_per_second(),
                           channel_count, config.extra_data().data(),
                           config.extra_data().size(), codec_delay_ns,
                           seek_preroll_ns, play_audio, media_crypto);
}

bool AudioCodecBridge::ConfigureAndStart(const AudioCodec& codec,
                                         int sample_rate,
                                         int channel_count,
                                         const uint8_t* extra_data,
                                         size_t extra_data_size,
                                         int64_t codec_delay_ns,
                                         int64_t seek_preroll_ns,
                                         bool play_audio,
                                         jobject media_crypto) {
  DVLOG(2) << __FUNCTION__ << ": "
           << " codec:" << GetCodecName(codec)
           << " samples_per_second:" << sample_rate
           << " channel_count:" << channel_count
           << " codec_delay_ns:" << codec_delay_ns
           << " seek_preroll_ns:" << seek_preroll_ns
           << " extra data size:" << extra_data_size
           << " play audio:" << play_audio << " media_crypto:" << media_crypto;
  DCHECK(media_codec());

  std::string codec_string = AudioCodecToAndroidMimeType(codec);
  if (codec_string.empty())
    return false;

  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jstring> j_mime =
      ConvertUTF8ToJavaString(env, codec_string);
  ScopedJavaLocalRef<jobject> j_format(Java_MediaCodecBridge_createAudioFormat(
      env, j_mime.obj(), sample_rate, channel_count));
  DCHECK(!j_format.is_null());

  if (!ConfigureMediaFormat(j_format.obj(), codec, extra_data, extra_data_size,
                            codec_delay_ns, seek_preroll_ns)) {
    return false;
  }

  if (!Java_MediaCodecBridge_configureAudio(env, media_codec(), j_format.obj(),
                                            media_crypto, 0, play_audio)) {
    return false;
  }

  return Start();
}

bool AudioCodecBridge::ConfigureMediaFormat(jobject j_format,
                                            const AudioCodec& codec,
                                            const uint8_t* extra_data,
                                            size_t extra_data_size,
                                            int64_t codec_delay_ns,
                                            int64_t seek_preroll_ns) {
  if (extra_data_size == 0 && codec != kCodecOpus)
    return true;

  JNIEnv* env = AttachCurrentThread();
  switch (codec) {
    case kCodecVorbis: {
      if (extra_data[0] != 2) {
        LOG(ERROR) << "Invalid number of vorbis headers before the codec "
                   << "header: " << extra_data[0];
        return false;
      }

      size_t header_length[2];
      // |total_length| keeps track of the total number of bytes before the last
      // header.
      size_t total_length = 1;
      const uint8_t* current_pos = extra_data;
      // Calculate the length of the first 2 headers.
      for (int i = 0; i < 2; ++i) {
        header_length[i] = 0;
        while (total_length < extra_data_size) {
          size_t size = *(++current_pos);
          total_length += 1 + size;
          if (total_length > 0x80000000) {
            LOG(ERROR) << "Vorbis header size too large";
            return false;
          }
          header_length[i] += size;
          if (size < 0xFF)
            break;
        }
        if (total_length >= extra_data_size) {
          LOG(ERROR) << "Invalid vorbis header size in the extra data";
          return false;
        }
      }
      current_pos++;
      // The first header is identification header.
      ScopedJavaLocalRef<jbyteArray> first_header =
          base::android::ToJavaByteArray(env, current_pos, header_length[0]);
      Java_MediaCodecBridge_setCodecSpecificData(env, j_format, 0,
                                                 first_header.obj());
      // The last header is codec header.
      ScopedJavaLocalRef<jbyteArray> last_header =
          base::android::ToJavaByteArray(env, extra_data + total_length,
                                         extra_data_size - total_length);
      Java_MediaCodecBridge_setCodecSpecificData(env, j_format, 1,
                                                 last_header.obj());
      break;
    }
    case kCodecAAC: {
      media::BitReader reader(extra_data, extra_data_size);

      // The following code is copied from aac.cc
      // TODO(qinmin): refactor the code in aac.cc to make it more reusable.
      uint8_t profile = 0;
      uint8_t frequency_index = 0;
      uint8_t channel_config = 0;
      RETURN_ON_ERROR(reader.ReadBits(5, &profile));
      RETURN_ON_ERROR(reader.ReadBits(4, &frequency_index));

      if (0xf == frequency_index)
        RETURN_ON_ERROR(reader.SkipBits(24));
      RETURN_ON_ERROR(reader.ReadBits(4, &channel_config));

      if (profile == 5 || profile == 29) {
        // Read extension config.
        RETURN_ON_ERROR(reader.ReadBits(4, &frequency_index));
        if (frequency_index == 0xf)
          RETURN_ON_ERROR(reader.SkipBits(24));
        RETURN_ON_ERROR(reader.ReadBits(5, &profile));
      }

      if (profile < 1 || profile > 4 || frequency_index == 0xf ||
          channel_config > 7) {
        LOG(ERROR) << "Invalid AAC header";
        return false;
      }

      const size_t kCsdLength = 2;
      uint8_t csd[kCsdLength];
      csd[0] = profile << 3 | frequency_index >> 1;
      csd[1] = (frequency_index & 0x01) << 7 | channel_config << 3;
      ScopedJavaLocalRef<jbyteArray> byte_array =
          base::android::ToJavaByteArray(env, csd, kCsdLength);
      Java_MediaCodecBridge_setCodecSpecificData(env, j_format, 0,
                                                 byte_array.obj());

      // TODO(qinmin): pass an extra variable to this function to determine
      // whether we need to call this.
      Java_MediaCodecBridge_setFrameHasADTSHeader(env, j_format);
      break;
    }
    case kCodecOpus: {
      if (!extra_data || extra_data_size == 0 || codec_delay_ns < 0 ||
          seek_preroll_ns < 0) {
        LOG(ERROR) << "Invalid Opus Header";
        return false;
      }

      // csd0 - Opus Header
      ScopedJavaLocalRef<jbyteArray> csd0 =
          base::android::ToJavaByteArray(env, extra_data, extra_data_size);
      Java_MediaCodecBridge_setCodecSpecificData(env, j_format, 0, csd0.obj());

      // csd1 - Codec Delay
      ScopedJavaLocalRef<jbyteArray> csd1 = base::android::ToJavaByteArray(
          env, reinterpret_cast<const uint8_t*>(&codec_delay_ns),
          sizeof(int64_t));
      Java_MediaCodecBridge_setCodecSpecificData(env, j_format, 1, csd1.obj());

      // csd2 - Seek Preroll
      ScopedJavaLocalRef<jbyteArray> csd2 = base::android::ToJavaByteArray(
          env, reinterpret_cast<const uint8_t*>(&seek_preroll_ns),
          sizeof(int64_t));
      Java_MediaCodecBridge_setCodecSpecificData(env, j_format, 2, csd2.obj());
      break;
    }
    default:
      LOG(ERROR) << "Invalid header encountered for codec: "
                 << AudioCodecToAndroidMimeType(codec);
      return false;
  }
  return true;
}

bool AudioCodecBridge::CreateAudioTrack(int sampling_rate, int channel_count) {
  DVLOG(2) << __FUNCTION__ << ": samping_rate:" << sampling_rate
           << " channel_count:" << channel_count;

  JNIEnv* env = AttachCurrentThread();
  return Java_MediaCodecBridge_createAudioTrack(env, media_codec(),
                                                sampling_rate, channel_count);
}

MediaCodecStatus AudioCodecBridge::PlayOutputBuffer(int index,
                                                    size_t size,
                                                    size_t offset,
                                                    bool postpone,
                                                    int64_t* playback_pos) {
  DCHECK_LE(0, index);
  int numBytes = base::checked_cast<int>(size);

  const uint8_t* buffer = nullptr;
  size_t capacity = 0;
  MediaCodecStatus status =
      GetOutputBufferAddress(index, offset, &buffer, &capacity);
  if (status != MEDIA_CODEC_OK) {
    DLOG(ERROR) << __FUNCTION__
                << ": GetOutputBufferAddress() failed for index:" << index;
    return status;
  }

  numBytes = std::min(base::checked_cast<int>(capacity), numBytes);
  CHECK_GE(numBytes, 0);

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> byte_array =
      base::android::ToJavaByteArray(env, buffer, numBytes);
  *playback_pos = Java_MediaCodecBridge_playOutputBuffer(
      env, media_codec(), byte_array.obj(), postpone);
  return status;
}

void AudioCodecBridge::SetVolume(double volume) {
  JNIEnv* env = AttachCurrentThread();
  Java_MediaCodecBridge_setVolume(env, media_codec(), volume);
}

// static
bool VideoCodecBridge::IsKnownUnaccelerated(const VideoCodec& codec,
                                            MediaCodecDirection direction) {
  return MediaCodecUtil::IsKnownUnaccelerated(
      VideoCodecToAndroidMimeType(codec), direction);
}

// static
VideoCodecBridge* VideoCodecBridge::CreateDecoder(const VideoCodec& codec,
                                                  bool is_secure,
                                                  const gfx::Size& size,
                                                  jobject surface,
                                                  jobject media_crypto,
                                                  bool allow_adaptive_playback,
                                                  bool require_software_codec) {
  if (!MediaCodecUtil::IsMediaCodecAvailable())
    return nullptr;

  const std::string mime = VideoCodecToAndroidMimeType(codec);
  if (mime.empty())
    return nullptr;

  std::unique_ptr<VideoCodecBridge> bridge(new VideoCodecBridge(
      mime, is_secure, MEDIA_CODEC_DECODER, require_software_codec));
  if (!bridge->media_codec())
    return nullptr;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_mime = ConvertUTF8ToJavaString(env, mime);
  ScopedJavaLocalRef<jobject> j_format(
      Java_MediaCodecBridge_createVideoDecoderFormat(
          env, j_mime.obj(), size.width(), size.height()));
  DCHECK(!j_format.is_null());
  if (!Java_MediaCodecBridge_configureVideo(
          env, bridge->media_codec(), j_format.obj(), surface, media_crypto, 0,
          allow_adaptive_playback)) {
    return nullptr;
  }

  return bridge->Start() ? bridge.release() : nullptr;
}

// static
VideoCodecBridge* VideoCodecBridge::CreateEncoder(const VideoCodec& codec,
                                                  const gfx::Size& size,
                                                  int bit_rate,
                                                  int frame_rate,
                                                  int i_frame_interval,
                                                  int color_format) {
  if (!MediaCodecUtil::IsMediaCodecAvailable())
    return nullptr;

  const std::string mime = VideoCodecToAndroidMimeType(codec);
  if (mime.empty())
    return nullptr;

  std::unique_ptr<VideoCodecBridge> bridge(
      new VideoCodecBridge(mime, false, MEDIA_CODEC_ENCODER, false));
  if (!bridge->media_codec())
    return nullptr;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_mime = ConvertUTF8ToJavaString(env, mime);
  ScopedJavaLocalRef<jobject> j_format(
      Java_MediaCodecBridge_createVideoEncoderFormat(
          env, j_mime.obj(), size.width(), size.height(), bit_rate, frame_rate,
          i_frame_interval, color_format));
  DCHECK(!j_format.is_null());
  if (!Java_MediaCodecBridge_configureVideo(env, bridge->media_codec(),
                                            j_format.obj(), nullptr, nullptr,
                                            kConfigureFlagEncode, true)) {
    return nullptr;
  }

  return bridge->Start() ? bridge.release() : nullptr;
}

VideoCodecBridge::VideoCodecBridge(const std::string& mime,
                                   bool is_secure,
                                   MediaCodecDirection direction,
                                   bool require_software_codec)
    : SdkMediaCodecBridge(mime, is_secure, direction, require_software_codec),
      adaptive_playback_supported_for_testing_(-1) {}

void VideoCodecBridge::SetVideoBitrate(int bps) {
  JNIEnv* env = AttachCurrentThread();
  Java_MediaCodecBridge_setVideoBitrate(env, media_codec(), bps);
}

void VideoCodecBridge::RequestKeyFrameSoon() {
  JNIEnv* env = AttachCurrentThread();
  Java_MediaCodecBridge_requestKeyFrameSoon(env, media_codec());
}

bool VideoCodecBridge::IsAdaptivePlaybackSupported(int width, int height) {
  if (adaptive_playback_supported_for_testing_ == 0)
    return false;
  else if (adaptive_playback_supported_for_testing_ > 0)
    return true;
  JNIEnv* env = AttachCurrentThread();
  return Java_MediaCodecBridge_isAdaptivePlaybackSupported(env, media_codec(),
                                                           width, height);
}

}  // namespace media
