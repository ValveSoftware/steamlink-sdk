// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_VIDEO_DECODER_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MOJO_VIDEO_DECODER_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "media/base/decode_status.h"
#include "media/mojo/interfaces/video_decoder.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace media {

class MojoDecoderBufferReader;
class MojoMediaClient;
class VideoDecoder;
class VideoFrame;

// Implementation of a mojom::VideoDecoder which runs in the GPU process,
// and wraps a media::VideoDecoder.
class MojoVideoDecoderService : public mojom::VideoDecoder {
 public:
  MojoVideoDecoderService(mojo::InterfaceRequest<mojom::VideoDecoder> request,
                          MojoMediaClient* mojo_media_client);
  ~MojoVideoDecoderService() final;

  // mojom::VideoDecoder implementation
  void Construct(mojom::VideoDecoderClientPtr client,
                 mojo::ScopedDataPipeConsumerHandle decoder_buffer_pipe) final;
  void Initialize(mojom::VideoDecoderConfigPtr config,
                  bool low_delay,
                  const InitializeCallback& callback) final;
  void Decode(mojom::DecoderBufferPtr buffer,
              const DecodeCallback& callback) final;
  void Reset(const ResetCallback& callback) final;

 private:
  void OnDecoderInitialized(const InitializeCallback& callback, bool success);
  void OnDecoderDecoded(const DecodeCallback& callback, DecodeStatus status);
  void OnDecoderOutput(const scoped_refptr<VideoFrame>& frame);
  void OnDecoderReset(const ResetCallback& callback);

  mojo::StrongBinding<mojom::VideoDecoder> binding_;
  mojom::VideoDecoderClientPtr client_;
  std::unique_ptr<MojoDecoderBufferReader> mojo_decoder_buffer_reader_;

  MojoMediaClient* mojo_media_client_;
  std::unique_ptr<media::VideoDecoder> decoder_;

  base::WeakPtr<MojoVideoDecoderService> weak_this_;
  base::WeakPtrFactory<MojoVideoDecoderService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoVideoDecoderService);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_VIDEO_DECODER_SERVICE_H_
