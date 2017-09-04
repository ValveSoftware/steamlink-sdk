// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_PPAPI_EXTERNAL_CLEAR_KEY_FAKE_CDM_VIDEO_DECODER_H_
#define MEDIA_CDM_PPAPI_EXTERNAL_CLEAR_KEY_FAKE_CDM_VIDEO_DECODER_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "media/cdm/api/content_decryption_module.h"
#include "media/cdm/ppapi/external_clear_key/cdm_video_decoder.h"

namespace media {

class FakeCdmVideoDecoder : public CdmVideoDecoder {
 public:
  explicit FakeCdmVideoDecoder(cdm::Host* host);
  ~FakeCdmVideoDecoder() override;

  // CdmVideoDecoder implementation.
  bool Initialize(const cdm::VideoDecoderConfig& config) override;
  void Deinitialize() override;
  void Reset() override;
  cdm::Status DecodeFrame(const uint8_t* compressed_frame,
                          int32_t compressed_frame_size,
                          int64_t timestamp,
                          cdm::VideoFrame* decoded_frame) override;
  bool is_initialized() const override { return is_initialized_; }

 private:
  bool is_initialized_;
  cdm::Size video_size_;

  cdm::Host* const host_;

  DISALLOW_COPY_AND_ASSIGN(FakeCdmVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_CDM_PPAPI_EXTERNAL_CLEAR_KEY_FAKE_CDM_VIDEO_DECODER_H_
