// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_decryptor.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/audio_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_frame.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "media/mojo/common/mojo_shared_buffer_video_frame.h"
#include "media/mojo/interfaces/decryptor.mojom.h"
#include "services/shell/public/cpp/connect.h"

namespace media {

MojoDecryptor::MojoDecryptor(mojom::DecryptorPtr remote_decryptor)
    : remote_decryptor_(std::move(remote_decryptor)), weak_factory_(this) {
  DVLOG(1) << __FUNCTION__;

  // Allocate DataPipe size based on video content.

  mojo::ScopedDataPipeConsumerHandle remote_consumer_handle;
  mojo_decoder_buffer_writer_ = MojoDecoderBufferWriter::Create(
      DemuxerStream::VIDEO, &remote_consumer_handle);

  mojo::ScopedDataPipeProducerHandle remote_producer_handle;
  mojo_decoder_buffer_reader_ = MojoDecoderBufferReader::Create(
      DemuxerStream::VIDEO, &remote_producer_handle);

  // Pass the other end of each pipe to |remote_decryptor_|.
  remote_decryptor_->Initialize(std::move(remote_consumer_handle),
                                std::move(remote_producer_handle));
}

MojoDecryptor::~MojoDecryptor() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
}

void MojoDecryptor::RegisterNewKeyCB(StreamType stream_type,
                                     const NewKeyCB& key_added_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  switch (stream_type) {
    case kAudio:
      new_audio_key_cb_ = key_added_cb;
      break;
    case kVideo:
      new_video_key_cb_ = key_added_cb;
      break;
    default:
      NOTREACHED();
  }
}

void MojoDecryptor::Decrypt(StreamType stream_type,
                            const scoped_refptr<DecoderBuffer>& encrypted,
                            const DecryptCB& decrypt_cb) {
  DVLOG(3) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  mojom::DecoderBufferPtr mojo_buffer =
      mojo_decoder_buffer_writer_->WriteDecoderBuffer(encrypted);
  if (!mojo_buffer) {
    decrypt_cb.Run(kError, nullptr);
    return;
  }

  remote_decryptor_->Decrypt(
      static_cast<mojom::DemuxerStream::Type>(stream_type),
      std::move(mojo_buffer),
      base::Bind(&MojoDecryptor::OnBufferDecrypted, weak_factory_.GetWeakPtr(),
                 decrypt_cb));
}

void MojoDecryptor::CancelDecrypt(StreamType stream_type) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_decryptor_->CancelDecrypt(
      static_cast<mojom::DemuxerStream::Type>(stream_type));
}

void MojoDecryptor::InitializeAudioDecoder(const AudioDecoderConfig& config,
                                           const DecoderInitCB& init_cb) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_decryptor_->InitializeAudioDecoder(
      mojom::AudioDecoderConfig::From(config), init_cb);
}

void MojoDecryptor::InitializeVideoDecoder(const VideoDecoderConfig& config,
                                           const DecoderInitCB& init_cb) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_decryptor_->InitializeVideoDecoder(
      mojom::VideoDecoderConfig::From(config), init_cb);
}

void MojoDecryptor::DecryptAndDecodeAudio(
    const scoped_refptr<DecoderBuffer>& encrypted,
    const AudioDecodeCB& audio_decode_cb) {
  DVLOG(3) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  mojom::DecoderBufferPtr mojo_buffer =
      mojo_decoder_buffer_writer_->WriteDecoderBuffer(encrypted);
  if (!mojo_buffer) {
    audio_decode_cb.Run(kError, AudioFrames());
    return;
  }

  remote_decryptor_->DecryptAndDecodeAudio(
      std::move(mojo_buffer),
      base::Bind(&MojoDecryptor::OnAudioDecoded, weak_factory_.GetWeakPtr(),
                 audio_decode_cb));
}

void MojoDecryptor::DecryptAndDecodeVideo(
    const scoped_refptr<DecoderBuffer>& encrypted,
    const VideoDecodeCB& video_decode_cb) {
  DVLOG(3) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  mojom::DecoderBufferPtr mojo_buffer =
      mojo_decoder_buffer_writer_->WriteDecoderBuffer(encrypted);
  if (!mojo_buffer) {
    video_decode_cb.Run(kError, nullptr);
    return;
  }

  remote_decryptor_->DecryptAndDecodeVideo(
      std::move(mojo_buffer),
      base::Bind(&MojoDecryptor::OnVideoDecoded, weak_factory_.GetWeakPtr(),
                 video_decode_cb));
}

