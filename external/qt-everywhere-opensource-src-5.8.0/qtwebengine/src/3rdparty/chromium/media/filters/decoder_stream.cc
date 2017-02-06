// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/decoder_stream.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/limits.h"
#include "media/base/media_log.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_decoder.h"
#include "media/base/video_frame.h"
#include "media/filters/decrypting_demuxer_stream.h"

namespace media {

// TODO(rileya): Devise a better way of specifying trace/UMA/etc strings for
// templated classes such as this.
template <DemuxerStream::Type StreamType>
static const char* GetTraceString();

#define FUNCTION_DVLOG(level) \
  DVLOG(level) << __FUNCTION__ << "<" << GetStreamTypeString() << ">"

template <>
const char* GetTraceString<DemuxerStream::VIDEO>() {
  return "DecoderStream<VIDEO>::Decode";
}

template <>
const char* GetTraceString<DemuxerStream::AUDIO>() {
  return "DecoderStream<AUDIO>::Decode";
}

template <DemuxerStream::Type StreamType>
DecoderStream<StreamType>::DecoderStream(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    ScopedVector<Decoder> decoders,
    const scoped_refptr<MediaLog>& media_log)
    : traits_(media_log),
      task_runner_(task_runner),
      media_log_(media_log),
      state_(STATE_UNINITIALIZED),
      stream_(NULL),
      decoder_selector_(new DecoderSelector<StreamType>(task_runner,
                                                        std::move(decoders),
                                                        media_log)),
      decoded_frames_since_fallback_(0),
      active_splice_(false),
      decoding_eos_(false),
      pending_decode_requests_(0),
      duration_tracker_(8),
      received_config_change_during_reinit_(false),
      pending_demuxer_read_(false),
      weak_factory_(this),
      fallback_weak_factory_(this) {}

template <DemuxerStream::Type StreamType>
DecoderStream<StreamType>::~DecoderStream() {
  FUNCTION_DVLOG(2);
  DCHECK(task_runner_->BelongsToCurrentThread());

  decoder_selector_.reset();

  if (!init_cb_.is_null()) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(base::ResetAndReturn(&init_cb_), false));
  }
  if (!read_cb_.is_null()) {
    task_runner_->PostTask(FROM_HERE, base::Bind(
        base::ResetAndReturn(&read_cb_), ABORTED, scoped_refptr<Output>()));
  }
  if (!reset_cb_.is_null())
    task_runner_->PostTask(FROM_HERE, base::ResetAndReturn(&reset_cb_));

  stream_ = NULL;
  decoder_.reset();
  decrypting_demuxer_stream_.reset();
}

