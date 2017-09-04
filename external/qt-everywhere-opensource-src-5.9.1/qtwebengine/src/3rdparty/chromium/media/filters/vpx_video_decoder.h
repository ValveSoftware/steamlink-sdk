// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_VPX_VIDEO_DECODER_H_
#define MEDIA_FILTERS_VPX_VIDEO_DECODER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "media/base/demuxer_stream.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_pool.h"

struct vpx_codec_ctx;
struct vpx_image;

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

// Libvpx video decoder wrapper.
// Note: VpxVideoDecoder accepts only YV12A VP8 content or VP9 content. This is
// done to avoid usurping FFmpeg for all VP8 decoding, because the FFmpeg VP8
// decoder is faster than the libvpx VP8 decoder.
// Alpha channel, if any, is sent in the DecoderBuffer's side_data() as a frame
// on its own of which the Y channel is taken [1].
// [1] http://wiki.webmproject.org/alpha-channel
class MEDIA_EXPORT VpxVideoDecoder : public VideoDecoder {
 public:
  VpxVideoDecoder();
  ~VpxVideoDecoder() override;

  // VideoDecoder implementation.
  std::string GetDisplayName() const override;
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  const InitCB& init_cb,
                  const OutputCB& output_cb) override;
  void Decode(const scoped_refptr<DecoderBuffer>& buffer,
              const DecodeCB& decode_cb) override;
  void Reset(const base::Closure& closure) override;

 private:
  enum DecoderState {
    kUninitialized,
    kNormal,
    kFlushCodec,
    kDecodeFinished,
    kError
  };

  // Return values for decoding alpha plane.
  enum AlphaDecodeStatus {
    kAlphaPlaneProcessed,  // Alpha plane (if found) was decoded successfully.
    kNoAlphaPlaneData,  // Alpha plane was found, but decoder did not return any
                        // data.
    kAlphaPlaneError  // Fatal error occured when trying to decode alpha plane.
  };

  // Handles (re-)initializing the decoder with a (new) config.
  // Returns true when initialization was successful.
  bool ConfigureDecoder(const VideoDecoderConfig& config);

  void CloseDecoder();

  // Helper method for decoding buffers either on the offload thread or directly
  // on the media thread. |bound_decode_cb| must be bound to the thread that
  // called Decode().
  void DecodeBuffer(const scoped_refptr<DecoderBuffer>& buffer,
                    const DecodeCB& bound_decode_cb);

  // Try to decode |buffer| into |video_frame|. Return true if all decoding
  // succeeded. Note that decoding can succeed and still |video_frame| be
  // nullptr if there has been a partial decoding.
  bool VpxDecode(const scoped_refptr<DecoderBuffer>& buffer,
                 scoped_refptr<VideoFrame>* video_frame);

  bool CopyVpxImageToVideoFrame(const struct vpx_image* vpx_image,
                                const struct vpx_image* vpx_image_alpha,
                                scoped_refptr<VideoFrame>* video_frame);

  AlphaDecodeStatus DecodeAlphaPlane(
      const struct vpx_image* vpx_image,
      const struct vpx_image** vpx_image_alpha,
      const scoped_refptr<DecoderBuffer>& buffer);

  base::ThreadChecker thread_checker_;

  // |state_| must only be read and written to on |offload_task_runner_| if it
  // is non-null and there are outstanding tasks on the offload thread.
  DecoderState state_;

  OutputCB output_cb_;

  VideoDecoderConfig config_;

  vpx_codec_ctx* vpx_codec_;
  vpx_codec_ctx* vpx_codec_alpha_;

  // |memory_pool_| is a single-threaded memory pool used for VP9 decoding
  // with no alpha. |frame_pool_| is used for all other cases.
  class MemoryPool;
  scoped_refptr<MemoryPool> memory_pool_;

  // High resolution vp9 may block the media thread for too long, in such cases
  // we share a per-process thread to avoid overly long blocks.
  scoped_refptr<base::SingleThreadTaskRunner> offload_task_runner_;

  VideoFramePool frame_pool_;

  DISALLOW_COPY_AND_ASSIGN(VpxVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_VPX_VIDEO_DECODER_H_
