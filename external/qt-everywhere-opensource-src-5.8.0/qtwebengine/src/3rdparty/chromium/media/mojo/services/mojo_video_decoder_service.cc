// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_video_decoder_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "media/mojo/services/mojo_media_client.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/handle.h"

namespace media {

MojoVideoDecoderService::MojoVideoDecoderService(
    mojo::InterfaceRequest<mojom::VideoDecoder> request,
    MojoMediaClient* mojo_media_client)
    : binding_(this, std::move(request)),
      mojo_media_client_(mojo_media_client),
      weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
}

MojoVideoDecoderService::~MojoVideoDecoderService() {}

void MojoVideoDecoderService::Construct(
    mojom::VideoDecoderClientPtr client,
    mojo::ScopedDataPipeConsumerHandle decoder_buffer_pipe) {
  DVLOG(1) << __FUNCTION__;

  if (decoder_)
    return;

  // TODO(sandersd): Provide callback for requesting a stub.
  decoder_ = mojo_media_client_->CreateVideoDecoder(
      base::ThreadTaskRunnerHandle::Get());

  client_ = std::move(client);

  mojo_decoder_buffer_reader_.reset(
      new MojoDecoderBufferReader(std::move(decoder_buffer_pipe)));
}

void MojoVideoDecoderService::Initialize(mojom::VideoDecoderConfigPtr config,
                                         bool low_delay,
                                         const InitializeCallback& callback) {
  DVLOG(1) << __FUNCTION__;

  if (!decoder_) {
    callback.Run(false);
    return;
  }

  decoder_->Initialize(
      config.To<VideoDecoderConfig>(), low_delay, nullptr,
      base::Bind(&MojoVideoDecoderService::OnDecoderInitialized, weak_this_,
                 callback),
      base::Bind(&MojoVideoDecoderService::OnDecoderOutput, weak_this_));
}

void MojoVideoDecoderService::OnDecoderInitialized(
    const InitializeCallback& callback,
    bool success) {
  DVLOG(1) << __FUNCTION__;
  callback.Run(success);
}

void MojoVideoDecoderService::OnDecoderOutput(
    const scoped_refptr<VideoFrame>& frame) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(client_);
  client_->OnVideoFrameDecoded(mojom::VideoFrame::From(frame));
}

void MojoVideoDecoderService::Decode(mojom::DecoderBufferPtr buffer,
                                     const DecodeCallback& callback) {
  DVLOG(1) << __FUNCTION__;

  if (!decoder_) {
    callback.Run(mojom::DecodeStatus::DECODE_ERROR);
    return;
  }

  // TODO(sandersd): After a decode error, we should enter an error state and
  // reject all future method calls.
  scoped_refptr<DecoderBuffer> media_buffer =
      mojo_decoder_buffer_reader_->ReadDecoderBuffer(buffer);
  if (!media_buffer) {
    callback.Run(mojom::DecodeStatus::DECODE_ERROR);
    return;
  }

  decoder_->Decode(media_buffer,
                   base::Bind(&MojoVideoDecoderService::OnDecoderDecoded,
                              weak_this_, callback));
}

void MojoVideoDecoderService::OnDecoderDecoded(const DecodeCallback& callback,
                                               DecodeStatus status) {
  DVLOG(1) << __FUNCTION__;
  callback.Run(static_cast<mojom::DecodeStatus>(status));
}

void MojoVideoDecoderService::Reset(const ResetCallback& callback) {
  DVLOG(1) << __FUNCTION__;

  if (!decoder_) {
    callback.Run();
    return;
  }

  decoder_->Reset(base::Bind(&MojoVideoDecoderService::OnDecoderReset,
                             weak_this_, callback));
}

void MojoVideoDecoderService::OnDecoderReset(const ResetCallback& callback) {
  DVLOG(1) << __FUNCTION__;
  callback.Run();
}

}  // namespace media
