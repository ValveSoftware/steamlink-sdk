// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_codec_util.h"

#include <stddef.h>

#include <algorithm>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "jni/MediaCodecUtil_jni.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaIntArrayToIntVector;
using base::android::ScopedJavaLocalRef;

namespace media {

namespace {

const char kMp4aMimeType[] = "audio/mp4a-latm";
const char kOpusMimeType[] = "audio/opus";
const char kVorbisMimeType[] = "audio/vorbis";
const char kAvcMimeType[] = "video/avc";
const char kHevcMimeType[] = "video/hevc";
const char kVp8MimeType[] = "video/x-vnd.on2.vp8";
const char kVp9MimeType[] = "video/x-vnd.on2.vp9";

}  // namespace

static std::string CodecTypeToAndroidMimeType(const std::string& codec) {
  // TODO(xhwang): Shall we handle more detailed strings like "mp4a.40.2"?
  if (codec == "avc1")
    return kAvcMimeType;
  if (codec == "hvc1")
    return kHevcMimeType;
  if (codec == "mp4a")
    return kMp4aMimeType;
  if (codec == "vp8" || codec == "vp8.0")
    return kVp8MimeType;
  if (codec == "vp9" || codec == "vp9.0")
    return kVp9MimeType;
  if (codec == "vorbis")
    return kVorbisMimeType;
  if (codec == "opus")
    return kOpusMimeType;
  DLOG(WARNING) << "Cannot convert codec to Android MIME type: " << codec;
  return std::string();
}

static bool IsSupportedAndroidMimeType(const std::string& mime_type) {
  std::vector<std::string> supported{
      kMp4aMimeType, kOpusMimeType, kVorbisMimeType, kAvcMimeType,
      kHevcMimeType, kVp8MimeType,  kVp9MimeType};
  return std::find(supported.begin(), supported.end(), mime_type) !=
         supported.end();
}

static std::string GetDefaultCodecName(const std::string& mime_type,
                                       MediaCodecDirection direction,
                                       bool require_software_codec) {
  DCHECK(MediaCodecUtil::IsMediaCodecAvailable());
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_mime = ConvertUTF8ToJavaString(env, mime_type);
  ScopedJavaLocalRef<jstring> j_codec_name =
      Java_MediaCodecUtil_getDefaultCodecName(env, j_mime.obj(), direction,
                                              require_software_codec);
  return ConvertJavaStringToUTF8(env, j_codec_name.obj());
}

static bool IsDecoderSupportedByDevice(const std::string& android_mime_type) {
  DCHECK(MediaCodecUtil::IsMediaCodecAvailable());
  DCHECK(IsSupportedAndroidMimeType(android_mime_type));
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_mime =
      ConvertUTF8ToJavaString(env, android_mime_type);
  return Java_MediaCodecUtil_isDecoderSupportedForDevice(env, j_mime.obj());
}

// static
bool MediaCodecUtil::IsMediaCodecAvailable() {
  // MediaCodec is only available on JB and greater.
  if (base::android::BuildInfo::GetInstance()->sdk_int() < 16)
    return false;

  // Blacklist some devices on Jellybean as for MediaCodec support is buggy.
  // http://crbug.com/365494, http://crbug.com/615872
  if (base::android::BuildInfo::GetInstance()->sdk_int() <= 19) {
    std::string model(base::android::BuildInfo::GetInstance()->model());
    return model != "GT-I9100" && model != "GT-I9300" && model != "GT-N7000" &&
           model != "GT-N7100";
  }
  return true;
}

// static
bool MediaCodecUtil::SupportsSetParameters() {
  // MediaCodec.setParameters() is only available starting with K.
  return base::android::BuildInfo::GetInstance()->sdk_int() >= 19;
}

// static
std::set<int> MediaCodecUtil::GetEncoderColorFormats(
    const std::string& mime_type) {
  std::set<int> color_formats;
  if (!IsMediaCodecAvailable())
    return color_formats;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_mime = ConvertUTF8ToJavaString(env, mime_type);
  ScopedJavaLocalRef<jintArray> j_color_format_array =
      Java_MediaCodecUtil_getEncoderColorFormatsForMime(env, j_mime.obj());

  if (j_color_format_array.obj()) {
    std::vector<int> formats;
    JavaIntArrayToIntVector(env, j_color_format_array.obj(), &formats);
    color_formats = std::set<int>(formats.begin(), formats.end());
  }

  return color_formats;
}

// static
bool MediaCodecUtil::CanDecode(const std::string& codec, bool is_secure) {
  if (!IsMediaCodecAvailable())
    return false;

  JNIEnv* env = AttachCurrentThread();
  std::string mime = CodecTypeToAndroidMimeType(codec);
  if (mime.empty())
    return false;
  ScopedJavaLocalRef<jstring> j_mime = ConvertUTF8ToJavaString(env, mime);
  return Java_MediaCodecUtil_canDecode(env, j_mime.obj(), is_secure);
}

// static
bool MediaCodecUtil::IsKnownUnaccelerated(const std::string& android_mime_type,
                                          MediaCodecDirection direction) {
  DCHECK(IsSupportedAndroidMimeType(android_mime_type));
  if (!IsMediaCodecAvailable())
    return true;

  std::string codec_name =
      GetDefaultCodecName(android_mime_type, direction, false);
  DVLOG(1) << __FUNCTION__ << "Default codec for " << android_mime_type << " : "
           << codec_name << ", direction: " << direction;
  if (!codec_name.size())
    return true;

  // MediaTek hardware vp8 is known slower than the software implementation.
  // MediaTek hardware vp9 is known crashy, see http://crbug.com/446974 and
  // http://crbug.com/597836.
  if (base::StartsWith(codec_name, "OMX.MTK.", base::CompareCase::SENSITIVE)) {
    if (android_mime_type == kVp8MimeType)
      return true;

    if (android_mime_type == kVp9MimeType)
      return base::android::BuildInfo::GetInstance()->sdk_int() < 21;

    return false;
  }

  // It would be nice if MediaCodecInfo externalized some notion of
  // HW-acceleration but it doesn't. Android Media guidance is that the
  // "OMX.google" prefix is always used for SW decoders, so that's what we
  // use. "OMX.SEC.*" codec is Samsung software implementation - report it
  // as unaccelerated as well.
  return base::StartsWith(codec_name, "OMX.google.",
                          base::CompareCase::SENSITIVE) ||
         base::StartsWith(codec_name, "OMX.SEC.", base::CompareCase::SENSITIVE);
}

// static
bool MediaCodecUtil::IsHLSPath(const GURL& url) {
  return (url.SchemeIsHTTPOrHTTPS() || url.SchemeIsFile()) &&
         base::EndsWith(url.path(), ".m3u8",
                        base::CompareCase::INSENSITIVE_ASCII);
}

// static
bool MediaCodecUtil::IsHLSURL(const GURL& url) {
  return (url.SchemeIsHTTPOrHTTPS() || url.SchemeIsFile()) &&
         url.spec().find("m3u8") != std::string::npos;
}

// static
bool MediaCodecUtil::RegisterMediaCodecUtil(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
bool MediaCodecUtil::IsVp8DecoderAvailable() {
  return IsMediaCodecAvailable() && IsDecoderSupportedByDevice(kVp8MimeType);
}

// static
bool MediaCodecUtil::IsVp8EncoderAvailable() {
  // Currently the vp8 encoder and decoder blacklists cover the same devices,
  // but we have a second method for clarity in future issues.
  return IsVp8DecoderAvailable();
}

// static
bool MediaCodecUtil::IsVp9DecoderAvailable() {
  return IsMediaCodecAvailable() && IsDecoderSupportedByDevice(kVp9MimeType);
}

// static
bool MediaCodecUtil::IsSurfaceViewOutputSupported() {
  // Disable SurfaceView output for the Samsung Galaxy S3; it does not work
  // well enough for even 360p24 H264 playback.  http://crbug.com/602870.
  //
  // Notably this is codec agnostic at present, so any devices added to
  // the blacklist will avoid trying to play any codecs on SurfaceView.  If
  // needed in the future this can be expanded to be codec specific.
  const char* model_prefixes[] = {// Exynos 4 (Mali-400)
                                  "GT-I9300", "GT-I9305", "SHV-E210",
                                  // Snapdragon S4 (Adreno-225)
                                  "SCH-I535", "SCH-J201", "SCH-R530",
                                  "SCH-I960", "SCH-S968", "SGH-T999",
                                  "SGH-I747", "SGH-N064", 0};

  std::string model(base::android::BuildInfo::GetInstance()->model());
  for (int i = 0; model_prefixes[i]; ++i) {
    if (base::StartsWith(model, model_prefixes[i],
                         base::CompareCase::INSENSITIVE_ASCII)) {
      return false;
    }
  }

  return true;
}

}  // namespace media