template <DemuxerStream::Type StreamType>
std::string DecoderStream<StreamType>::GetStreamTypeString() {
  return DecoderStreamTraits<StreamType>::ToString();
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::Initialize(
    DemuxerStream* stream,
    const InitCB& init_cb,
    CdmContext* cdm_context,
    const StatisticsCB& statistics_cb,
    const base::Closure& waiting_for_decryption_key_cb) {
  FUNCTION_DVLOG(2);
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_UNINITIALIZED);
  DCHECK(init_cb_.is_null());
  DCHECK(!init_cb.is_null());

  statistics_cb_ = statistics_cb;
  init_cb_ = init_cb;
  waiting_for_decryption_key_cb_ = waiting_for_decryption_key_cb;
  stream_ = stream;

  traits_.OnStreamReset(stream_);

  state_ = STATE_INITIALIZING;
  SelectDecoder(cdm_context);
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::Read(const ReadCB& read_cb) {
  FUNCTION_DVLOG(2);
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(state_ != STATE_UNINITIALIZED && state_ != STATE_INITIALIZING)
      << state_;
  // No two reads in the flight at any time.
  DCHECK(read_cb_.is_null());
  // No read during resetting or stopping process.
  DCHECK(reset_cb_.is_null());

  if (state_ == STATE_ERROR) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(read_cb, DECODE_ERROR, scoped_refptr<Output>()));
    return;
  }

  if (state_ == STATE_END_OF_STREAM && ready_outputs_.empty()) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(read_cb, OK, StreamTraits::CreateEOSOutput()));
    return;
  }

  if (!ready_outputs_.empty()) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(read_cb, OK, ready_outputs_.front()));
    ready_outputs_.pop_front();
  } else {
    read_cb_ = read_cb;
  }

  if (state_ == STATE_NORMAL && CanDecodeMore())
    ReadFromDemuxerStream();
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::Reset(const base::Closure& closure) {
  FUNCTION_DVLOG(2);
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_NE(state_, STATE_UNINITIALIZED);
  DCHECK(reset_cb_.is_null());

  reset_cb_ = closure;

  if (!read_cb_.is_null()) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(base::ResetAndReturn(&read_cb_), ABORTED,
                                      scoped_refptr<Output>()));
  }

  ready_outputs_.clear();
  traits_.OnStreamReset(stream_);

  // It's possible to have received a DECODE_ERROR and entered STATE_ERROR right
  // before a Reset() is executed. If we are still waiting for a demuxer read,
  // OnBufferReady() will handle the reset callback.
  // See crbug.com/597605 and crbug.com/607454.
  if (state_ == STATE_ERROR && !pending_demuxer_read_) {
    task_runner_->PostTask(FROM_HERE, base::ResetAndReturn(&reset_cb_));
    return;
  }

  // During decoder reinitialization, the Decoder does not need to be and
  // cannot be Reset(). |decrypting_demuxer_stream_| was reset before decoder
  // reinitialization.
  if (state_ == STATE_REINITIALIZING_DECODER)
    return;

  // |decrypting_demuxer_stream_| will fire all of its read requests when
  // it resets. |reset_cb_| will be fired in OnDecoderReset(), after the
  // decrypting demuxer stream finishes its reset.
  if (decrypting_demuxer_stream_) {
    decrypting_demuxer_stream_->Reset(base::Bind(
        &DecoderStream<StreamType>::ResetDecoder, weak_factory_.GetWeakPtr()));
    return;
  }

  // During pending demuxer read and when not using DecryptingDemuxerStream,
  // the Decoder will be reset after demuxer read is returned
  // (in OnBufferReady()).
  if (pending_demuxer_read_)
    return;

  ResetDecoder();
}

template <DemuxerStream::Type StreamType>
bool DecoderStream<StreamType>::CanReadWithoutStalling() const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return !ready_outputs_.empty() || decoder_->CanReadWithoutStalling();
}

template <>
bool DecoderStream<DemuxerStream::AUDIO>::CanReadWithoutStalling() const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return true;
}

template <DemuxerStream::Type StreamType>
int DecoderStream<StreamType>::GetMaxDecodeRequests() const {
  return decoder_->GetMaxDecodeRequests();
}

template <>
int DecoderStream<DemuxerStream::AUDIO>::GetMaxDecodeRequests() const {
  return 1;
}

template <DemuxerStream::Type StreamType>
bool DecoderStream<StreamType>::CanDecodeMore() const {
  DCHECK(task_runner_->BelongsToCurrentThread());

  bool buffers_left = !(fallback_buffers_.empty() && decoding_eos_);

  // Limit total number of outputs stored in |ready_outputs_| and being decoded.
  // It only makes sense to saturate decoder completely when output queue is
  // empty.
  int num_decodes =
      static_cast<int>(ready_outputs_.size()) + pending_decode_requests_;
  return buffers_left && num_decodes < GetMaxDecodeRequests();
}

