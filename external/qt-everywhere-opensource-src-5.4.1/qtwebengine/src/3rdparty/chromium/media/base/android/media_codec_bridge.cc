// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_codec_bridge.h"

#include <jni.h>
#include <string>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "jni/MediaCodecBridge_jni.h"
#include "media/base/bit_reader.h"
#include "media/base/decrypt_config.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace media {

enum {
  kBufferFlagSyncFrame = 1,    // BUFFER_FLAG_SYNC_FRAME
  kBufferFlagEndOfStream = 4,  // BUFFER_FLAG_END_OF_STREAM
  kConfigureFlagEncode = 1,    // CONFIGURE_FLAG_ENCODE
};

static const std::string AudioCodecToAndroidMimeType(const AudioCodec& codec) {
  switch (codec) {
    case kCodecMP3:
      return "audio/mpeg";
    case kCodecVorbis:
      return "audio/vorbis";
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
    case kCodecVP8:
      return "video/x-vnd.on2.vp8";
    case kCodecVP9:
      return "video/x-vnd.on2.vp9";
    default:
      return std::string();
  }
}

static const std::string CodecTypeToAndroidMimeType(const std::string& codec) {
  // TODO(xhwang): Shall we handle more detailed strings like "mp4a.40.2"?
  if (codec == "avc1")
    return "video/avc";
  if (codec == "mp4a")
    return "audio/mp4a-latm";
  if (codec == "vp8" || codec == "vp8.0")
    return "video/x-vnd.on2.vp8";
  if (codec == "vp9" || codec == "vp9.0")
    return "video/x-vnd.on2.vp9";
  if (codec == "vorbis")
    return "audio/vorbis";
  return std::string();
}

// TODO(qinmin): using a map to help all the conversions in this class.
static const std::string AndroidMimeTypeToCodecType(const std::string& mime) {
  if (mime == "video/mp4v-es")
    return "mp4v";
  if (mime == "video/avc")
    return "avc1";
  if (mime == "video/x-vnd.on2.vp8")
    return "vp8";
  if (mime == "video/x-vnd.on2.vp9")
    return "vp9";
  if (mime == "audio/mp4a-latm")
    return "mp4a";
  if (mime == "audio/mpeg")
    return "mp3";
  if (mime == "audio/vorbis")
    return "vorbis";
  return std::string();
}

static ScopedJavaLocalRef<jintArray>
ToJavaIntArray(JNIEnv* env, scoped_ptr<jint[]> native_array, int size) {
  ScopedJavaLocalRef<jintArray> j_array(env, env->NewIntArray(size));
  env->SetIntArrayRegion(j_array.obj(), 0, size, native_array.get());
  return j_array;
}

// static
bool MediaCodecBridge::IsAvailable() {
  // MediaCodec is only available on JB and greater.
  return base::android::BuildInfo::GetInstance()->sdk_int() >= 16;
}

// static
bool MediaCodecBridge::SupportsSetParameters() {
  // MediaCodec.setParameters() is only available starting with K.
  return base::android::BuildInfo::GetInstance()->sdk_int() >= 19;
}

// static
std::vector<MediaCodecBridge::CodecsInfo> MediaCodecBridge::GetCodecsInfo() {
  std::vector<CodecsInfo> codecs_info;
  if (!IsAvailable())
    return codecs_info;

  JNIEnv* env = AttachCurrentThread();
  std::string mime_type;
  ScopedJavaLocalRef<jobjectArray> j_codec_info_array =
      Java_MediaCodecBridge_getCodecsInfo(env);
  jsize len = env->GetArrayLength(j_codec_info_array.obj());
  for (jsize i = 0; i < len; ++i) {
    ScopedJavaLocalRef<jobject> j_info(
        env, env->GetObjectArrayElement(j_codec_info_array.obj(), i));
    ScopedJavaLocalRef<jstring> j_codec_type =
        Java_CodecInfo_codecType(env, j_info.obj());
    ConvertJavaStringToUTF8(env, j_codec_type.obj(), &mime_type);
    ScopedJavaLocalRef<jstring> j_codec_name =
        Java_CodecInfo_codecName(env, j_info.obj());
    CodecsInfo info;
    info.codecs = AndroidMimeTypeToCodecType(mime_type);
    ConvertJavaStringToUTF8(env, j_codec_name.obj(), &info.name);
    info.direction = static_cast<MediaCodecDirection>(
        Java_CodecInfo_direction(env, j_info.obj()));
    codecs_info.push_back(info);
  }
  return codecs_info;
}

