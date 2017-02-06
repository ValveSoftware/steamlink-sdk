// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_NDK_MEDIA_CODEC_BRIDGE_H_
#define MEDIA_BASE_ANDROID_NDK_MEDIA_CODEC_BRIDGE_H_

#include <media/NdkMediaCodec.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/media_export.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class MEDIA_EXPORT NdkMediaCodecBridge : public MediaCodecBridge {
 public:
  ~NdkMediaCodecBridge() override;

  // MediaCodecBridge implementation.
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

 protected:
  NdkMediaCodecBridge(const std::string& mime,
                      bool is_secure,
                      MediaCodecDirection direction);

 private:
  struct AMediaCodecDeleter {
    inline void operator()(AMediaCodec* ptr) const { AMediaCodec_delete(ptr); }
  };

  std::unique_ptr<AMediaCodec, AMediaCodecDeleter> media_codec_;

  DISALLOW_COPY_AND_ASSIGN(NdkMediaCodecBridge);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_NDK_MEDIA_CODEC_BRIDGE_H_
