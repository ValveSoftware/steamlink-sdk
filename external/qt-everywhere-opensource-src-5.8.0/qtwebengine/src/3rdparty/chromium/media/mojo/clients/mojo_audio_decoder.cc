// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_audio_decoder.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/audio_buffer.h"
#include "media/base/cdm_context.h"
#include "media/base/demuxer_stream.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"

namespace media {

MojoAudioDecoder::MojoAudioDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojom::AudioDecoderPtr remote_decoder)
    : task_runner_(task_runner),
      remote_decoder_info_(remote_decoder.PassInterface()),
      binding_(this),
      has_connection_error_(false),
      needs_bitstream_conversion_(false) {
  DVLOG(1) << __FUNCTION__;
}

MojoAudioDecoder::~MojoAudioDecoder() {
  DVLOG(1) << __FUNCTION__;
}

std::string MojoAudioDecoder::GetDisplayName() const {
  return "MojoAudioDecoder";
}

void MojoAudioDecoder::Initialize(const AudioDecoderConfig& config,
                                  CdmContext* cdm_context,
                                  const InitCB& init_cb,
                                  const OutputCB& output_cb) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Bind |remote_decoder_| to the |task_runner_|.
  remote_decoder_.Bind(std::move(remote_decoder_info_));

  // Fail immediately if the stream is encrypted but |cdm_context| is invalid.
  int cdm_id = (config.is_encrypted() && cdm_context)
                   ? cdm_context->GetCdmId()
                   : CdmContext::kInvalidCdmId;

  if (config.is_encrypted() && CdmContext::kInvalidCdmId == cdm_id) {
    task_runner_->PostTask(FROM_HERE, base::Bind(init_cb, false));
    return;
  }

  // Otherwise, set an error handler to catch the connection error.
  // Using base::Unretained(this) is safe because |this| owns |remote_decoder_|,
  // and the error handler can't be invoked once |remote_decoder_| is destroyed.
  remote_decoder_.set_connection_error_handler(
      base::Bind(&MojoAudioDecoder::OnConnectionError, base::Unretained(this)));

  init_cb_ = init_cb;
  output_cb_ = output_cb;

  // Using base::Unretained(this) is safe because |this| owns |remote_decoder_|,
  // and the callback won't be dispatched if |remote_decoder_| is destroyed.
  remote_decoder_->Initialize(
      binding_.CreateInterfacePtrAndBind(),
      mojom::AudioDecoderConfig::From(config), cdm_id,
      base::Bind(&MojoAudioDecoder::OnInitialized, base::Unretained(this)));
}

void MojoAudioDecoder::Decode(const scoped_refptr<DecoderBuffer>& media_buffer,
                              const DecodeCB& decode_cb) {
  DVLOG(3) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (has_connection_error_) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(decode_cb, DecodeStatus::DECODE_ERROR));
    return;
  }

  mojom::DecoderBufferPtr buffer =
      mojo_decoder_buffer_writer_->WriteDecoderBuffer(media_buffer);
  if (!buffer) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(decode_cb, DecodeStatus::DECODE_ERROR));
    return;
  }

  DCHECK(decode_cb_.is_null());
  decode_cb_ = decode_cb;

  remote_decoder_->Decode(
      std::move(buffer),
      base::Bind(&MojoAudioDecoder::OnDecodeStatus, base::Unretained(this)));
}

void MojoAudioDecoder::Reset(const base::Closure& closure) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (has_connection_error_) {
    if (!decode_cb_.is_null()) {
      task_runner_->PostTask(FROM_HERE,
                             base::Bind(base::ResetAndReturn(&decode_cb_),
                                        DecodeStatus::DECODE_ERROR));
    }

    task_runner_->PostTask(FROM_HERE, closure);
    return;
  }

  DCHECK(reset_cb_.is_null());
  reset_cb_ = closure;
  remote_decoder_->Reset(
      base::Bind(&MojoAudioDecoder::OnResetDone, base::Unretained(this)));
}

bool MojoAudioDecoder::NeedsBitstreamConversion() const {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  return needs_bitstream_conversion_;
}

void MojoAudioDecoder::OnBufferDecoded(mojom::AudioBufferPtr buffer) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  output_cb_.Run(buffer.To<scoped_refptr<AudioBuffer>>());
}

void MojoAudioDecoder::OnConnectionError() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  has_connection_error_ = true;

  if (!init_cb_.is_null()) {
    base::ResetAndReturn(&init_cb_).Run(false);
    return;
  }

  if (!decode_cb_.is_null())
    base::ResetAndReturn(&decode_cb_).Run(DecodeStatus::DECODE_ERROR);
  if (!reset_cb_.is_null())
    base::ResetAndReturn(&reset_cb_).Run();
}

void MojoAudioDecoder::OnInitialized(bool success,
                                     bool needs_bitstream_conversion) {
  DVLOG(1) << __FUNCTION__ << ": success:" << success;
  DCHECK(task_runner_->BelongsToCurrentThread());

  needs_bitstream_conversion_ = needs_bitstream_conversion;

  if (success) {
    mojo::ScopedDataPipeConsumerHandle remote_consumer_handle;
    mojo_decoder_buffer_writer_ = MojoDecoderBufferWriter::Create(
        DemuxerStream::AUDIO, &remote_consumer_handle);
    // Pass consumer end to |remote_decoder_|.
    remote_decoder_->SetDataSource(std::move(remote_consumer_handle));
  }

  base::ResetAndReturn(&init_cb_).Run(success);
}

void MojoAudioDecoder::OnDecodeStatus(mojom::DecodeStatus status) {
  DVLOG(1) << __FUNCTION__ << ": status:" << status;
  DCHECK(task_runner_->BelongsToCurrentThread());

  DCHECK(!decode_cb_.is_null());
  base::ResetAndReturn(&decode_cb_).Run(static_cast<DecodeStatus>(status));
}

void MojoAudioDecoder::OnResetDone() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  // For pending decodes OnDecodeStatus() should arrive before OnResetDone().
  DCHECK(decode_cb_.is_null());

  DCHECK(!reset_cb_.is_null());
  base::ResetAndReturn(&reset_cb_).Run();
}

}  // namespace media
