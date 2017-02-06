// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_decoder_job.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/timestamp_constants.h"

namespace media {

// Timeout value for media codec operations. Because the first
// DequeInputBuffer() can take about 150 milliseconds, use 250 milliseconds
// here. See http://b/9357571.
static const int kMediaCodecTimeoutInMilliseconds = 250;

MediaDecoderJob::MediaDecoderJob(
    const scoped_refptr<base::SingleThreadTaskRunner>& decoder_task_runner,
    const base::Closure& request_data_cb,
    const base::Closure& config_changed_cb)
    : need_to_reconfig_decoder_job_(false),
      ui_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      decoder_task_runner_(decoder_task_runner),
      needs_flush_(false),
      input_eos_encountered_(false),
      output_eos_encountered_(false),
      skip_eos_enqueue_(true),
      prerolling_(true),
      request_data_cb_(request_data_cb),
      config_changed_cb_(config_changed_cb),
      current_demuxer_data_index_(0),
      input_buf_index_(-1),
      is_content_encrypted_(false),
      stop_decode_pending_(false),
      destroy_pending_(false),
      is_requesting_demuxer_data_(false),
      is_incoming_data_invalid_(false),
      release_resources_pending_(false),
      drm_bridge_(NULL),
      drain_decoder_(false) {
  InitializeReceivedData();
  eos_unit_.is_end_of_stream = true;
}

MediaDecoderJob::~MediaDecoderJob() {
  ReleaseMediaCodecBridge();
}

void MediaDecoderJob::OnDataReceived(const DemuxerData& data) {
  DVLOG(1) << __FUNCTION__ << ": " << data.access_units.size() << " units";
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(NoAccessUnitsRemainingInChunk(false));

  TRACE_EVENT_ASYNC_END2(
      "media", "MediaDecoderJob::RequestData", this,
      "Data type", data.type == media::DemuxerStream::AUDIO ? "AUDIO" : "VIDEO",
      "Units read", data.access_units.size());

  if (is_incoming_data_invalid_) {
    is_incoming_data_invalid_ = false;

    // If there is a pending callback, need to request the data again to get
    // valid data.
    if (!data_received_cb_.is_null())
      request_data_cb_.Run();
    else
      is_requesting_demuxer_data_ = false;
    return;
  }

  size_t next_demuxer_data_index = inactive_demuxer_data_index();
  received_data_[next_demuxer_data_index] = data;
  access_unit_index_[next_demuxer_data_index] = 0;
  is_requesting_demuxer_data_ = false;

  base::Closure done_cb = base::ResetAndReturn(&data_received_cb_);

  // If this data request is for the inactive chunk, or |data_received_cb_|
  // was set to null by Flush() or Release(), do nothing.
  if (done_cb.is_null())
    return;

  if (stop_decode_pending_) {
    DCHECK(is_decoding());
    OnDecodeCompleted(MEDIA_CODEC_ABORT, false, kNoTimestamp(), kNoTimestamp());
    return;
  }

  done_cb.Run();
}

void MediaDecoderJob::Prefetch(const base::Closure& prefetch_cb) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(data_received_cb_.is_null());
  DCHECK(decode_cb_.is_null());

  if (HasData()) {
    DVLOG(1) << __FUNCTION__ << " : using previously received data";
    ui_task_runner_->PostTask(FROM_HERE, prefetch_cb);
    return;
  }

  DVLOG(1) << __FUNCTION__ << " : requesting data";
  RequestData(prefetch_cb);
}

