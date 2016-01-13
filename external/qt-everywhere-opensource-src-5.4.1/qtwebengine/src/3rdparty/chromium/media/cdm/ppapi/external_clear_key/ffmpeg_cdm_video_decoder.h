// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_PPAPI_EXTERNAL_CLEAR_KEY_FFMPEG_CDM_VIDEO_DECODER_H_
#define MEDIA_CDM_PPAPI_EXTERNAL_CLEAR_KEY_FFMPEG_CDM_VIDEO_DECODER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "media/cdm/ppapi/external_clear_key/cdm_video_decoder.h"
#include "media/cdm/ppapi/external_clear_key/clear_key_cdm_common.h"
#include "media/ffmpeg/ffmpeg_deleters.h"

struct AVCodecContext;
struct AVFrame;

namespace media {

class FFmpegCdmVideoDecoder : public CdmVideoDecoder {
 public:
  explicit FFmpegCdmVideoDecoder(ClearKeyCdmHost* host);
  virtual ~FFmpegCdmVideoDecoder();

  // CdmVideoDecoder implementation.
  virtual bool Initialize(const cdm::VideoDecoderConfig& config) OVERRIDE;
  virtual void Deinitialize() OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual cdm::Status DecodeFrame(const uint8_t* compressed_frame,
                                  int32_t compressed_frame_size,
                                  int64_t timestamp,
                                  cdm::VideoFrame* decoded_frame) OVERRIDE;
  virtual bool is_initialized() const OVERRIDE { return is_initialized_; }

  // Returns true when |format| and |data_size| specify a supported video
  // output configuration.
  static bool IsValidOutputConfig(cdm::VideoFormat format,
                                  const cdm::Size& data_size);

 private:
  // Allocates storage, then copies video frame stored in |av_frame_| to
  // |cdm_video_frame|. Returns true when allocation and copy succeed.
  bool CopyAvFrameTo(cdm::VideoFrame* cdm_video_frame);

  void ReleaseFFmpegResources();

  // FFmpeg structures owned by this object.
  scoped_ptr<AVCodecContext, ScopedPtrAVFreeContext> codec_context_;
  scoped_ptr<AVFrame, ScopedPtrAVFreeFrame> av_frame_;

  bool is_initialized_;

  ClearKeyCdmHost* const host_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegCdmVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_CDM_PPAPI_EXTERNAL_CLEAR_KEY_FFMPEG_CDM_VIDEO_DECODER_H_