// static
bool MediaCodecBridge::CanDecode(const std::string& codec, bool is_secure) {
  if (!IsAvailable())
    return false;

  JNIEnv* env = AttachCurrentThread();
  std::string mime = CodecTypeToAndroidMimeType(codec);
  if (mime.empty())
    return false;
  ScopedJavaLocalRef<jstring> j_mime = ConvertUTF8ToJavaString(env, mime);
  ScopedJavaLocalRef<jobject> j_media_codec_bridge =
      Java_MediaCodecBridge_create(env, j_mime.obj(), is_secure, false);
  if (!j_media_codec_bridge.is_null()) {
    Java_MediaCodecBridge_release(env, j_media_codec_bridge.obj());
    return true;
  }
  return false;
}

// static
bool MediaCodecBridge::IsKnownUnaccelerated(const std::string& mime_type,
                                            MediaCodecDirection direction) {
  if (!IsAvailable())
    return true;

  std::string codec_type = AndroidMimeTypeToCodecType(mime_type);
  std::vector<media::MediaCodecBridge::CodecsInfo> codecs_info =
      MediaCodecBridge::GetCodecsInfo();
  for (size_t i = 0; i < codecs_info.size(); ++i) {
    if (codecs_info[i].codecs == codec_type &&
        codecs_info[i].direction == direction) {
      // It would be nice if MediaCodecInfo externalized some notion of
      // HW-acceleration but it doesn't. Android Media guidance is that the
      // prefix below is always used for SW decoders, so that's what we use.
      return StartsWithASCII(codecs_info[i].name, "OMX.google.", true);
    }
  }
  return true;
}

MediaCodecBridge::MediaCodecBridge(const std::string& mime,
                                   bool is_secure,
                                   MediaCodecDirection direction) {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);
  DCHECK(!mime.empty());
  ScopedJavaLocalRef<jstring> j_mime = ConvertUTF8ToJavaString(env, mime);
  j_media_codec_.Reset(
      Java_MediaCodecBridge_create(env, j_mime.obj(), is_secure, direction));
}

MediaCodecBridge::~MediaCodecBridge() {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);
  if (j_media_codec_.obj())
    Java_MediaCodecBridge_release(env, j_media_codec_.obj());
}

bool MediaCodecBridge::StartInternal() {
  JNIEnv* env = AttachCurrentThread();
  return Java_MediaCodecBridge_start(env, j_media_codec_.obj()) &&
         GetOutputBuffers();
}

MediaCodecStatus MediaCodecBridge::Reset() {
  JNIEnv* env = AttachCurrentThread();
  return static_cast<MediaCodecStatus>(
      Java_MediaCodecBridge_flush(env, j_media_codec_.obj()));
}

void MediaCodecBridge::Stop() {
  JNIEnv* env = AttachCurrentThread();
  Java_MediaCodecBridge_stop(env, j_media_codec_.obj());
}

void MediaCodecBridge::GetOutputFormat(int* width, int* height) {
  JNIEnv* env = AttachCurrentThread();

  *width = Java_MediaCodecBridge_getOutputWidth(env, j_media_codec_.obj());
  *height = Java_MediaCodecBridge_getOutputHeight(env, j_media_codec_.obj());
}