MediaDecoderJob::MediaDecoderJobStatus MediaDecoderJob::Decode(
    base::TimeTicks start_time_ticks,
    base::TimeDelta start_presentation_timestamp,
    const DecoderCallback& callback) {
  DCHECK(decode_cb_.is_null());
  DCHECK(data_received_cb_.is_null());
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  if (!media_codec_bridge_ || need_to_reconfig_decoder_job_) {
    if (drain_decoder_)
      OnDecoderDrained();
    MediaDecoderJobStatus status = CreateMediaCodecBridge();
    need_to_reconfig_decoder_job_ = (status != STATUS_SUCCESS);
    skip_eos_enqueue_ = true;
    if (need_to_reconfig_decoder_job_)
      return status;
  }

  decode_cb_ = callback;

  if (!HasData()) {
    RequestData(base::Bind(&MediaDecoderJob::DecodeCurrentAccessUnit,
                           base::Unretained(this),
                           start_time_ticks,
                           start_presentation_timestamp));
    return STATUS_SUCCESS;
  }

  DecodeCurrentAccessUnit(start_time_ticks, start_presentation_timestamp);
  return STATUS_SUCCESS;
}

void MediaDecoderJob::StopDecode() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(is_decoding());
  stop_decode_pending_ = true;
}

bool MediaDecoderJob::OutputEOSReached() const {
  return !drain_decoder_ && output_eos_encountered_;
}

void MediaDecoderJob::SetDrmBridge(MediaDrmBridge* drm_bridge) {
  drm_bridge_ = drm_bridge;
  need_to_reconfig_decoder_job_ = true;
}

void MediaDecoderJob::Flush() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(data_received_cb_.is_null());
  DCHECK(decode_cb_.is_null());

  // Clean up the received data.
  current_demuxer_data_index_ = 0;
  InitializeReceivedData();
  if (is_requesting_demuxer_data_)
    is_incoming_data_invalid_ = true;
  input_eos_encountered_ = false;
  output_eos_encountered_ = false;
  drain_decoder_ = false;

  // Do nothing, flush when the next Decode() happens.
  needs_flush_ = true;
}

void MediaDecoderJob::BeginPrerolling(base::TimeDelta preroll_timestamp) {
  DVLOG(1) << __FUNCTION__ << "(" << preroll_timestamp.InSecondsF() << ")";
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(!is_decoding());

  preroll_timestamp_ = preroll_timestamp;
  prerolling_ = true;
}

void MediaDecoderJob::ReleaseDecoderResources() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  if (decode_cb_.is_null()) {
    DCHECK(!drain_decoder_);
    // Since the decoder job is not decoding data, we can safely destroy
    // |media_codec_bridge_|.
    ReleaseMediaCodecBridge();
    return;
  }

  // Release |media_codec_bridge_| once decoding is completed.
  release_resources_pending_ = true;
}

jobject MediaDecoderJob::GetMediaCrypto() {
  return drm_bridge_ ? drm_bridge_->GetMediaCrypto() : nullptr;
}

bool MediaDecoderJob::SetCurrentFrameToPreviouslyCachedKeyFrame() {
  const std::vector<AccessUnit>& access_units =
      received_data_[current_demuxer_data_index_].access_units;
  // If the current data chunk is empty, the player must be in an initial or
  // seek state. The next access unit will always be a key frame.
  if (access_units.size() == 0)
    return true;

  // Find key frame in all the access units the decoder have decoded,
  // or is about to decode.
  int i = std::min(access_unit_index_[current_demuxer_data_index_],
                   access_units.size() - 1);
  for (; i >= 0; --i) {
    // Config change is always the last access unit, and it always come with
    // a key frame afterwards.
    if (access_units[i].status == DemuxerStream::kConfigChanged)
      return true;
    if (access_units[i].is_key_frame) {
      access_unit_index_[current_demuxer_data_index_] = i;
      return true;
    }
  }
  return false;
}


void MediaDecoderJob::Release() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __FUNCTION__;

  // If the decoder job is still decoding, we cannot delete the job immediately.
  destroy_pending_ = is_decoding();

  request_data_cb_.Reset();
  data_received_cb_.Reset();
  decode_cb_.Reset();

  if (destroy_pending_) {
    DVLOG(1) << __FUNCTION__ << " : delete is pending decode completion";
    return;
  }

  delete this;
}

