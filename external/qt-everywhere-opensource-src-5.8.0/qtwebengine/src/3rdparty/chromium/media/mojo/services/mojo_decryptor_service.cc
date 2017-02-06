// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_decryptor_service.h"

#include <utility>

#include "base/bind.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decryptor.h"
#include "media/base/media_keys.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "media/mojo/common/mojo_shared_buffer_video_frame.h"
#include "media/mojo/interfaces/demuxer_stream.mojom.h"

namespace media {

MojoDecryptorService::MojoDecryptorService(
    const scoped_refptr<MediaKeys>& cdm,
    mojo::InterfaceRequest<mojom::Decryptor> request,
    const base::Closure& error_handler)
    : binding_(this, std::move(request)), cdm_(cdm), weak_factory_(this) {
  DVLOG(1) << __FUNCTION__;
  decryptor_ = cdm->GetCdmContext()->GetDecryptor();
  DCHECK(decryptor_);
  weak_this_ = weak_factory_.GetWeakPtr();
  binding_.set_connection_error_handler(error_handler);
}

MojoDecryptorService::~MojoDecryptorService() {}

void MojoDecryptorService::Initialize(
    mojo::ScopedDataPipeConsumerHandle receive_pipe,
    mojo::ScopedDataPipeProducerHandle transmit_pipe) {
  mojo_decoder_buffer_writer_.reset(
      new MojoDecoderBufferWriter(std::move(transmit_pipe)));
  mojo_decoder_buffer_reader_.reset(
      new MojoDecoderBufferReader(std::move(receive_pipe)));
}

void MojoDecryptorService::Decrypt(mojom::DemuxerStream::Type stream_type,
                                   mojom::DecoderBufferPtr encrypted,
                                   const DecryptCallback& callback) {
  DVLOG(3) << __FUNCTION__;

  scoped_refptr<DecoderBuffer> media_buffer =
      mojo_decoder_buffer_reader_->ReadDecoderBuffer(encrypted);
  if (!media_buffer) {
    callback.Run(Status::DECRYPTION_ERROR, nullptr);
    return;
  }

  decryptor_->Decrypt(
      static_cast<media::Decryptor::StreamType>(stream_type),
      std::move(media_buffer),
      base::Bind(&MojoDecryptorService::OnDecryptDone, weak_this_, callback));
}

void MojoDecryptorService::CancelDecrypt(
    mojom::DemuxerStream::Type stream_type) {
  DVLOG(1) << __FUNCTION__;
  decryptor_->CancelDecrypt(
      static_cast<media::Decryptor::StreamType>(stream_type));
}

void MojoDecryptorService::InitializeAudioDecoder(
    mojom::AudioDecoderConfigPtr config,
    const InitializeAudioDecoderCallback& callback) {
  DVLOG(1) << __FUNCTION__;
  decryptor_->InitializeAudioDecoder(
      config.To<AudioDecoderConfig>(),
      base::Bind(&MojoDecryptorService::OnAudioDecoderInitialized, weak_this_,
                 callback));
}

void MojoDecryptorService::InitializeVideoDecoder(
    mojom::VideoDecoderConfigPtr config,
    const InitializeVideoDecoderCallback& callback) {
  DVLOG(1) << __FUNCTION__;
  decryptor_->InitializeVideoDecoder(
      config.To<VideoDecoderConfig>(),
      base::Bind(&MojoDecryptorService::OnVideoDecoderInitialized, weak_this_,
                 callback));
}

void MojoDecryptorService::DecryptAndDecodeAudio(
    mojom::DecoderBufferPtr encrypted,
    const DecryptAndDecodeAudioCallback& callback) {
  DVLOG(3) << __FUNCTION__;

  scoped_refptr<DecoderBuffer> media_buffer =
      mojo_decoder_buffer_reader_->ReadDecoderBuffer(encrypted);
  if (!media_buffer) {
    callback.Run(Status::DECRYPTION_ERROR, nullptr);
    return;
  }

  decryptor_->DecryptAndDecodeAudio(
      std::move(media_buffer),
      base::Bind(&MojoDecryptorService::OnAudioDecoded, weak_this_, callback));
}

void MojoDecryptorService::DecryptAndDecodeVideo(
    mojom::DecoderBufferPtr encrypted,
    const DecryptAndDecodeVideoCallback& callback) {
  DVLOG(3) << __FUNCTION__;

  scoped_refptr<DecoderBuffer> media_buffer =
      mojo_decoder_buffer_reader_->ReadDecoderBuffer(encrypted);
  if (!media_buffer) {
    callback.Run(Status::DECRYPTION_ERROR, nullptr);
    return;
  }

  decryptor_->DecryptAndDecodeVideo(
      std::move(media_buffer),
      base::Bind(&MojoDecryptorService::OnVideoDecoded, weak_this_, callback));
}

void MojoDecryptorService::ResetDecoder(
    mojom::DemuxerStream::Type stream_type) {
  DVLOG(1) << __FUNCTION__;
  decryptor_->ResetDecoder(
      static_cast<media::Decryptor::StreamType>(stream_type));
}

void MojoDecryptorService::DeinitializeDecoder(
    mojom::DemuxerStream::Type stream_type) {
  DVLOG(1) << __FUNCTION__;
  decryptor_->DeinitializeDecoder(
      static_cast<media::Decryptor::StreamType>(stream_type));
}

void MojoDecryptorService::ReleaseSharedBuffer(
    mojo::ScopedSharedBufferHandle buffer,
    uint64_t buffer_size) {
  in_use_video_frames_.erase(buffer.get().value());
}

void MojoDecryptorService::OnDecryptDone(
    const DecryptCallback& callback,
    media::Decryptor::Status status,
    const scoped_refptr<DecoderBuffer>& buffer) {
  DVLOG_IF(1, status != media::Decryptor::kSuccess) << __FUNCTION__ << "("
                                                    << status << ")";
  DVLOG_IF(3, status == media::Decryptor::kSuccess) << __FUNCTION__;

  if (!buffer) {
    DCHECK_NE(status, media::Decryptor::kSuccess);
    callback.Run(static_cast<Decryptor::Status>(status), nullptr);
    return;
  }

  mojom::DecoderBufferPtr mojo_buffer =
      mojo_decoder_buffer_writer_->WriteDecoderBuffer(buffer);
  if (!mojo_buffer) {
    callback.Run(Status::DECRYPTION_ERROR, nullptr);
    return;
  }

  callback.Run(static_cast<Decryptor::Status>(status), std::move(mojo_buffer));
}

void MojoDecryptorService::OnAudioDecoderInitialized(
    const InitializeAudioDecoderCallback& callback,
    bool success) {
  DVLOG(1) << __FUNCTION__ << "(" << success << ")";
  callback.Run(success);
}

void MojoDecryptorService::OnVideoDecoderInitialized(
    const InitializeVideoDecoderCallback& callback,
    bool success) {
  DVLOG(1) << __FUNCTION__ << "(" << success << ")";
  callback.Run(success);
}

void MojoDecryptorService::OnAudioDecoded(
    const DecryptAndDecodeAudioCallback& callback,
    media::Decryptor::Status status,
    const media::Decryptor::AudioFrames& frames) {
  DVLOG_IF(1, status != media::Decryptor::kSuccess) << __FUNCTION__ << "("
                                                    << status << ")";
  DVLOG_IF(3, status == media::Decryptor::kSuccess) << __FUNCTION__;

  mojo::Array<mojom::AudioBufferPtr> audio_buffers;
  for (const auto& frame : frames)
    audio_buffers.push_back(mojom::AudioBuffer::From(frame));

  callback.Run(static_cast<Decryptor::Status>(status),
               std::move(audio_buffers));
}

void MojoDecryptorService::OnVideoDecoded(
    const DecryptAndDecodeVideoCallback& callback,
    media::Decryptor::Status status,
    const scoped_refptr<VideoFrame>& frame) {
  DVLOG_IF(1, status != media::Decryptor::kSuccess) << __FUNCTION__ << "("
                                                    << status << ")";
  DVLOG_IF(3, status == media::Decryptor::kSuccess) << __FUNCTION__;

  if (!frame) {
    DCHECK_NE(status, media::Decryptor::kSuccess);
    callback.Run(static_cast<Decryptor::Status>(status), nullptr);
    return;
  }

  // If |frame| has shared memory that will be passed back, keep a reference
  // to it until the other side is done with the memory.
  if (frame->storage_type() == VideoFrame::STORAGE_MOJO_SHARED_BUFFER) {
    MojoSharedBufferVideoFrame* mojo_frame =
        static_cast<MojoSharedBufferVideoFrame*>(frame.get());
    in_use_video_frames_.insert(
        std::make_pair(mojo_frame->Handle().value(), frame));
  }

  callback.Run(static_cast<Decryptor::Status>(status),
               mojom::VideoFrame::From(frame));
}

}  // namespace media