MediaCodecStatus MediaCodecBridge::QueueInputBuffer(
    int index,
    const uint8* data,
    size_t data_size,
    const base::TimeDelta& presentation_time) {
  DVLOG(3) << __PRETTY_FUNCTION__ << index << ": " << data_size;
  if (data_size > base::checked_cast<size_t>(kint32max))
    return MEDIA_CODEC_ERROR;
  if (data && !FillInputBuffer(index, data, data_size))
    return MEDIA_CODEC_ERROR;
  JNIEnv* env = AttachCurrentThread();
  return static_cast<MediaCodecStatus>(
      Java_MediaCodecBridge_queueInputBuffer(env,
                                             j_media_codec_.obj(),
                                             index,
                                             0,
                                             data_size,
                                             presentation_time.InMicroseconds(),
                                             0));
}

MediaCodecStatus MediaCodecBridge::QueueSecureInputBuffer(
    int index,
    const uint8* data,
    size_t data_size,
    const uint8* key_id,
    int key_id_size,
    const uint8* iv,
    int iv_size,
    const SubsampleEntry* subsamples,
    int subsamples_size,
    const base::TimeDelta& presentation_time) {
  DVLOG(3) << __PRETTY_FUNCTION__ << index << ": " << data_size;
  if (data_size > base::checked_cast<size_t>(kint32max))
    return MEDIA_CODEC_ERROR;
  if (data && !FillInputBuffer(index, data, data_size))
    return MEDIA_CODEC_ERROR;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_key_id =
      base::android::ToJavaByteArray(env, key_id, key_id_size);
  ScopedJavaLocalRef<jbyteArray> j_iv =
      base::android::ToJavaByteArray(env, iv, iv_size);

  // MediaCodec.CryptoInfo documentations says passing NULL for |clear_array|
  // to indicate that all data is encrypted. But it doesn't specify what
  // |cypher_array| and |subsamples_size| should be in that case. Passing
  // one subsample here just to be on the safe side.
  int new_subsamples_size = subsamples_size == 0 ? 1 : subsamples_size;

  scoped_ptr<jint[]> native_clear_array(new jint[new_subsamples_size]);
  scoped_ptr<jint[]> native_cypher_array(new jint[new_subsamples_size]);

  if (subsamples_size == 0) {
    DCHECK(!subsamples);
    native_clear_array[0] = 0;
    native_cypher_array[0] = data_size;
  } else {
    DCHECK_GT(subsamples_size, 0);
    DCHECK(subsamples);
    for (int i = 0; i < subsamples_size; ++i) {
      DCHECK(subsamples[i].clear_bytes <= std::numeric_limits<uint16>::max());
      if (subsamples[i].cypher_bytes >
          static_cast<uint32>(std::numeric_limits<jint>::max())) {
        return MEDIA_CODEC_ERROR;
      }

      native_clear_array[i] = subsamples[i].clear_bytes;
      native_cypher_array[i] = subsamples[i].cypher_bytes;
    }
  }

  ScopedJavaLocalRef<jintArray> clear_array =
      ToJavaIntArray(env, native_clear_array.Pass(), new_subsamples_size);
  ScopedJavaLocalRef<jintArray> cypher_array =
      ToJavaIntArray(env, native_cypher_array.Pass(), new_subsamples_size);

  return static_cast<MediaCodecStatus>(
      Java_MediaCodecBridge_queueSecureInputBuffer(
          env,
          j_media_codec_.obj(),
          index,
          0,
          j_iv.obj(),
          j_key_id.obj(),
          clear_array.obj(),
          cypher_array.obj(),
          new_subsamples_size,
          presentation_time.InMicroseconds()));
}

void MediaCodecBridge::QueueEOS(int input_buffer_index) {
  DVLOG(3) << __PRETTY_FUNCTION__ << ": " << input_buffer_index;
  JNIEnv* env = AttachCurrentThread();
  Java_MediaCodecBridge_queueInputBuffer(env,
                                         j_media_codec_.obj(),
                                         input_buffer_index,
                                         0,
                                         0,
                                         0,
                                         kBufferFlagEndOfStream);
}