void MojoDecryptor::ResetDecoder(StreamType stream_type) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_decryptor_->ResetDecoder(
      static_cast<mojom::DemuxerStream::Type>(stream_type));
}

void MojoDecryptor::DeinitializeDecoder(StreamType stream_type) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_decryptor_->DeinitializeDecoder(
      static_cast<mojom::DemuxerStream::Type>(stream_type));
}

void MojoDecryptor::OnKeyAdded() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!new_audio_key_cb_.is_null())
    new_audio_key_cb_.Run();

  if (!new_video_key_cb_.is_null())
    new_video_key_cb_.Run();
}

void MojoDecryptor::OnBufferDecrypted(const DecryptCB& decrypt_cb,
                                      mojom::Decryptor::Status status,
                                      mojom::DecoderBufferPtr buffer) {
  DVLOG_IF(1, status != mojom::Decryptor::Status::SUCCESS)
      << __FUNCTION__ << "(" << status << ")";
  DVLOG_IF(3, status == mojom::Decryptor::Status::SUCCESS) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (buffer.is_null()) {
    decrypt_cb.Run(static_cast<Decryptor::Status>(status), nullptr);
    return;
  }

  scoped_refptr<DecoderBuffer> media_buffer =
      mojo_decoder_buffer_reader_->ReadDecoderBuffer(buffer);
  if (!media_buffer) {
    decrypt_cb.Run(kError, nullptr);
    return;
  }

  decrypt_cb.Run(static_cast<Decryptor::Status>(status), media_buffer);
}

void MojoDecryptor::OnAudioDecoded(
    const AudioDecodeCB& audio_decode_cb,
    mojom::Decryptor::Status status,
    mojo::Array<mojom::AudioBufferPtr> audio_buffers) {
  DVLOG_IF(1, status != mojom::Decryptor::Status::SUCCESS)
      << __FUNCTION__ << "(" << status << ")";
  DVLOG_IF(3, status == mojom::Decryptor::Status::SUCCESS) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  Decryptor::AudioFrames audio_frames;
  for (size_t i = 0; i < audio_buffers.size(); ++i)
    audio_frames.push_back(audio_buffers[i].To<scoped_refptr<AudioBuffer>>());

  audio_decode_cb.Run(static_cast<Decryptor::Status>(status), audio_frames);
}

void MojoDecryptor::OnVideoDecoded(const VideoDecodeCB& video_decode_cb,
                                   mojom::Decryptor::Status status,
                                   mojom::VideoFramePtr video_frame) {
  DVLOG_IF(1, status != mojom::Decryptor::Status::SUCCESS)
      << __FUNCTION__ << "(" << status << ")";
  DVLOG_IF(3, status == mojom::Decryptor::Status::SUCCESS) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (video_frame.is_null()) {
    video_decode_cb.Run(static_cast<Decryptor::Status>(status), nullptr);
    return;
  }

  scoped_refptr<VideoFrame> frame(video_frame.To<scoped_refptr<VideoFrame>>());

  // If using shared memory, ensure that ReleaseSharedBuffer() is called when
  // |frame| is destroyed.
  if (frame->storage_type() == VideoFrame::STORAGE_MOJO_SHARED_BUFFER) {
    MojoSharedBufferVideoFrame* mojo_frame =
        static_cast<MojoSharedBufferVideoFrame*>(frame.get());
    mojo_frame->SetMojoSharedBufferDoneCB(base::Bind(
        &MojoDecryptor::ReleaseSharedBuffer, weak_factory_.GetWeakPtr()));
  }

  video_decode_cb.Run(static_cast<Decryptor::Status>(status), frame);
}

void MojoDecryptor::ReleaseSharedBuffer(mojo::ScopedSharedBufferHandle buffer,
                                        size_t buffer_size) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  remote_decryptor_->ReleaseSharedBuffer(std::move(buffer), buffer_size);
}

}  // namespace media
