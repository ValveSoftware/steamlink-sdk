// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/ndk_media_codec_bridge.h"

#include <media/NdkMediaError.h>
#include <media/NdkMediaFormat.h>

#include <limits>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/strings/string_util.h"
#include "media/base/decrypt_config.h"

namespace {
const char kMediaFormatKeyCropLeft[] = "crop-left";
const char kMediaFormatKeyCropRight[] = "crop-right";
const char kMediaFormatKeyCropBottom[] = "crop-bottom";
const char kMediaFormatKeyCropTop[] = "crop-top";
}

namespace media {

// Translate media_status_t to MediaCodecStatus.
static MediaCodecStatus TranslateMediaCodecStatus(media_status_t status) {
  switch (status) {
    case AMEDIA_OK:
      return MEDIA_CODEC_OK;
    case AMEDIA_DRM_NEED_KEY:
      return MEDIA_CODEC_NO_KEY;
    default:
      return MEDIA_CODEC_ERROR;
  }
}

NdkMediaCodecBridge::~NdkMediaCodecBridge() {}

NdkMediaCodecBridge::NdkMediaCodecBridge(const std::string& mime,
                                         bool is_secure,
                                         MediaCodecDirection direction) {
  if (base::StartsWith(mime, "video", base::CompareCase::SENSITIVE) &&
      is_secure && direction == MEDIA_CODEC_DECODER) {
    // TODO(qinmin): get the secure decoder name from java.
    NOTIMPLEMENTED();
    return;
  }

  if (direction == MEDIA_CODEC_DECODER)
    media_codec_.reset(AMediaCodec_createDecoderByType(mime.c_str()));
  else
    media_codec_.reset(AMediaCodec_createEncoderByType(mime.c_str()));
}

bool NdkMediaCodecBridge::Start() {
  return AMEDIA_OK == AMediaCodec_start(media_codec_.get());
}

void NdkMediaCodecBridge::Stop() {
  AMediaCodec_stop(media_codec_.get());
}

MediaCodecStatus NdkMediaCodecBridge::Flush() {
  media_status_t status = AMediaCodec_flush(media_codec_.get());
  return TranslateMediaCodecStatus(status);
}

MediaCodecStatus NdkMediaCodecBridge::GetOutputSize(gfx::Size* size) {
  AMediaFormat* format = AMediaCodec_getOutputFormat(media_codec_.get());
  int left, right, bottom, top;
  bool has_left = AMediaFormat_getInt32(format, kMediaFormatKeyCropLeft, &left);
  bool has_right =
      AMediaFormat_getInt32(format, kMediaFormatKeyCropRight, &right);
  bool has_bottom =
      AMediaFormat_getInt32(format, kMediaFormatKeyCropBottom, &bottom);
  bool has_top = AMediaFormat_getInt32(format, kMediaFormatKeyCropTop, &top);
  int width, height;
  if (has_left && has_right && has_bottom && has_top) {
    // Use crop size as it is more accurate. right and bottom are inclusive.
    width = right - left + 1;
    height = top - bottom + 1;
  } else {
    AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &width);
    AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &height);
  }
  AMediaFormat_delete(format);
  size->SetSize(width, height);
  return MEDIA_CODEC_OK;
}

MediaCodecStatus NdkMediaCodecBridge::GetOutputSamplingRate(
    int* sampling_rate) {
  AMediaFormat* format = AMediaCodec_getOutputFormat(media_codec_.get());
  *sampling_rate = 0;
  AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_SAMPLE_RATE, sampling_rate);
  AMediaFormat_delete(format);
  DCHECK_NE(*sampling_rate, 0);
  return MEDIA_CODEC_OK;
}

MediaCodecStatus NdkMediaCodecBridge::GetOutputChannelCount(
    int* channel_count) {
  AMediaFormat* format = AMediaCodec_getOutputFormat(media_codec_.get());
  *channel_count = 0;
  AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_CHANNEL_COUNT, channel_count);
  AMediaFormat_delete(format);
  DCHECK_NE(*channel_count, 0);
  return MEDIA_CODEC_OK;
}

MediaCodecStatus NdkMediaCodecBridge::QueueInputBuffer(
    int index,
    const uint8_t* data,
    size_t data_size,
    const base::TimeDelta& presentation_time) {
  if (data_size >
      base::checked_cast<size_t>(std::numeric_limits<int32_t>::max())) {
    return MEDIA_CODEC_ERROR;
  }
  if (data && !FillInputBuffer(index, data, data_size))
    return MEDIA_CODEC_ERROR;

  media_status_t status =
      AMediaCodec_queueInputBuffer(media_codec_.get(), index, 0, data_size,
                                   presentation_time.InMicroseconds(), 0);
  return TranslateMediaCodecStatus(status);
}

