// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_decoder_job.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/debug/trace_event.h"
#include "base/message_loop/message_loop_proxy.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/buffers.h"

namespace media {

// Timeout value for media codec operations. Because the first
// DequeInputBuffer() can take about 150 milliseconds, use 250 milliseconds
// here. See http://b/9357571.
static const int kMediaCodecTimeoutInMilliseconds = 250;

MediaDecoderJob::MediaDecoderJob(
    const scoped_refptr<base::SingleThreadTaskRunner>& decoder_task_runner,
    const base::Closure& request_data_cb,
    const base::Closure& config_changed_cb)
    : ui_task_runner_(base::MessageLoopProxy::current()),
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
  eos_unit_.end_of_stream = true;
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
    OnDecodeCompleted(MEDIA_CODEC_STOPPED, kNoTimestamp(), kNoTimestamp());
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

bool MediaDecoderJob::Decode(
    base::TimeTicks start_time_ticks,
    base::TimeDelta start_presentation_timestamp,
    const DecoderCallback& callback) {
  DCHECK(decode_cb_.is_null());
  DCHECK(data_received_cb_.is_null());
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  if (!media_codec_bridge_ || need_to_reconfig_decoder_job_) {
    need_to_reconfig_decoder_job_ = !CreateMediaCodecBridge();
    if (drain_decoder_) {
      // Decoder has been recreated, stop draining.
      drain_decoder_ = false;
      input_eos_encountered_ = false;
      output_eos_encountered_ = false;
      access_unit_index_[current_demuxer_data_index_]++;
    }
    skip_eos_enqueue_ = true;
    if (need_to_reconfig_decoder_job_)
      return false;
  }

  decode_cb_ = callback;

  if (!HasData()) {
    RequestData(base::Bind(&MediaDecoderJob::DecodeCurrentAccessUnit,
                           base::Unretained(this),
                           start_time_ticks,
                           start_presentation_timestamp));
    return true;
  }

  DecodeCurrentAccessUnit(start_time_ticks, start_presentation_timestamp);
  return true;
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

bool MediaDecoderJob::SetDemuxerConfigs(const DemuxerConfigs& configs) {
  bool config_changed = AreDemuxerConfigsChanged(configs);
  if (config_changed)
    UpdateDemuxerConfigs(configs);
  return config_changed;
}

base::android::ScopedJavaLocalRef<jobject> MediaDecoderJob::GetMediaCrypto() {
  base::android::ScopedJavaLocalRef<jobject> media_crypto;
  if (drm_bridge_)
    media_crypto = drm_bridge_->GetMediaCrypto();
  return media_crypto;
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
  if (unit.end_of_stream || unit.data.empty()) {
    media_codec_bridge_->QueueEOS(input_buf_index);
    return MEDIA_CODEC_INPUT_END_OF_STREAM;
  }

  if (unit.key_id.empty() || unit.iv.empty()) {
    DCHECK(unit.iv.empty() || !unit.key_id.empty());
    return media_codec_bridge_->QueueInputBuffer(
        input_buf_index, &unit.data[0], unit.data.size(), unit.timestamp);
  }

  MediaCodecStatus status = media_codec_bridge_->QueueSecureInputBuffer(
      input_buf_index,
      &unit.data[0], unit.data.size(),
      reinterpret_cast<const uint8*>(&unit.key_id[0]), unit.key_id.size(),
      reinterpret_cast<const uint8*>(&unit.iv[0]), unit.iv.size(),
      unit.subsamples.empty() ? NULL : &unit.subsamples[0],
      unit.subsamples.size(),
      unit.timestamp);

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
    // TODO(qinmin): |config_changed_cb_| should be run after draining finishes.
    // http://crbug.com/381975.
    if (SetDemuxerConfigs(configs))
      config_changed_cb_.Run();
    if (!drain_decoder_) {
      // If we haven't decoded any data yet, just skip the current access unit
      // and request the MediaCodec to be recreated on next Decode().
      if (skip_eos_enqueue_ || !reconfigure_needed) {
        need_to_reconfig_decoder_job_ =
            need_to_reconfig_decoder_job_ || reconfigure_needed;
        ui_task_runner_->PostTask(FROM_HERE, base::Bind(
            &MediaDecoderJob::OnDecodeCompleted, base::Unretained(this),
            MEDIA_CODEC_OUTPUT_FORMAT_CHANGED, kNoTimestamp(), kNoTimestamp()));
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
    MediaCodecStatus reset_status = media_codec_bridge_->Reset();
    if (MEDIA_CODEC_OK != reset_status) {
      callback.Run(reset_status, kNoTimestamp(), kNoTimestamp());
      return;
    }
  }

  // Once output EOS has occurred, we should not be asked to decode again.
  // MediaCodec has undefined behavior if similarly asked to decode after output
  // EOS.
  DCHECK(!output_eos_encountered_);

  // For aborted access unit, just skip it and inform the player.
  if (unit.status == DemuxerStream::kAborted) {
    // TODO(qinmin): use a new enum instead of MEDIA_CODEC_STOPPED.
    callback.Run(MEDIA_CODEC_STOPPED, kNoTimestamp(), kNoTimestamp());
    return;
  }

  if (skip_eos_enqueue_) {
    if (unit.end_of_stream || unit.data.empty()) {
      input_eos_encountered_ = true;
      output_eos_encountered_ = true;
      callback.Run(MEDIA_CODEC_OUTPUT_END_OF_STREAM, kNoTimestamp(),
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
    } else if (input_status != MEDIA_CODEC_OK) {
      callback.Run(input_status, kNoTimestamp(), kNoTimestamp());
      return;
    }
  }

  int buffer_index = 0;
  size_t offset = 0;
  size_t size = 0;
  base::TimeDelta presentation_timestamp;

  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(
      kMediaCodecTimeoutInMilliseconds);

  MediaCodecStatus status =
      media_codec_bridge_->DequeueOutputBuffer(timeout,
                                               &buffer_index,
                                               &offset,
                                               &size,
                                               &presentation_timestamp,
                                               &output_eos_encountered_,
                                               NULL);

  if (status != MEDIA_CODEC_OK) {
    if (status == MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED &&
        !media_codec_bridge_->GetOutputBuffers()) {
      status = MEDIA_CODEC_ERROR;
    }
    callback.Run(status, kNoTimestamp(), kNoTimestamp());
    return;
  }

  // TODO(xhwang/qinmin): This logic is correct but strange. Clean it up.
  if (output_eos_encountered_)
    status = MEDIA_CODEC_OUTPUT_END_OF_STREAM;

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
        FROM_HERE,
        base::Bind(&MediaDecoderJob::ReleaseOutputBuffer,
                   base::Unretained(this),
                   buffer_index,
                   size,
                   render_output,
                   presentation_timestamp,
                   base::Bind(callback, status)),
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
  ReleaseOutputCompletionCallback completion_callback = base::Bind(
      callback, status);
  ReleaseOutputBuffer(buffer_index, size, render_output, presentation_timestamp,
                      completion_callback);
}

void MediaDecoderJob::OnDecodeCompleted(
    MediaCodecStatus status, base::TimeDelta current_presentation_timestamp,
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
  if (current_presentation_timestamp != kNoTimestamp())
    prerolling_ = false;

  switch (status) {
    case MEDIA_CODEC_OK:
    case MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER:
    case MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED:
    case MEDIA_CODEC_OUTPUT_FORMAT_CHANGED:
    case MEDIA_CODEC_OUTPUT_END_OF_STREAM:
      if (!input_eos_encountered_) {
        CurrentDataConsumed(
            CurrentAccessUnit().status == DemuxerStream::kConfigChanged);
        access_unit_index_[current_demuxer_data_index_]++;
      }
      break;

    case MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER:
    case MEDIA_CODEC_INPUT_END_OF_STREAM:
    case MEDIA_CODEC_NO_KEY:
    case MEDIA_CODEC_STOPPED:
    case MEDIA_CODEC_ERROR:
      // Do nothing.
      break;
  };

  if (status == MEDIA_CODEC_OUTPUT_END_OF_STREAM && drain_decoder_) {
    OnDecoderDrained();
    status = MEDIA_CODEC_OK;
  }

  if (release_resources_pending_) {
    ReleaseMediaCodecBridge();
    release_resources_pending_ = false;
    if (drain_decoder_)
      OnDecoderDrained();
  }

  stop_decode_pending_ = false;
  base::ResetAndReturn(&decode_cb_).Run(
      status, current_presentation_timestamp, max_presentation_timestamp);
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
  const AccessUnit last_access_unit =
      received_data_[current_demuxer_data_index_].access_units.back();
  if (!last_access_unit.end_of_stream &&
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
  CurrentDataConsumed(true);
}

bool MediaDecoderJob::CreateMediaCodecBridge() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(decode_cb_.is_null());

  if (!HasStream()) {
    ReleaseMediaCodecBridge();
    return false;
  }

  // Create |media_codec_bridge_| only if config changes.
  if (media_codec_bridge_ && !need_to_reconfig_decoder_job_)
    return true;

  base::android::ScopedJavaLocalRef<jobject> media_crypto = GetMediaCrypto();
  if (is_content_encrypted_ && media_crypto.is_null())
    return false;

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

void MediaDecoderJob::ReleaseMediaCodecBridge() {
  if (!media_codec_bridge_)
    return;

  media_codec_bridge_.reset();
  OnMediaCodecBridgeReleased();
}

}  // namespace media
