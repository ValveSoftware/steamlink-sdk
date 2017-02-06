// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_codec_bridge.h"

#include <algorithm>
#include <limits>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "media/base/decrypt_config.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaIntArrayToIntVector;
using base::android::ScopedJavaLocalRef;

namespace media {

MediaCodecBridge::MediaCodecBridge() {}

MediaCodecBridge::~MediaCodecBridge() {}

MediaCodecStatus MediaCodecBridge::QueueSecureInputBuffer(
    int index,
    const uint8_t* data,
    size_t data_size,
    const std::string& key_id,
    const std::string& iv,
    const std::vector<SubsampleEntry>& subsamples,
    const base::TimeDelta& presentation_time) {
  const std::vector<char> key_vec(key_id.begin(), key_id.end());
  const std::vector<char> iv_vec(iv.begin(), iv.end());
  return QueueSecureInputBuffer(index, data, data_size, key_vec, iv_vec,
                                subsamples.empty() ? nullptr : &subsamples[0],
                                subsamples.size(), presentation_time);
}

MediaCodecStatus MediaCodecBridge::CopyFromOutputBuffer(int index,
                                                        size_t offset,
                                                        void* dst,
                                                        size_t num) {
  const uint8_t* src_data = nullptr;
  size_t src_capacity = 0;
  MediaCodecStatus status =
      GetOutputBufferAddress(index, offset, &src_data, &src_capacity);
  if (status == MEDIA_CODEC_OK) {
    CHECK_GE(src_capacity, num);
    memcpy(dst, src_data, num);
  }
  return status;
}

bool MediaCodecBridge::FillInputBuffer(int index,
                                       const uint8_t* data,
                                       size_t size) {
  uint8_t* dst = nullptr;
  size_t capacity = 0;
  if (GetInputBuffer(index, &dst, &capacity) != MEDIA_CODEC_OK) {
    LOG(ERROR) << "GetInputBuffer failed";
    return false;
  }
  CHECK(dst);

  if (size > capacity) {
    LOG(ERROR) << "Input buffer size " << size
               << " exceeds MediaCodec input buffer capacity: " << capacity;
    return false;
  }

  memcpy(dst, data, size);
  return true;
}

}  // namespace media
