// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOJO_VIDEO_DECODER_H_
#define MEDIA_MOJO_CLIENTS_MOJO_VIDEO_DECODER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/base/video_decoder.h"
#include "media/mojo/interfaces/video_decoder.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class GpuVideoAcceleratorFactories;
class MojoDecoderBufferWriter;

// A VideoDecoder, for use in the renderer process, that proxies to a
// mojom::VideoDecoder. It is assumed that the other side will be implemented by
// MojoVideoDecoderService, running in the GPU process, and that the remote
// decoder will be hardware accelerated.
class MojoVideoDecoder final : public VideoDecoder,
                               public mojom::VideoDecoderClient {
 public:
  MojoVideoDecoder(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                   GpuVideoAcceleratorFactories* gpu_factories,
                   mojom::VideoDecoderPtr remote_decoder);
  ~MojoVideoDecoder() final;

  // VideoDecoder implementation.
  std::string GetDisplayName() const final;
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  const InitCB& init_cb,
                  const OutputCB& output_cb) final;
  void Decode(const scoped_refptr<DecoderBuffer>& buffer,
              const DecodeCB& decode_cb) final;
  void Reset(const base::Closure& closure) final;
  bool NeedsBitstreamConversion() const final;
  bool CanReadWithoutStalling() const final;
  int GetMaxDecodeRequests() const final;

  // mojom::VideoDecoderClient implementation.
  void OnVideoFrameDecoded(mojom::VideoFramePtr frame) final;

 private:
  void OnInitializeDone(bool status);
  void OnDecodeDone(mojom::DecodeStatus status);
  void OnResetDone();

  void BindRemoteDecoder();
  void OnConnectionError();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  GpuVideoAcceleratorFactories* gpu_factories_;

  // Used to pass the remote decoder from the constructor (on the main thread)
  // to Initialize() (on the media thread).
  mojom::VideoDecoderPtrInfo remote_decoder_info_;

  InitCB init_cb_;
  OutputCB output_cb_;
  DecodeCB decode_cb_;
  base::Closure reset_cb_;

  mojom::VideoDecoderPtr remote_decoder_;
  std::unique_ptr<MojoDecoderBufferWriter> mojo_decoder_buffer_writer_;
  bool remote_decoder_bound_ = false;
  bool has_connection_error_ = false;
  mojo::Binding<VideoDecoderClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(MojoVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_MOJO_VIDEO_DECODER_H_
