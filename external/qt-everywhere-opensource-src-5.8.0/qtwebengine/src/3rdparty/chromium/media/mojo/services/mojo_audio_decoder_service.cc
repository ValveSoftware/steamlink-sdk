// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_audio_decoder_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "media/base/cdm_context.h"
#include "media/base/media_keys.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "media/mojo/services/mojo_cdm_service_context.h"

namespace media {

MojoAudioDecoderService::MojoAudioDecoderService(
    base::WeakPtr<MojoCdmServiceContext> mojo_cdm_service_context,
    std::unique_ptr<media::AudioDecoder> decoder,
    mojo::InterfaceRequest<mojom::AudioDecoder> request)
    : binding_(this, std::move(request)),
      mojo_cdm_service_context_(mojo_cdm_service_context),
      decoder_(std::move(decoder)),
      weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
}

MojoAudioDecoderService::~MojoAudioDecoderService() {}

void MojoAudioDecoderService::Initialize(mojom::AudioDecoderClientPtr client,
                                         mojom::AudioDecoderConfigPtr config,
                                         int32_t cdm_id,
                                         const InitializeCallback& callback) {
  DVLOG(1) << __FUNCTION__ << " "
           << config.To<media::AudioDecoderConfig>().AsHumanReadableString();

  // Get CdmContext from cdm_id if the stream is encrypted.
  CdmContext* cdm_context = nullptr;
  scoped_refptr<MediaKeys> cdm;
  if (config.To<media::AudioDecoderConfig>().is_encrypted()) {
    if (!mojo_cdm_service_context_) {
      DVLOG(1) << "CDM service context not available.";
      callback.Run(false, false);
      return;
    }

    cdm = mojo_cdm_service_context_->GetCdm(cdm_id);
    if (!cdm) {
      DVLOG(1) << "CDM not found for CDM id: " << cdm_id;
      callback.Run(false, false);
      return;
    }

    cdm_context = cdm->GetCdmContext();
    if (!cdm_context) {
      DVLOG(1) << "CDM context not available for CDM id: " << cdm_id;
      callback.Run(false, false);
      return;
    }
  }

  client_ = std::move(client);

  decoder_->Initialize(
      config.To<media::AudioDecoderConfig>(), cdm_context,
      base::Bind(&MojoAudioDecoderService::OnInitialized, weak_this_, callback,
                 cdm),
      base::Bind(&MojoAudioDecoderService::OnAudioBufferReady, weak_this_));
}

void MojoAudioDecoderService::SetDataSource(
    mojo::ScopedDataPipeConsumerHandle receive_pipe) {
  DVLOG(1) << __FUNCTION__;

  mojo_decoder_buffer_reader_.reset(
      new MojoDecoderBufferReader(std::move(receive_pipe)));
}

void MojoAudioDecoderService::Decode(mojom::DecoderBufferPtr buffer,
                                     const DecodeCallback& callback) {
  DVLOG(3) << __FUNCTION__;

  scoped_refptr<DecoderBuffer> media_buffer =
      mojo_decoder_buffer_reader_->ReadDecoderBuffer(buffer);
  if (!media_buffer) {
    callback.Run(mojom::DecodeStatus::DECODE_ERROR);
    return;
  }

  decoder_->Decode(media_buffer,
                   base::Bind(&MojoAudioDecoderService::OnDecodeStatus,
                              weak_this_, callback));
}

void MojoAudioDecoderService::Reset(const ResetCallback& callback) {
  DVLOG(1) << __FUNCTION__;
  decoder_->Reset(
      base::Bind(&MojoAudioDecoderService::OnResetDone, weak_this_, callback));
}

void MojoAudioDecoderService::OnInitialized(const InitializeCallback& callback,
                                            scoped_refptr<MediaKeys> cdm,
                                            bool success) {
  DVLOG(1) << __FUNCTION__ << " success:" << success;

  if (success) {
    cdm_ = cdm;
    callback.Run(success, decoder_->NeedsBitstreamConversion());
  } else {
    // Do not call decoder_->NeedsBitstreamConversion() if init failed.
    callback.Run(false, false);
  }
}

void MojoAudioDecoderService::OnDecodeStatus(const DecodeCallback& callback,
                                             media::DecodeStatus status) {
  DVLOG(3) << __FUNCTION__ << " status:" << status;
  callback.Run(static_cast<mojom::DecodeStatus>(status));
}

void MojoAudioDecoderService::OnResetDone(const ResetCallback& callback) {
  DVLOG(1) << __FUNCTION__;
  callback.Run();
}

void MojoAudioDecoderService::OnAudioBufferReady(
    const scoped_refptr<AudioBuffer>& audio_buffer) {
  DVLOG(1) << __FUNCTION__;

  // TODO(timav): Use DataPipe.
  client_->OnBufferDecoded(mojom::AudioBuffer::From(audio_buffer));
}

}  // namespace media
