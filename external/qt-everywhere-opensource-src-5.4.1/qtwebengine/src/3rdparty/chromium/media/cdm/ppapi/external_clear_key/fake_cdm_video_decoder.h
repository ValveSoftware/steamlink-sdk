// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_PPAPI_EXTERNAL_CLEAR_KEY_FAKE_CDM_VIDEO_DECODER_H_
#define MEDIA_CDM_PPAPI_EXTERNAL_CLEAR_KEY_FAKE_CDM_VIDEO_DECODER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "media/cdm/ppapi/api/content_decryption_module.h"
#include "media/cdm/ppapi/external_clear_key/cdm_video_decoder.h"

namespace media {

class FakeCdmVideoDecoder : public CdmVideoDecoder {
 public:
  explicit FakeCdmVideoDecoder(cdm::Host* host);
  virtual ~FakeCdmVideoDecoder();

  // CdmVideoDecoder implementation.
  virtual bool Initialize(const cdm::VideoDecoderConfig& config) OVERRIDE;
  virtual void Deinitialize() OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual cdm::Status DecodeFrame(const uint8_t* compressed_frame,
                                  int32_t compressed_frame_size,
                                  int64_t timestamp,
                                  cdm::VideoFrame* decoded_frame) OVERRIDE;
  virtual bool is_initialized() const OVERRIDE { return is_initialized_; }

 private:
  bool is_initialized_;
  cdm::Size video_size_;

  cdm::Host* const host_;

  DISALLOW_COPY_AND_ASSIGN(FakeCdmVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_CDM_PPAPI_EXTERNAL_CLEAR_KEY_FAKE_CDM_VIDEO_DECODER_H_
