// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "decoder_selector.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "media/base/audio_decoder.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/demuxer_stream.h"
#include "media/base/pipeline.h"
#include "media/base/video_decoder.h"
#include "media/filters/decoder_stream_traits.h"
#include "media/filters/decrypting_audio_decoder.h"
#include "media/filters/decrypting_demuxer_stream.h"
#include "media/filters/decrypting_video_decoder.h"

namespace media {

static bool HasValidStreamConfig(DemuxerStream* stream) {
  switch (stream->type()) {
    case DemuxerStream::AUDIO:
      return stream->audio_decoder_config().IsValidConfig();
    case DemuxerStream::VIDEO:
      return stream->video_decoder_config().IsValidConfig();
    case DemuxerStream::UNKNOWN:
    case DemuxerStream::TEXT:
    case DemuxerStream::NUM_TYPES:
      NOTREACHED();
  }
  return false;
}

static bool IsStreamEncrypted(DemuxerStream* stream) {
  switch (stream->type()) {
    case DemuxerStream::AUDIO:
      return stream->audio_decoder_config().is_encrypted();
    case DemuxerStream::VIDEO:
      return stream->video_decoder_config().is_encrypted();
    case DemuxerStream::UNKNOWN:
    case DemuxerStream::TEXT:
    case DemuxerStream::NUM_TYPES:
      NOTREACHED();
  }
  return false;
}

template <DemuxerStream::Type StreamType>
DecoderSelector<StreamType>::DecoderSelector(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    ScopedVector<Decoder> decoders,
    const SetDecryptorReadyCB& set_decryptor_ready_cb)
    : task_runner_(task_runner),
      decoders_(decoders.Pass()),
      set_decryptor_ready_cb_(set_decryptor_ready_cb),
      input_stream_(NULL),
      weak_ptr_factory_(this) {}

template <DemuxerStream::Type StreamType>
DecoderSelector<StreamType>::~DecoderSelector() {
  DVLOG(2) << __FUNCTION__;
  DCHECK(select_decoder_cb_.is_null());
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::SelectDecoder(
    DemuxerStream* stream,
    bool low_delay,
    const SelectDecoderCB& select_decoder_cb,
    const typename Decoder::OutputCB& output_cb) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(stream);

  // Make sure |select_decoder_cb| runs on a different execution stack.
  select_decoder_cb_ = BindToCurrentLoop(select_decoder_cb);

  if (!HasValidStreamConfig(stream)) {
    DLOG(ERROR) << "Invalid stream config.";
    ReturnNullDecoder();
    return;
  }

  input_stream_ = stream;
  low_delay_ = low_delay;
  output_cb_ = output_cb;

  if (!IsStreamEncrypted(input_stream_)) {
    InitializeDecoder();
    return;
  }

  // This could happen if Encrypted Media Extension (EME) is not enabled.
  if (set_decryptor_ready_cb_.is_null()) {
    ReturnNullDecoder();
    return;
  }

  decoder_.reset(new typename StreamTraits::DecryptingDecoderType(
      task_runner_, set_decryptor_ready_cb_));

  DecoderStreamTraits<StreamType>::Initialize(
      decoder_.get(),
      StreamTraits::GetDecoderConfig(*input_stream_),
      low_delay_,
      base::Bind(&DecoderSelector<StreamType>::DecryptingDecoderInitDone,
                 weak_ptr_factory_.GetWeakPtr()),
      output_cb_);
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::Abort() {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  // This could happen when SelectDecoder() was not called or when
  // |select_decoder_cb_| was already posted but not fired (e.g. in the
  // message loop queue).
  if (select_decoder_cb_.is_null())
    return;

  // We must be trying to initialize the |decoder_| or the
  // |decrypted_stream_|. Invalid all weak pointers so that all initialization
  // callbacks won't fire.
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (decoder_) {
    // |decrypted_stream_| is either NULL or already initialized. We don't
    // need to Stop() |decrypted_stream_| in either case.
    decoder_->Stop();
    ReturnNullDecoder();
    return;
  }

  if (decrypted_stream_) {
    decrypted_stream_->Stop(
        base::Bind(&DecoderSelector<StreamType>::ReturnNullDecoder,
                   weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  NOTREACHED();
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::DecryptingDecoderInitDone(
    PipelineStatus status) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (status == PIPELINE_OK) {
    base::ResetAndReturn(&select_decoder_cb_)
        .Run(decoder_.Pass(), scoped_ptr<DecryptingDemuxerStream>());
    return;
  }

  decoder_.reset();

  decrypted_stream_.reset(
      new DecryptingDemuxerStream(task_runner_, set_decryptor_ready_cb_));

  decrypted_stream_->Initialize(
      input_stream_,
      base::Bind(&DecoderSelector<StreamType>::DecryptingDemuxerStreamInitDone,
                 weak_ptr_factory_.GetWeakPtr()));
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::DecryptingDemuxerStreamInitDone(
    PipelineStatus status) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (status != PIPELINE_OK) {
    ReturnNullDecoder();
    return;
  }

  DCHECK(!IsStreamEncrypted(decrypted_stream_.get()));
  input_stream_ = decrypted_stream_.get();
  InitializeDecoder();
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::InitializeDecoder() {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!decoder_);

  if (decoders_.empty()) {
    ReturnNullDecoder();
    return;
  }

  decoder_.reset(decoders_.front());
  decoders_.weak_erase(decoders_.begin());

  DecoderStreamTraits<StreamType>::Initialize(
      decoder_.get(),
      StreamTraits::GetDecoderConfig(*input_stream_),
      low_delay_,
      base::Bind(&DecoderSelector<StreamType>::DecoderInitDone,
                 weak_ptr_factory_.GetWeakPtr()),
      output_cb_);
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::DecoderInitDone(PipelineStatus status) {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (status != PIPELINE_OK) {
    decoder_.reset();
    InitializeDecoder();
    return;
  }

  base::ResetAndReturn(&select_decoder_cb_)
      .Run(decoder_.Pass(), decrypted_stream_.Pass());
}

template <DemuxerStream::Type StreamType>
void DecoderSelector<StreamType>::ReturnNullDecoder() {
  DVLOG(2) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::ResetAndReturn(&select_decoder_cb_)
      .Run(scoped_ptr<Decoder>(),
           scoped_ptr<DecryptingDemuxerStream>());
}

// These forward declarations tell the compiler that we will use
// DecoderSelector with these arguments, allowing us to keep these definitions
// in our .cc without causing linker errors. This also means if anyone tries to
// instantiate a DecoderSelector with anything but these two specializations
// they'll most likely get linker errors.
template class DecoderSelector<DemuxerStream::AUDIO>;
template class DecoderSelector<DemuxerStream::VIDEO>;

}  // namespace media
