// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_PPAPI_EXTERNAL_CLEAR_KEY_LIBVPX_CDM_VIDEO_DECODER_H_
#define MEDIA_CDM_PPAPI_EXTERNAL_CLEAR_KEY_LIBVPX_CDM_VIDEO_DECODER_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "media/cdm/api/content_decryption_module.h"
#include "media/cdm/ppapi/external_clear_key/cdm_video_decoder.h"

struct vpx_codec_ctx;
struct vpx_image;

namespace media {

class LibvpxCdmVideoDecoder : public CdmVideoDecoder {
 public:
  explicit LibvpxCdmVideoDecoder(CdmHost* host);
  ~LibvpxCdmVideoDecoder() override;

  // CdmVideoDecoder implementation.
  bool Initialize(const cdm::VideoDecoderConfig& config) override;
  void Deinitialize() override;
  void Reset() override;
  cdm::Status DecodeFrame(const uint8_t* compressed_frame,
                          int32_t compressed_frame_size,
                          int64_t timestamp,
                          cdm::VideoFrame* decoded_frame) override;
  bool is_initialized() const override { return is_initialized_; }

  // Returns true when |format| and |data_size| specify a supported video
  // output configuration.
  static bool IsValidOutputConfig(cdm::VideoFormat format,
                                  const cdm::Size& data_size);

 private:
  // Allocates storage, then copies video frame stored in |vpx_image_| to
  // |cdm_video_frame|. Returns true when allocation and copy succeed.
  bool CopyVpxImageTo(cdm::VideoFrame* cdm_video_frame);

  bool is_initialized_;

  CdmHost* const host_;

  vpx_codec_ctx* vpx_codec_;
  vpx_image* vpx_image_;

  DISALLOW_COPY_AND_ASSIGN(LibvpxCdmVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_CDM_PPAPI_EXTERNAL_CLEAR_KEY_LIBVPX_CDM_VIDEO_DECODER_H_