MediaCodecStatus MediaDecoderJob::QueueInputBuffer(const AccessUnit& unit) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("media", __FUNCTION__);

  int input_buf_index = input_buf_index_;
  input_buf_index_ = -1;

  // TODO(xhwang): Hide DequeueInputBuffer() and the index in MediaCodecBridge.
  if (input_buf_index == -1) {
    base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(
        kMediaCodecTimeoutInMilliseconds);
    MediaCodecStatus status =
        media_codec_bridge_->DequeueInputBuffer(timeout, &input_buf_index);
    if (status != MEDIA_CODEC_OK) {
      DVLOG(1) << "DequeueInputBuffer fails: " << status;
      return status;
    }
  }

  // TODO(qinmin): skip frames if video is falling far behind.
  DCHECK_GE(input_buf_index, 0);
  if (unit.is_end_of_stream || unit.data.empty()) {
    media_codec_bridge_->QueueEOS(input_buf_index);
    return MEDIA_CODEC_INPUT_END_OF_STREAM;
  }

  if (unit.key_id.empty() || unit.iv.empty()) {
    DCHECK(unit.iv.empty() || !unit.key_id.empty());
    return media_codec_bridge_->QueueInputBuffer(
        input_buf_index, &unit.data[0], unit.data.size(), unit.timestamp);
  }

  MediaCodecStatus status = media_codec_bridge_->QueueSecureInputBuffer(
      input_buf_index, &unit.data[0], unit.data.size(), unit.key_id, unit.iv,
      unit.subsamples.empty() ? NULL : &unit.subsamples[0],
      unit.subsamples.size(), unit.timestamp);

  // In case of MEDIA_CODEC_NO_KEY, we must reuse the |input_buf_index_|.
  // Otherwise MediaDrm will report errors.
  if (status == MEDIA_CODEC_NO_KEY)
    input_buf_index_ = input_buf_index;

  return status;
}

bool MediaDecoderJob::HasData() const {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  // When |input_eos_encountered_| is set, |access_unit_index_| and
  // |current_demuxer_data_index_| must be pointing to an EOS unit,
  // or a |kConfigChanged| unit if |drain_decoder_| is true. In both cases,
  // we'll feed an EOS input unit to drain the decoder until we hit output EOS.
  DCHECK(!input_eos_encountered_ || !NoAccessUnitsRemainingInChunk(true));
  return !NoAccessUnitsRemainingInChunk(true) ||
      !NoAccessUnitsRemainingInChunk(false);
}

void MediaDecoderJob::RequestData(const base::Closure& done_cb) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(data_received_cb_.is_null());
  DCHECK(!input_eos_encountered_);
  DCHECK(NoAccessUnitsRemainingInChunk(false));

  TRACE_EVENT_ASYNC_BEGIN0("media", "MediaDecoderJob::RequestData", this);

  data_received_cb_ = done_cb;

  // If we are already expecting new data, just set the callback and do
  // nothing.
  if (is_requesting_demuxer_data_)
    return;

  // The new incoming data will be stored as the next demuxer data chunk, since
  // the decoder might still be decoding the current one.
  size_t next_demuxer_data_index = inactive_demuxer_data_index();
  received_data_[next_demuxer_data_index] = DemuxerData();
  access_unit_index_[next_demuxer_data_index] = 0;
  is_requesting_demuxer_data_ = true;

  request_data_cb_.Run();
}