template <DemuxerStream::Type StreamType>
base::TimeDelta DecoderStream<StreamType>::AverageDuration() const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return duration_tracker_.count() ? duration_tracker_.Average()
                                   : base::TimeDelta();
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::SelectDecoder(CdmContext* cdm_context) {
  decoder_selector_->SelectDecoder(
      &traits_, stream_, cdm_context,
      base::Bind(&DecoderStream<StreamType>::OnDecoderSelected,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&DecoderStream<StreamType>::OnDecodeOutputReady,
                 fallback_weak_factory_.GetWeakPtr()),
      waiting_for_decryption_key_cb_);
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::OnDecoderSelected(
    std::unique_ptr<Decoder> selected_decoder,
    std::unique_ptr<DecryptingDemuxerStream> decrypting_demuxer_stream) {
  FUNCTION_DVLOG(2) << ": "
                    << (selected_decoder ? selected_decoder->GetDisplayName()
                                         : "No decoder selected.");
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == STATE_INITIALIZING || state_ == STATE_REINITIALIZING_DECODER)
      << state_;
  if (state_ == STATE_INITIALIZING) {
    DCHECK(!init_cb_.is_null());
    DCHECK(read_cb_.is_null());
    DCHECK(reset_cb_.is_null());
  } else if (state_ == STATE_REINITIALIZING_DECODER) {
    DCHECK(decoder_);
  }

  previous_decoder_ = std::move(decoder_);
  decoded_frames_since_fallback_ = 0;
  decoder_ = std::move(selected_decoder);
  if (decrypting_demuxer_stream) {
    decrypting_demuxer_stream_ = std::move(decrypting_demuxer_stream);
    stream_ = decrypting_demuxer_stream_.get();
  }

  // TODO(tguilbert): crbug.com/603713 support config changes on decoder reinit.
  if (received_config_change_during_reinit_) {
    CompleteDecoderReinitialization(false);
    return;
  }

  // Attempt to decode buffers from previous decoders (when those decoders have
  // never successfully outputed a frame).
  fallback_buffers_ = pending_buffers_;

  if (!decoder_) {
    if (state_ == STATE_INITIALIZING) {
      state_ = STATE_UNINITIALIZED;
      MEDIA_LOG(ERROR, media_log_) << GetStreamTypeString()
                                   << " decoder initialization failed";
      base::ResetAndReturn(&init_cb_).Run(false);
    } else {
      CompleteDecoderReinitialization(false);
    }
    return;
  }

  media_log_->SetBooleanProperty(GetStreamTypeString() + "_dds",
                                 !!decrypting_demuxer_stream_);
  media_log_->SetStringProperty(GetStreamTypeString() + "_decoder",
                                decoder_->GetDisplayName());

  if (state_ == STATE_REINITIALIZING_DECODER) {
    CompleteDecoderReinitialization(true);
    return;
  }

  // Initialization succeeded.
  state_ = STATE_NORMAL;
  if (StreamTraits::NeedsBitstreamConversion(decoder_.get()))
    stream_->EnableBitstreamConverter();
  base::ResetAndReturn(&init_cb_).Run(true);
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::SatisfyRead(
    Status status,
    const scoped_refptr<Output>& output) {
  DCHECK(!read_cb_.is_null());
  base::ResetAndReturn(&read_cb_).Run(status, output);
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::Decode(
    const scoped_refptr<DecoderBuffer>& buffer) {
  FUNCTION_DVLOG(2);

  // We don't know if the decoder will error out on first decode yet. Save the
  // buffer to feed it to the fallback decoder later if needed.
  if (!decoded_frames_since_fallback_)
    pending_buffers_.push_back(buffer);

  // It's possible for a buffer to arrive from the demuxer right after the
  // fallback decoder successfully completed its initialization. At this point
  // |pending_buffers_| has already been copied to |fallback_buffers_| and we
  // need to append it ourselves.
  if (!fallback_buffers_.empty()) {
    fallback_buffers_.push_back(buffer);

    scoped_refptr<DecoderBuffer> temp = fallback_buffers_.front();
    fallback_buffers_.pop_front();
    DecodeInternal(temp);
  } else {
    DecodeInternal(buffer);
  }
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::DecodeInternal(
    const scoped_refptr<DecoderBuffer>& buffer) {
  FUNCTION_DVLOG(2);
  DCHECK(state_ == STATE_NORMAL || state_ == STATE_FLUSHING_DECODER) << state_;
  DCHECK_LT(pending_decode_requests_, GetMaxDecodeRequests());
  DCHECK(reset_cb_.is_null());
  DCHECK(buffer.get());

  traits_.OnDecode(buffer);

  int buffer_size = buffer->end_of_stream() ? 0 : buffer->data_size();

  TRACE_EVENT_ASYNC_BEGIN2(
      "media", GetTraceString<StreamType>(), this, "key frame",
      !buffer->end_of_stream() && buffer->is_key_frame(), "timestamp (ms)",
      !buffer->end_of_stream() ? buffer->timestamp().InMilliseconds() : 0);

  if (buffer->end_of_stream())
    decoding_eos_ = true;
  else if (buffer->duration() != kNoTimestamp())
    duration_tracker_.AddSample(buffer->duration());

  ++pending_decode_requests_;
  decoder_->Decode(buffer, base::Bind(&DecoderStream<StreamType>::OnDecodeDone,
                                      fallback_weak_factory_.GetWeakPtr(),
                                      buffer_size, buffer->end_of_stream()));
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::FlushDecoder() {
  // Send the EOS directly to the decoder, bypassing a potential add to
  // |pending_buffers_|.
  DecodeInternal(DecoderBuffer::CreateEOSBuffer());
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::OnDecodeDone(int buffer_size,
                                             bool end_of_stream,
                                             DecodeStatus status) {
  FUNCTION_DVLOG(2) << ": " << status;
  DCHECK(state_ == STATE_NORMAL || state_ == STATE_FLUSHING_DECODER ||
         state_ == STATE_ERROR)
      << state_;
  DCHECK_GT(pending_decode_requests_, 0);

  --pending_decode_requests_;

  TRACE_EVENT_ASYNC_END0("media", GetTraceString<StreamType>(), this);

  if (end_of_stream) {
    DCHECK(!pending_decode_requests_);
    decoding_eos_ = false;
  }

  if (state_ == STATE_ERROR) {
    DCHECK(read_cb_.is_null());
    return;
  }

  // Drop decoding result if Reset() was called during decoding.
  // The resetting process will be handled when the decoder is reset.
  if (!reset_cb_.is_null())
    return;

  switch (status) {
    case DecodeStatus::DECODE_ERROR:
      if (!decoded_frames_since_fallback_) {
        pending_decode_requests_ = 0;

        // Prevent all pending decode requests and outputs form those requests
        // from being called back.
        fallback_weak_factory_.InvalidateWeakPtrs();

        state_ = STATE_REINITIALIZING_DECODER;
        decoder_selector_->SelectDecoder(
            &traits_, stream_, nullptr,
            base::Bind(&DecoderStream<StreamType>::OnDecoderSelected,
                       weak_factory_.GetWeakPtr()),
            base::Bind(&DecoderStream<StreamType>::OnDecodeOutputReady,
                       fallback_weak_factory_.GetWeakPtr()),
            waiting_for_decryption_key_cb_);
        return;
      }
      state_ = STATE_ERROR;
      MEDIA_LOG(ERROR, media_log_) << GetStreamTypeString() << " decode error";
      ready_outputs_.clear();
      if (!read_cb_.is_null())
        SatisfyRead(DECODE_ERROR, NULL);
      return;

    case DecodeStatus::ABORTED:
      // Decoder can return DecodeStatus::ABORTED during Reset() or during
      // destruction.
      return;

    case DecodeStatus::OK:
      // Any successful decode counts!
      if (buffer_size > 0)
        StreamTraits::ReportStatistics(statistics_cb_, buffer_size);

      if (state_ == STATE_NORMAL) {
        if (end_of_stream) {
          state_ = STATE_END_OF_STREAM;
          if (ready_outputs_.empty() && !read_cb_.is_null())
            SatisfyRead(OK, StreamTraits::CreateEOSOutput());
          return;
        }

        if (CanDecodeMore())
          ReadFromDemuxerStream();
        return;
      }

      if (state_ == STATE_FLUSHING_DECODER && !pending_decode_requests_)
        ReinitializeDecoder();
      return;
  }
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::OnDecodeOutputReady(
    const scoped_refptr<Output>& output) {
  FUNCTION_DVLOG(2) << ": " << output->timestamp().InMilliseconds() << " ms";
  DCHECK(output.get());
  DCHECK(state_ == STATE_NORMAL || state_ == STATE_FLUSHING_DECODER ||
         state_ == STATE_ERROR)
      << state_;

  if (state_ == STATE_ERROR) {
    DCHECK(read_cb_.is_null());
    return;
  }

  // Drop decoding result if Reset() was called during decoding.
  // The resetting process will be handled when the decoder is reset.
  if (!reset_cb_.is_null())
    return;

  traits_.OnDecodeDone(output);

  ++decoded_frames_since_fallback_;

  // |decoder_| sucessfully decoded a frame. No need to keep buffers for a
  // fallback decoder.
  // Note: |fallback_buffers_| might still have buffers, and we will keep
  // reading from there before requesting new buffers from |stream_|.
  pending_buffers_.clear();

  if (!read_cb_.is_null()) {
    // If |ready_outputs_| was non-empty, the read would have already been
    // satisifed by Read().
    DCHECK(ready_outputs_.empty());
    SatisfyRead(OK, output);
    return;
  }

  // Store decoded output.
  ready_outputs_.push_back(output);

  // Destruct any previous decoder once we've decoded enough frames to ensure
  // that it's no longer in use.
  if (previous_decoder_ &&
      decoded_frames_since_fallback_ > limits::kMaxVideoFrames) {
    previous_decoder_.reset();
  }
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::ReadFromDemuxerStream() {
  FUNCTION_DVLOG(2);
  DCHECK_EQ(state_, STATE_NORMAL);
  DCHECK(CanDecodeMore());
  DCHECK(reset_cb_.is_null());

  if (!fallback_buffers_.empty()) {
    scoped_refptr<DecoderBuffer> buffer = fallback_buffers_.front();
    fallback_buffers_.pop_front();

    // Decode the buffer without re-appending it to |pending_buffers_|.
    DecodeInternal(buffer);
    return;
  }

  // We may get here when a read is already pending, ignore this.
  if (pending_demuxer_read_)
    return;

  pending_demuxer_read_ = true;
  stream_->Read(base::Bind(&DecoderStream<StreamType>::OnBufferReady,
                           weak_factory_.GetWeakPtr()));
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::OnBufferReady(
    DemuxerStream::Status status,
    const scoped_refptr<DecoderBuffer>& buffer) {
  FUNCTION_DVLOG(2) << ": " << status << ", "
                    << (buffer.get() ? buffer->AsHumanReadableString()
                                     : "NULL");

  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(pending_demuxer_read_);
  if (decoded_frames_since_fallback_) {
    DCHECK(pending_demuxer_read_ || state_ == STATE_ERROR) << state_;
  } else {
    DCHECK(state_ == STATE_ERROR || state_ == STATE_REINITIALIZING_DECODER ||
           state_ == STATE_NORMAL)
        << state_;
  }
  DCHECK_EQ(buffer.get() != NULL, status == DemuxerStream::kOk) << status;
  pending_demuxer_read_ = false;

  // If parallel decode requests are supported, multiple read requests might
  // have been sent to the demuxer. The buffers might arrive while the decoder
  // is reinitializing after falling back on first decode error.
  if (state_ == STATE_REINITIALIZING_DECODER &&
      !decoded_frames_since_fallback_) {
    switch (status) {
      case DemuxerStream::kOk:
        // Save valid buffers to be consumed by the new decoder.
        // |pending_buffers_| is copied to |fallback_buffers| in
        // OnDecoderSelected().
        pending_buffers_.push_back(buffer);
        break;
      case DemuxerStream::kConfigChanged:
        // TODO(tguilbert): crbug.com/603713
        // |decoder_| might have a stale config by the time it is reinitialized.
        // Ideally, we would save the config from |stream_| and reinitialize the
        // decoder by playing back the sequence of buffers and config changes.
        received_config_change_during_reinit_ = true;
        pending_buffers_.clear();
        break;
      case DemuxerStream::kAborted:
        // |this| will read from the demuxer stream again in OnDecoderSelected()
        // and receive a kAborted then.
        pending_buffers_.clear();
        break;
    }
    return;
  }

  // Decoding has been stopped.
  if (state_ == STATE_ERROR) {
    DCHECK(read_cb_.is_null());

    if (!reset_cb_.is_null()) {
      // If we are using DecryptingDemuxerStream, we already called DDS::Reset()
      // which will continue the resetting process in its callback.
      if (!decrypting_demuxer_stream_)
        Reset(base::ResetAndReturn(&reset_cb_));
    }
    return;
  }

  state_ = STATE_NORMAL;

  if (status == DemuxerStream::kConfigChanged) {
    FUNCTION_DVLOG(2) << ": " << "ConfigChanged";
    DCHECK(stream_->SupportsConfigChanges());

    // Pending buffers might not match the reinitialized decoder's new config.
    //
    // Note: as part of crbug.com/603713, we should record the config in order
    // to play it back to the fallback decoder.
    //
    // Clearing the buffers is an acceptable workaround for the time being. It
    // assures us that we maintain a consistent state, at the cost of
    // potentially dropping some frames. Flushing the decoder will cause one of
    // the following outcomes:
    //   - The decoder outputs a valid frame during flushing (we no longer
    //   care about |pending_buffers_| and fallback scenarios).
    //   - The decoder returns a DECODE_ERROR via OnDecodeDone() without having
    //   outputted a frame (we fallback to a new decoder which will read
    //   straight from the demuxer, dropping some frames).
    //   - The decoder is flushed without returning a frame or without a
    //   DECODE_ERROR (we reinitialize the decoder as if a normal flush
    //   happened, and read straight from the demuxer, which could lead to some
    //   lost frames if we were to fallback then).
    pending_buffers_.clear();

    if (!config_change_observer_cb_.is_null())
      config_change_observer_cb_.Run();

    state_ = STATE_FLUSHING_DECODER;
    if (!reset_cb_.is_null()) {
      // If we are using DecryptingDemuxerStream, we already called DDS::Reset()
      // which will continue the resetting process in its callback.
      if (!decrypting_demuxer_stream_)
        Reset(base::ResetAndReturn(&reset_cb_));
      // Reinitialization will continue after Reset() is done.
    } else {
      FlushDecoder();
    }
    return;
  }

  if (!reset_cb_.is_null()) {
    // If we are using DecryptingDemuxerStream, we already called DDS::Reset()
    // which will continue the resetting process in its callback.
    if (!decrypting_demuxer_stream_)
      Reset(base::ResetAndReturn(&reset_cb_));
    return;
  }

  if (status == DemuxerStream::kAborted) {
    if (!read_cb_.is_null())
      SatisfyRead(DEMUXER_READ_ABORTED, NULL);
    return;
  }

  if (!splice_observer_cb_.is_null() && !buffer->end_of_stream()) {
    const bool has_splice_ts = buffer->splice_timestamp() != kNoTimestamp();
    if (active_splice_ || has_splice_ts) {
      splice_observer_cb_.Run(buffer->splice_timestamp());
      active_splice_ = has_splice_ts;
    }
  }

  DCHECK(status == DemuxerStream::kOk) << status;
  Decode(buffer);

  // Read more data if the decoder supports multiple parallel decoding requests.
  if (CanDecodeMore())
    ReadFromDemuxerStream();
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::ReinitializeDecoder() {
  FUNCTION_DVLOG(2);
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_FLUSHING_DECODER);
  DCHECK_EQ(pending_decode_requests_, 0);

  state_ = STATE_REINITIALIZING_DECODER;
  // Decoders should not need a new CDM during reinitialization.
  traits_.InitializeDecoder(
      decoder_.get(), stream_, nullptr,
      base::Bind(&DecoderStream<StreamType>::OnDecoderReinitialized,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&DecoderStream<StreamType>::OnDecodeOutputReady,
                 fallback_weak_factory_.GetWeakPtr()));
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::OnDecoderReinitialized(bool success) {
  FUNCTION_DVLOG(2);
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_REINITIALIZING_DECODER);

  // ReinitializeDecoder() can be called in two cases:
  // 1, Flushing decoder finished (see OnDecodeOutputReady()).
  // 2, Reset() was called during flushing decoder (see OnDecoderReset()).
  // Also, Reset() can be called during pending ReinitializeDecoder().
  // This function needs to handle them all!

  if (!success) {
    // Reinitialization failed. Try to fall back to one of the remaining
    // decoders. This will consume at least one decoder so doing it more than
    // once is safe.
    // For simplicity, don't attempt to fall back to a decrypting decoder.
    // Calling this with a null CdmContext ensures that one won't be selected.
    SelectDecoder(nullptr);
  } else {
    CompleteDecoderReinitialization(true);
  }
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::CompleteDecoderReinitialization(bool success) {
  FUNCTION_DVLOG(2);
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_REINITIALIZING_DECODER);

  state_ = success ? STATE_NORMAL : STATE_ERROR;

  if (!reset_cb_.is_null()) {
    base::ResetAndReturn(&reset_cb_).Run();
    return;
  }

  if (read_cb_.is_null())
    return;

  if (state_ == STATE_ERROR) {
    MEDIA_LOG(ERROR, media_log_) << GetStreamTypeString()
                                 << " decoder reinitialization failed";
    SatisfyRead(DECODE_ERROR, NULL);
    return;
  }

  ReadFromDemuxerStream();
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::ResetDecoder() {
  FUNCTION_DVLOG(2);
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == STATE_NORMAL || state_ == STATE_FLUSHING_DECODER ||
         state_ == STATE_ERROR || state_ == STATE_END_OF_STREAM) << state_;
  DCHECK(!reset_cb_.is_null());

  decoder_->Reset(base::Bind(&DecoderStream<StreamType>::OnDecoderReset,
                             weak_factory_.GetWeakPtr()));
}

template <DemuxerStream::Type StreamType>
void DecoderStream<StreamType>::OnDecoderReset() {
  FUNCTION_DVLOG(2);
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == STATE_NORMAL || state_ == STATE_FLUSHING_DECODER ||
         state_ == STATE_ERROR || state_ == STATE_END_OF_STREAM) << state_;
  // If Reset() was called during pending read, read callback should be fired
  // before the reset callback is fired.
  DCHECK(read_cb_.is_null());
  DCHECK(!reset_cb_.is_null());

  // Make sure we read directly from the demuxer after a reset.
  fallback_buffers_.clear();
  pending_buffers_.clear();

  if (state_ != STATE_FLUSHING_DECODER) {
    state_ = STATE_NORMAL;
    active_splice_ = false;
    base::ResetAndReturn(&reset_cb_).Run();
    return;
  }

  // The resetting process will be continued in OnDecoderReinitialized().
  ReinitializeDecoder();
}

template class DecoderStream<DemuxerStream::VIDEO>;
template class DecoderStream<DemuxerStream::AUDIO>;

}  // namespace media