MediaCodecStatus MediaCodecBridge::DequeueInputBuffer(
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

MediaCodecStatus MediaCodecBridge::DequeueOutputBuffer(
    const base::TimeDelta& timeout,
    int* index,
    size_t* offset,
    size_t* size,
    base::TimeDelta* presentation_time,
    bool* end_of_stream,
    bool* key_frame) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> result =
      Java_MediaCodecBridge_dequeueOutputBuffer(
          env, j_media_codec_.obj(), timeout.InMicroseconds());
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

void MediaCodecBridge::ReleaseOutputBuffer(int index, bool render) {
  DVLOG(3) << __PRETTY_FUNCTION__ << ": " << index;
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  Java_MediaCodecBridge_releaseOutputBuffer(
      env, j_media_codec_.obj(), index, render);
}

int MediaCodecBridge::GetInputBuffersCount() {
  JNIEnv* env = AttachCurrentThread();
  return Java_MediaCodecBridge_getInputBuffersCount(env, j_media_codec_.obj());
}

int MediaCodecBridge::GetOutputBuffersCount() {
  JNIEnv* env = AttachCurrentThread();
  return Java_MediaCodecBridge_getOutputBuffersCount(env, j_media_codec_.obj());
}

size_t MediaCodecBridge::GetOutputBuffersCapacity() {
  JNIEnv* env = AttachCurrentThread();
  return Java_MediaCodecBridge_getOutputBuffersCapacity(env,
                                                        j_media_codec_.obj());
}

bool MediaCodecBridge::GetOutputBuffers() {
  JNIEnv* env = AttachCurrentThread();
  return Java_MediaCodecBridge_getOutputBuffers(env, j_media_codec_.obj());
}

void MediaCodecBridge::GetInputBuffer(int input_buffer_index,
                                      uint8** data,
                                      size_t* capacity) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_buffer(Java_MediaCodecBridge_getInputBuffer(
      env, j_media_codec_.obj(), input_buffer_index));
  *data = static_cast<uint8*>(env->GetDirectBufferAddress(j_buffer.obj()));
  *capacity = base::checked_cast<size_t>(
      env->GetDirectBufferCapacity(j_buffer.obj()));
}

bool MediaCodecBridge::CopyFromOutputBuffer(int index,
                                            size_t offset,
                                            void* dst,
                                            int dst_size) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_buffer(
      Java_MediaCodecBridge_getOutputBuffer(env, j_media_codec_.obj(), index));
  void* src_data =
      reinterpret_cast<uint8*>(env->GetDirectBufferAddress(j_buffer.obj())) +
      offset;
  int src_capacity = env->GetDirectBufferCapacity(j_buffer.obj()) - offset;
  if (src_capacity < dst_size)
    return false;
  memcpy(dst, src_data, dst_size);
  return true;
}

bool MediaCodecBridge::FillInputBuffer(int index,
                                       const uint8* data,
                                       size_t size) {
  uint8* dst = NULL;
  size_t capacity = 0;
  GetInputBuffer(index, &dst, &capacity);
  CHECK(dst);

  if (size > capacity) {
    LOG(ERROR) << "Input buffer size " << size
               << " exceeds MediaCodec input buffer capacity: " << capacity;
    return false;
  }

  memcpy(dst, data, size);
  return true;
}

AudioCodecBridge::AudioCodecBridge(const std::string& mime)
    // Audio codec doesn't care about security level and there is no need for
    // audio encoding yet.
    : MediaCodecBridge(mime, false, MEDIA_CODEC_DECODER) {}