void MediaDecoderJob::DecodeCurrentAccessUnit(
    base::TimeTicks start_time_ticks,
    base::TimeDelta start_presentation_timestamp) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(!decode_cb_.is_null());

  RequestCurrentChunkIfEmpty();
  const AccessUnit& access_unit = CurrentAccessUnit();
  if (CurrentAccessUnit().status == DemuxerStream::kConfigChanged) {
    int index = CurrentReceivedDataChunkIndex();
    const DemuxerConfigs& configs = received_data_[index].demuxer_configs[0];
    bool reconfigure_needed = IsCodecReconfigureNeeded(configs);
    SetDemuxerConfigs(configs);
    if (!drain_decoder_) {
      // If we haven't decoded any data yet, just skip the current access unit
      // and request the MediaCodec to be recreated on next Decode().
      if (skip_eos_enqueue_ || !reconfigure_needed) {
        need_to_reconfig_decoder_job_ =
            need_to_reconfig_decoder_job_ || reconfigure_needed;
        // Report MEDIA_CODEC_OK status so decoder will continue decoding and
        // MEDIA_CODEC_OUTPUT_FORMAT_CHANGED status will come later.
        ui_task_runner_->PostTask(FROM_HERE, base::Bind(
            &MediaDecoderJob::OnDecodeCompleted, base::Unretained(this),
            MEDIA_CODEC_OK, false, kNoTimestamp(), kNoTimestamp()));
        return;
      }
      // Start draining the decoder so that all the remaining frames are
      // rendered.
      drain_decoder_ = true;
    }
  }

  DCHECK(!(needs_flush_ && drain_decoder_));
  decoder_task_runner_->PostTask(FROM_HERE, base::Bind(
      &MediaDecoderJob::DecodeInternal, base::Unretained(this),
      drain_decoder_ ? eos_unit_ : access_unit,
      start_time_ticks, start_presentation_timestamp, needs_flush_,
      media::BindToCurrentLoop(base::Bind(
          &MediaDecoderJob::OnDecodeCompleted, base::Unretained(this)))));
  needs_flush_ = false;
}

void MediaDecoderJob::DecodeInternal(
    const AccessUnit& unit,
    base::TimeTicks start_time_ticks,
    base::TimeDelta start_presentation_timestamp,
    bool needs_flush,
    const MediaDecoderJob::DecoderCallback& callback) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("media", __FUNCTION__);

  if (needs_flush) {
    DVLOG(1) << "DecodeInternal needs flush.";
    input_eos_encountered_ = false;
    output_eos_encountered_ = false;
    input_buf_index_ = -1;
    MediaCodecStatus flush_status = media_codec_bridge_->Flush();
    if (flush_status != MEDIA_CODEC_OK) {
      callback.Run(flush_status, false, kNoTimestamp(), kNoTimestamp());
      return;
    }
  }

  // Once output EOS has occurred, we should not be asked to decode again.
  // MediaCodec has undefined behavior if similarly asked to decode after output
  // EOS.
  DCHECK(!output_eos_encountered_);

  // For aborted access unit, just skip it and inform the player.
  if (unit.status == DemuxerStream::kAborted) {
    callback.Run(MEDIA_CODEC_ABORT, false, kNoTimestamp(), kNoTimestamp());
    return;
  }

  if (skip_eos_enqueue_) {
    if (unit.is_end_of_stream || unit.data.empty()) {
      input_eos_encountered_ = true;
      output_eos_encountered_ = true;
      callback.Run(MEDIA_CODEC_OUTPUT_END_OF_STREAM, false, kNoTimestamp(),
                   kNoTimestamp());
      return;
    }

    skip_eos_enqueue_ = false;
  }

  MediaCodecStatus input_status = MEDIA_CODEC_INPUT_END_OF_STREAM;
  if (!input_eos_encountered_) {
    input_status = QueueInputBuffer(unit);
    if (input_status == MEDIA_CODEC_INPUT_END_OF_STREAM) {
      input_eos_encountered_ = true;
    } else if (input_status == MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER) {
      // In some cases, all buffers must be released to codec before format
      // change can be resolved. Context: b/21786703
      DVLOG(1) << "dequeueInputBuffer gave AGAIN_LATER, dequeue output buffers";
    } else if (input_status != MEDIA_CODEC_OK) {
      callback.Run(input_status, false, kNoTimestamp(), kNoTimestamp());
      return;
    }
  }

  int buffer_index = 0;
  size_t offset = 0;
  size_t size = 0;
  base::TimeDelta presentation_timestamp;

  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(
      kMediaCodecTimeoutInMilliseconds);

  MediaCodecStatus status = MEDIA_CODEC_OK;
  bool has_format_change = false;
  // Dequeue the output buffer until a MEDIA_CODEC_OK, MEDIA_CODEC_ERROR or
  // MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER is received.
  do {
    status = media_codec_bridge_->DequeueOutputBuffer(
        timeout,
        &buffer_index,
        &offset,
        &size,
        &presentation_timestamp,
        &output_eos_encountered_,
        NULL);
    if (status == MEDIA_CODEC_OUTPUT_FORMAT_CHANGED) {
      // TODO(qinmin): instead of waiting for the next output buffer to be
      // dequeued, post a task on the UI thread to signal the format change.
      if (OnOutputFormatChanged())
        has_format_change = true;
      else
        status = MEDIA_CODEC_ERROR;
    }
  } while (status != MEDIA_CODEC_OK && status != MEDIA_CODEC_ERROR &&
           status != MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER);

  if (status != MEDIA_CODEC_OK) {
    callback.Run(status, false, kNoTimestamp(), kNoTimestamp());
    return;
  }

  // TODO(xhwang/qinmin): This logic is correct but strange. Clean it up.
  if (output_eos_encountered_)
    status = MEDIA_CODEC_OUTPUT_END_OF_STREAM;
  else if (has_format_change)
    status = MEDIA_CODEC_OUTPUT_FORMAT_CHANGED;

  bool render_output  = presentation_timestamp >= preroll_timestamp_ &&
      (status != MEDIA_CODEC_OUTPUT_END_OF_STREAM || size != 0u);
  base::TimeDelta time_to_render;
  DCHECK(!start_time_ticks.is_null());
  if (render_output && ComputeTimeToRender()) {
    time_to_render = presentation_timestamp - (base::TimeTicks::Now() -
        start_time_ticks + start_presentation_timestamp);
  }

  if (time_to_render > base::TimeDelta()) {
    decoder_task_runner_->PostDelayedTask(
        FROM_HERE, base::Bind(&MediaDecoderJob::ReleaseOutputBuffer,
                              base::Unretained(this), buffer_index, offset,
                              size, render_output,
                              false,  // this is not a late frame
                              presentation_timestamp, status, callback),
        time_to_render);
    return;
  }

  // TODO(qinmin): The codec is lagging behind, need to recalculate the
  // |start_presentation_timestamp_| and |start_time_ticks_| in
  // media_source_player.cc.
  DVLOG(1) << "codec is lagging behind :" << time_to_render.InMicroseconds();
  if (render_output) {
    // The player won't expect a timestamp smaller than the
    // |start_presentation_timestamp|. However, this could happen due to decoder
    // errors.
    presentation_timestamp = std::max(
        presentation_timestamp, start_presentation_timestamp);
  } else {
    presentation_timestamp = kNoTimestamp();
  }

  const bool is_late_frame = (time_to_render < base::TimeDelta());
  ReleaseOutputBuffer(buffer_index, offset, size, render_output, is_late_frame,
                      presentation_timestamp, status, callback);
}

