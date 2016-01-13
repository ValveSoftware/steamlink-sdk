// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "media/cdm/ppapi/external_clear_key/cdm_video_decoder.h"

#if defined(CLEAR_KEY_CDM_USE_FAKE_VIDEO_DECODER)
#include "media/cdm/ppapi/external_clear_key/fake_cdm_video_decoder.h"
#endif

#if defined(CLEAR_KEY_CDM_USE_FFMPEG_DECODER)
#include "media/cdm/ppapi/external_clear_key/ffmpeg_cdm_video_decoder.h"
#endif

#if defined(CLEAR_KEY_CDM_USE_LIBVPX_DECODER)
#include "media/cdm/ppapi/external_clear_key/libvpx_cdm_video_decoder.h"
#endif

namespace media {

scoped_ptr<CdmVideoDecoder> CreateVideoDecoder(
    ClearKeyCdmHost* host, const cdm::VideoDecoderConfig& config) {
  scoped_ptr<CdmVideoDecoder> video_decoder;
#if defined(CLEAR_KEY_CDM_USE_FAKE_VIDEO_DECODER)
  video_decoder.reset(new FakeCdmVideoDecoder(host));

  if (!video_decoder->Initialize(config))
    video_decoder.reset();
#else

#if defined(CLEAR_KEY_CDM_USE_LIBVPX_DECODER)
  if (config.codec == cdm::VideoDecoderConfig::kCodecVp8 ||
      config.codec == cdm::VideoDecoderConfig::kCodecVp9) {
    video_decoder.reset(new LibvpxCdmVideoDecoder(host));

    if (!video_decoder->Initialize(config))
      video_decoder.reset();

    return video_decoder.Pass();
  }
#endif

#if defined(CLEAR_KEY_CDM_USE_FFMPEG_DECODER)
  video_decoder.reset(new FFmpegCdmVideoDecoder(host));

  if (!video_decoder->Initialize(config))
    video_decoder.reset();
#endif

#endif  // CLEAR_KEY_CDM_USE_FAKE_VIDEO_DECODER

  return video_decoder.Pass();
}

}  // namespace media