bool AudioCodecBridge::Start(const AudioCodec& codec,
                             int sample_rate,
                             int channel_count,
                             const uint8* extra_data,
                             size_t extra_data_size,
                             bool play_audio,
                             jobject media_crypto) {
  JNIEnv* env = AttachCurrentThread();

  if (!media_codec())
    return false;

  std::string codec_string = AudioCodecToAndroidMimeType(codec);
  if (codec_string.empty())
    return false;

  ScopedJavaLocalRef<jstring> j_mime =
      ConvertUTF8ToJavaString(env, codec_string);
  ScopedJavaLocalRef<jobject> j_format(Java_MediaCodecBridge_createAudioFormat(
      env, j_mime.obj(), sample_rate, channel_count));
  DCHECK(!j_format.is_null());

  if (!ConfigureMediaFormat(j_format.obj(), codec, extra_data, extra_data_size))
    return false;

  if (!Java_MediaCodecBridge_configureAudio(
           env, media_codec(), j_format.obj(), media_crypto, 0, play_audio)) {
    return false;
  }

  return StartInternal();
}

bool AudioCodecBridge::ConfigureMediaFormat(jobject j_format,
                                            const AudioCodec& codec,
                                            const uint8* extra_data,
                                            size_t extra_data_size) {
  if (extra_data_size == 0)
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
      const uint8* current_pos = extra_data;
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
      Java_MediaCodecBridge_setCodecSpecificData(
          env, j_format, 0, first_header.obj());
      // The last header is codec header.
      ScopedJavaLocalRef<jbyteArray> last_header =
          base::android::ToJavaByteArray(
              env, extra_data + total_length, extra_data_size - total_length);
      Java_MediaCodecBridge_setCodecSpecificData(
          env, j_format, 1, last_header.obj());
      break;
    }
    case kCodecAAC: {
      media::BitReader reader(extra_data, extra_data_size);

      // The following code is copied from aac.cc
      // TODO(qinmin): refactor the code in aac.cc to make it more reusable.
      uint8 profile = 0;
      uint8 frequency_index = 0;
      uint8 channel_config = 0;
      if (!reader.ReadBits(5, &profile) ||
          !reader.ReadBits(4, &frequency_index)) {
        LOG(ERROR) << "Unable to parse AAC header";
        return false;
      }
      if (0xf == frequency_index && !reader.SkipBits(24)) {
        LOG(ERROR) << "Unable to parse AAC header";
        return false;
      }
      if (!reader.ReadBits(4, &channel_config)) {
        LOG(ERROR) << "Unable to parse AAC header";
        return false;
      }

      if (profile < 1 || profile > 4 || frequency_index == 0xf ||
          channel_config > 7) {
        LOG(ERROR) << "Invalid AAC header";
        return false;
      }
      const size_t kCsdLength = 2;
      uint8 csd[kCsdLength];
      csd[0] = profile << 3 | frequency_index >> 1;
      csd[1] = (frequency_index & 0x01) << 7 | channel_config << 3;
      ScopedJavaLocalRef<jbyteArray> byte_array =
          base::android::ToJavaByteArray(env, csd, kCsdLength);
      Java_MediaCodecBridge_setCodecSpecificData(
          env, j_format, 0, byte_array.obj());

      // TODO(qinmin): pass an extra variable to this function to determine
      // whether we need to call this.
      Java_MediaCodecBridge_setFrameHasADTSHeader(env, j_format);
      break;
    }
    default:
      LOG(ERROR) << "Invalid header encountered for codec: "
                 << AudioCodecToAndroidMimeType(codec);
      return false;
  }
  return true;
}

int64 AudioCodecBridge::PlayOutputBuffer(int index, size_t size) {
  DCHECK_LE(0, index);
  int numBytes = base::checked_cast<int>(size);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> buf =
      Java_MediaCodecBridge_getOutputBuffer(env, media_codec(), index);
  uint8* buffer = static_cast<uint8*>(env->GetDirectBufferAddress(buf.obj()));

  ScopedJavaLocalRef<jbyteArray> byte_array =
      base::android::ToJavaByteArray(env, buffer, numBytes);
  return Java_MediaCodecBridge_playOutputBuffer(
      env, media_codec(), byte_array.obj());
}

void AudioCodecBridge::SetVolume(double volume) {
  JNIEnv* env = AttachCurrentThread();
  Java_MediaCodecBridge_setVolume(env, media_codec(), volume);
}