void MediaDecoderJob::OnDecodeCompleted(
    MediaCodecStatus status,
    bool is_late_frame,
    base::TimeDelta current_presentation_timestamp,
    base::TimeDelta max_presentation_timestamp) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  if (destroy_pending_) {
    DVLOG(1) << __FUNCTION__ << " : completing pending deletion";
    delete this;
    return;
  }

  if (status == MEDIA_CODEC_OUTPUT_END_OF_STREAM)
    output_eos_encountered_ = true;

  DCHECK(!decode_cb_.is_null());

  // If output was queued for rendering, then we have completed prerolling.
  if (current_presentation_timestamp != kNoTimestamp() ||
      status == MEDIA_CODEC_OUTPUT_END_OF_STREAM) {
    prerolling_ = false;
  }

  switch (status) {
    case MEDIA_CODEC_OK:
    case MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER:
    case MEDIA_CODEC_OUTPUT_FORMAT_CHANGED:
    case MEDIA_CODEC_OUTPUT_END_OF_STREAM:
      if (!input_eos_encountered_)
        access_unit_index_[current_demuxer_data_index_]++;
      break;

    case MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER:
    case MEDIA_CODEC_INPUT_END_OF_STREAM:
    case MEDIA_CODEC_NO_KEY:
    case MEDIA_CODEC_ABORT:
    case MEDIA_CODEC_ERROR:
      // Do nothing.
      break;

    case MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED:
      DCHECK(false) << "Invalid output status";
      break;
  };

  if (status == MEDIA_CODEC_OUTPUT_END_OF_STREAM && drain_decoder_) {
    OnDecoderDrained();
    status = MEDIA_CODEC_OK;
  }

  if (status == MEDIA_CODEC_OUTPUT_FORMAT_CHANGED) {
    if (UpdateOutputFormat())
      config_changed_cb_.Run();
    status = MEDIA_CODEC_OK;
  }

  if (release_resources_pending_) {
    ReleaseMediaCodecBridge();
    release_resources_pending_ = false;
    if (drain_decoder_)
      OnDecoderDrained();
  }

  stop_decode_pending_ = false;
  base::ResetAndReturn(&decode_cb_)
      .Run(status, is_late_frame, current_presentation_timestamp,
           max_presentation_timestamp);
}