MediaCodecStatus NdkMediaCodecBridge::QueueSecureInputBuffer(
    int index,
    const uint8_t* data,
    size_t data_size,
    const std::vector<char>& key_id,
    const std::vector<char>& iv,
    const SubsampleEntry* subsamples,
    int subsamples_size,
    const base::TimeDelta& presentation_time) {
  if (data_size >
      base::checked_cast<size_t>(std::numeric_limits<int32_t>::max())) {
    return MEDIA_CODEC_ERROR;
  }
  if (key_id.size() > 16 || iv.size())
    return MEDIA_CODEC_ERROR;
  if (data && !FillInputBuffer(index, data, data_size))
    return MEDIA_CODEC_ERROR;

  int new_subsamples_size = subsamples_size == 0 ? 1 : subsamples_size;
  std::vector<size_t> clear_data, encrypted_data;
  if (subsamples_size == 0) {
    DCHECK(!subsamples);
    clear_data.push_back(0);
    encrypted_data.push_back(data_size);
  } else {
    DCHECK_GT(subsamples_size, 0);
    DCHECK(subsamples);
    for (int i = 0; i < subsamples_size; ++i) {
      DCHECK(subsamples[i].clear_bytes <= std::numeric_limits<uint16_t>::max());
      if (subsamples[i].cypher_bytes >
          static_cast<int32_t>(std::numeric_limits<int32_t>::max())) {
        return MEDIA_CODEC_ERROR;
      }
      clear_data.push_back(subsamples[i].clear_bytes);
      encrypted_data.push_back(subsamples[i].cypher_bytes);
    }
  }

  AMediaCodecCryptoInfo* crypto_info = AMediaCodecCryptoInfo_new(
      new_subsamples_size,
      reinterpret_cast<uint8_t*>(const_cast<char*>(key_id.data())),
      reinterpret_cast<uint8_t*>(const_cast<char*>(iv.data())),
      AMEDIACODECRYPTOINFO_MODE_AES_CTR, clear_data.data(),
      encrypted_data.data());

  media_status_t status = AMediaCodec_queueSecureInputBuffer(
      media_codec_.get(), index, 0, crypto_info,
      presentation_time.InMicroseconds(), 0);
  AMediaCodecCryptoInfo_delete(crypto_info);
  return TranslateMediaCodecStatus(status);
}

void NdkMediaCodecBridge::QueueEOS(int input_buffer_index) {
  AMediaCodec_queueInputBuffer(media_codec_.get(), input_buffer_index, 0, 0, 0,
                               AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
}

MediaCodecStatus NdkMediaCodecBridge::DequeueInputBuffer(
    const base::TimeDelta& timeout,
    int* index) {
  *index = AMediaCodec_dequeueInputBuffer(media_codec_.get(),
                                          timeout.InMicroseconds());
  if (*index >= 0)
    return MEDIA_CODEC_OK;
  else if (*index == AMEDIACODEC_INFO_TRY_AGAIN_LATER)
    return MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER;
  else
    return MEDIA_CODEC_ERROR;
}

MediaCodecStatus NdkMediaCodecBridge::DequeueOutputBuffer(
    const base::TimeDelta& timeout,
    int* index,
    size_t* offset,
    size_t* size,
    base::TimeDelta* presentation_time,
    bool* end_of_stream,
    bool* key_frame) {
  AMediaCodecBufferInfo buffer_info;
  *index = AMediaCodec_dequeueOutputBuffer(media_codec_.get(), &buffer_info,
                                           timeout.InMicroseconds());
  *offset = buffer_info.offset;
  *size = buffer_info.size;
  *presentation_time =
      base::TimeDelta::FromMicroseconds(buffer_info.presentationTimeUs);
  if (end_of_stream)
    *end_of_stream = buffer_info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM;
  if (key_frame)
    *key_frame = false;  // This is deprecated.
  if (*index >= 0)
    return MEDIA_CODEC_OK;
  else if (*index == AMEDIACODEC_INFO_TRY_AGAIN_LATER)
    return MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER;
  else
    return MEDIA_CODEC_ERROR;
}

void NdkMediaCodecBridge::ReleaseOutputBuffer(int index, bool render) {
  AMediaCodec_releaseOutputBuffer(media_codec_.get(), index, render);
}

MediaCodecStatus NdkMediaCodecBridge::GetInputBuffer(int input_buffer_index,
                                                     uint8_t** data,
                                                     size_t* capacity) {
  *data = AMediaCodec_getInputBuffer(media_codec_.get(), input_buffer_index,
                                     capacity);
  return MEDIA_CODEC_OK;
}

MediaCodecStatus NdkMediaCodecBridge::GetOutputBufferAddress(
    int index,
    size_t offset,
    const uint8_t** addr,
    size_t* capacity) {
  const uint8_t* src_data =
      AMediaCodec_getOutputBuffer(media_codec_.get(), index, capacity);
  *addr = src_data + offset;
  *capacity -= offset;
  return MEDIA_CODEC_OK;
}

}  // namespace media