// static
AudioCodecBridge* AudioCodecBridge::Create(const AudioCodec& codec) {
  if (!MediaCodecBridge::IsAvailable())
    return NULL;

  const std::string mime = AudioCodecToAndroidMimeType(codec);
  return mime.empty() ? NULL : new AudioCodecBridge(mime);
}

// static
bool AudioCodecBridge::IsKnownUnaccelerated(const AudioCodec& codec) {
  return MediaCodecBridge::IsKnownUnaccelerated(
      AudioCodecToAndroidMimeType(codec), MEDIA_CODEC_DECODER);
}

// static
bool VideoCodecBridge::IsKnownUnaccelerated(const VideoCodec& codec,
                                            MediaCodecDirection direction) {
  return MediaCodecBridge::IsKnownUnaccelerated(
      VideoCodecToAndroidMimeType(codec), direction);
}

// static
VideoCodecBridge* VideoCodecBridge::CreateDecoder(const VideoCodec& codec,
                                                  bool is_secure,
                                                  const gfx::Size& size,
                                                  jobject surface,
                                                  jobject media_crypto) {
  if (!MediaCodecBridge::IsAvailable())
    return NULL;

  const std::string mime = VideoCodecToAndroidMimeType(codec);
  if (mime.empty())
    return NULL;

  scoped_ptr<VideoCodecBridge> bridge(
      new VideoCodecBridge(mime, is_secure, MEDIA_CODEC_DECODER));
  if (!bridge->media_codec())
    return NULL;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_mime = ConvertUTF8ToJavaString(env, mime);
  ScopedJavaLocalRef<jobject> j_format(
      Java_MediaCodecBridge_createVideoDecoderFormat(
          env, j_mime.obj(), size.width(), size.height()));
  DCHECK(!j_format.is_null());
  if (!Java_MediaCodecBridge_configureVideo(env,
                                            bridge->media_codec(),
                                            j_format.obj(),
                                            surface,
                                            media_crypto,
                                            0)) {
    return NULL;
  }

  return bridge->StartInternal() ? bridge.release() : NULL;
}

// static
VideoCodecBridge* VideoCodecBridge::CreateEncoder(const VideoCodec& codec,
                                                  const gfx::Size& size,
                                                  int bit_rate,
                                                  int frame_rate,
                                                  int i_frame_interval,
                                                  int color_format) {
  if (!MediaCodecBridge::IsAvailable())
    return NULL;

  const std::string mime = VideoCodecToAndroidMimeType(codec);
  if (mime.empty())
    return NULL;

  scoped_ptr<VideoCodecBridge> bridge(
      new VideoCodecBridge(mime, false, MEDIA_CODEC_ENCODER));
  if (!bridge->media_codec())
    return NULL;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_mime = ConvertUTF8ToJavaString(env, mime);
  ScopedJavaLocalRef<jobject> j_format(
      Java_MediaCodecBridge_createVideoEncoderFormat(env,
                                                     j_mime.obj(),
                                                     size.width(),
                                                     size.height(),
                                                     bit_rate,
                                                     frame_rate,
                                                     i_frame_interval,
                                                     color_format));
  DCHECK(!j_format.is_null());
  if (!Java_MediaCodecBridge_configureVideo(env,
                                            bridge->media_codec(),
                                            j_format.obj(),
                                            NULL,
                                            NULL,
                                            kConfigureFlagEncode)) {
    return NULL;
  }

  return bridge->StartInternal() ? bridge.release() : NULL;
}

VideoCodecBridge::VideoCodecBridge(const std::string& mime,
                                   bool is_secure,
                                   MediaCodecDirection direction)
    : MediaCodecBridge(mime, is_secure, direction),
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
  return Java_MediaCodecBridge_isAdaptivePlaybackSupported(
      env, media_codec(), width, height);
}

bool MediaCodecBridge::RegisterMediaCodecBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace media