const AccessUnit& MediaDecoderJob::CurrentAccessUnit() const {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(HasData());
  size_t index = CurrentReceivedDataChunkIndex();
  return received_data_[index].access_units[access_unit_index_[index]];
}

size_t MediaDecoderJob::CurrentReceivedDataChunkIndex() const {
  return NoAccessUnitsRemainingInChunk(true) ?
      inactive_demuxer_data_index() : current_demuxer_data_index_;
}

bool MediaDecoderJob::NoAccessUnitsRemainingInChunk(
    bool is_active_chunk) const {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  size_t index = is_active_chunk ? current_demuxer_data_index_ :
      inactive_demuxer_data_index();
  return received_data_[index].access_units.size() <= access_unit_index_[index];
}

void MediaDecoderJob::RequestCurrentChunkIfEmpty() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(HasData());
  if (!NoAccessUnitsRemainingInChunk(true))
    return;

  // Requests new data if the the last access unit of the next chunk is not EOS.
  current_demuxer_data_index_ = inactive_demuxer_data_index();
  const AccessUnit& last_access_unit =
      received_data_[current_demuxer_data_index_].access_units.back();
  if (!last_access_unit.is_end_of_stream &&
      last_access_unit.status != DemuxerStream::kAborted) {
    RequestData(base::Closure());
  }
}

void MediaDecoderJob::InitializeReceivedData() {
  for (size_t i = 0; i < 2; ++i) {
    received_data_[i] = DemuxerData();
    access_unit_index_[i] = 0;
  }
}

void MediaDecoderJob::OnDecoderDrained() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(drain_decoder_);

  input_eos_encountered_ = false;
  output_eos_encountered_ = false;
  drain_decoder_ = false;
  ReleaseMediaCodecBridge();
  // Increase the access unit index so that the new decoder will not handle
  // the config change again.
  access_unit_index_[current_demuxer_data_index_]++;
}

MediaDecoderJob::MediaDecoderJobStatus
    MediaDecoderJob::CreateMediaCodecBridge() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(decode_cb_.is_null());

  if (!HasStream()) {
    ReleaseMediaCodecBridge();
    return STATUS_FAILURE;
  }

  // Create |media_codec_bridge_| only if config changes.
  if (media_codec_bridge_ && !need_to_reconfig_decoder_job_)
    return STATUS_SUCCESS;

  if (is_content_encrypted_ && !GetMediaCrypto())
    return STATUS_FAILURE;

  ReleaseMediaCodecBridge();
  DVLOG(1) << __FUNCTION__ << " : creating new media codec bridge";

  return CreateMediaCodecBridgeInternal();
}

bool MediaDecoderJob::IsCodecReconfigureNeeded(
    const DemuxerConfigs& configs) const {
  if (!AreDemuxerConfigsChanged(configs))
    return false;
  return true;
}

bool MediaDecoderJob::OnOutputFormatChanged() {
  return true;
}

bool MediaDecoderJob::UpdateOutputFormat() {
  return false;
}

void MediaDecoderJob::ReleaseMediaCodecBridge() {
  if (!media_codec_bridge_)
    return;

  media_codec_bridge_.reset();
  input_buf_index_ = -1;
}

}  // namespace media
