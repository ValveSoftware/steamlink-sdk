// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_codec_decoder.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "media/base/android/media_codec_bridge.h"

namespace media {

namespace {

// Stop requesting new data in the kPrefetching state when the queue size
// reaches this limit.
const int kPrefetchLimit = 8;

// Request new data in the kRunning state if the queue size is less than this.
const int kPlaybackLowLimit = 4;

// Posting delay of the next frame processing, in milliseconds
const int kNextFrameDelay = 1;

// Timeout for dequeuing an input buffer from MediaCodec in milliseconds.
const int kInputBufferTimeout = 20;

// Timeout for dequeuing an output buffer from MediaCodec in milliseconds.
const int kOutputBufferTimeout = 20;

}  // namespace

MediaCodecDecoder::MediaCodecDecoder(
    const char* decoder_thread_name,
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    FrameStatistics* frame_statistics,
    const base::Closure& external_request_data_cb,
    const base::Closure& starvation_cb,
    const base::Closure& decoder_drained_cb,
    const base::Closure& stop_done_cb,
    const base::Closure& waiting_for_decryption_key_cb,
    const base::Closure& error_cb)
    : decoder_thread_(decoder_thread_name),
      media_task_runner_(media_task_runner),
      frame_statistics_(frame_statistics),
      needs_reconfigure_(false),
      drain_decoder_(false),
      always_reconfigure_for_tests_(false),
      external_request_data_cb_(external_request_data_cb),
      starvation_cb_(starvation_cb),
      decoder_drained_cb_(decoder_drained_cb),
      stop_done_cb_(stop_done_cb),
      waiting_for_decryption_key_cb_(waiting_for_decryption_key_cb),
      error_cb_(error_cb),
      state_(kStopped),
      pending_input_buf_index_(-1),
      is_prepared_(false),
      eos_enqueued_(false),
      missing_key_reported_(false),
      completed_(false),
      last_frame_posted_(false),
      is_data_request_in_progress_(false),
      is_incoming_data_invalid_(false),
#ifndef NDEBUG
      verify_next_frame_is_key_(false),
#endif
      weak_factory_(this) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << "Decoder::Decoder() " << decoder_thread_name;

  internal_error_cb_ =
      base::Bind(&MediaCodecDecoder::OnCodecError, weak_factory_.GetWeakPtr());
  internal_preroll_done_cb_ =
      base::Bind(&MediaCodecDecoder::OnPrerollDone, weak_factory_.GetWeakPtr());
  request_data_cb_ =
      base::Bind(&MediaCodecDecoder::RequestData, weak_factory_.GetWeakPtr());
}

MediaCodecDecoder::~MediaCodecDecoder() {}

const char* MediaCodecDecoder::class_name() const {
  return "Decoder";
}

void MediaCodecDecoder::Flush() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__;

  DCHECK_EQ(GetState(), kStopped);

  // Flush() is a part of the Seek request. Whenever we request a seek we need
  // to invalidate the current data request.
  if (is_data_request_in_progress_)
    is_incoming_data_invalid_ = true;

  eos_enqueued_ = false;
  missing_key_reported_ = false;
  completed_ = false;
  drain_decoder_ = false;
  au_queue_.Flush();

  // |is_prepared_| is set on the decoder thread, it shouldn't be running now.
  DCHECK(!decoder_thread_.IsRunning());
  is_prepared_ = false;

  pending_input_buf_index_ = -1;

#ifndef NDEBUG
  // We check and reset |verify_next_frame_is_key_| on Decoder thread.
  // We have just DCHECKed that decoder thread is not running.

  // For video the first frame after flush must be key frame.
  verify_next_frame_is_key_ = true;
#endif

  if (media_codec_bridge_) {
    MediaCodecStatus flush_status = media_codec_bridge_->Flush();
    if (flush_status != MEDIA_CODEC_OK) {
      DVLOG(0) << class_name() << "::" << __FUNCTION__
               << "MediaCodecBridge::Flush() failed";
      media_task_runner_->PostTask(FROM_HERE, internal_error_cb_);
    }
  }
}

void MediaCodecDecoder::ReleaseMediaCodec() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__;

  DCHECK(!decoder_thread_.IsRunning());

  media_codec_bridge_.reset();

  // |is_prepared_| is set on the decoder thread, it shouldn't be running now.
  is_prepared_ = false;

  pending_input_buf_index_ = -1;
}

bool MediaCodecDecoder::IsPrefetchingOrPlaying() const {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  // Whether decoder needs to be stopped.
  base::AutoLock lock(state_lock_);
  switch (state_) {
    case kPrefetching:
    case kPrefetched:
    case kPrerolling:
    case kPrerolled:
    case kRunning:
      return true;
    case kStopped:
    case kStopping:
    case kInEmergencyStop:
    case kError:
      return false;
  }
  NOTREACHED();
  return false;
}

bool MediaCodecDecoder::IsStopped() const {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  return GetState() == kStopped;
}

bool MediaCodecDecoder::IsCompleted() const {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  return completed_;
}

bool MediaCodecDecoder::NotCompletedAndNeedsPreroll() const {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  return HasStream() && !completed_ &&
         (!is_prepared_ || !preroll_timestamp_.is_zero());
}

void MediaCodecDecoder::SetPrerollTimestamp(base::TimeDelta preroll_timestamp) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << class_name() << "::" << __FUNCTION__ << ": " << preroll_timestamp;

  preroll_timestamp_ = preroll_timestamp;
}

void MediaCodecDecoder::SetNeedsReconfigure() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__;

  needs_reconfigure_ = true;
}

void MediaCodecDecoder::Prefetch(const base::Closure& prefetch_done_cb) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__;

  DCHECK(GetState() == kStopped);

  prefetch_done_cb_ = prefetch_done_cb;

  SetState(kPrefetching);
  PrefetchNextChunk();
}

MediaCodecDecoder::ConfigStatus MediaCodecDecoder::Configure(
    jobject media_crypto) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__;

  if (GetState() == kError) {
    DVLOG(0) << class_name() << "::" << __FUNCTION__ << ": wrong state kError";
    return kConfigFailure;
  }

  if (needs_reconfigure_) {
    DVLOG(1) << class_name() << "::" << __FUNCTION__
             << ": needs reconfigure, deleting MediaCodec";
    needs_reconfigure_ = false;
    ReleaseMediaCodec();
  }

  if (media_codec_bridge_) {
    DVLOG(1) << class_name() << "::" << __FUNCTION__
             << ": reconfiguration is not required, ignoring";
    return kConfigOk;
  }

  // Read all |kConfigChanged| units preceding the data one.
  AccessUnitQueue::Info au_info = au_queue_.GetInfo();
  while (au_info.configs) {
    SetDemuxerConfigs(*au_info.configs);
    au_queue_.Advance();
    au_info = au_queue_.GetInfo();
  }

  MediaCodecDecoder::ConfigStatus result = ConfigureInternal(media_crypto);

#ifndef NDEBUG
  // We check and reset |verify_next_frame_is_key_| on Decoder thread.
  // This DCHECK ensures we won't need to lock this variable.
  DCHECK(!decoder_thread_.IsRunning());

  // For video the first frame after reconfiguration must be key frame.
  if (result == kConfigOk)
    verify_next_frame_is_key_ = true;
#endif

  return result;
}

bool MediaCodecDecoder::Preroll(const base::Closure& preroll_done_cb) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__
           << " preroll_timestamp:" << preroll_timestamp_;

  DecoderState state = GetState();
  if (state != kPrefetched) {
    DVLOG(0) << class_name() << "::" << __FUNCTION__ << ": wrong state "
             << AsString(state) << ", ignoring";
    return false;
  }

  if (!media_codec_bridge_) {
    DVLOG(0) << class_name() << "::" << __FUNCTION__
             << ": not configured, ignoring";
    return false;
  }

  DCHECK(!decoder_thread_.IsRunning());

  preroll_done_cb_ = preroll_done_cb;

  // We only synchronize video stream.
  DissociatePTSFromTime();  // associaton will happen after preroll is done.

  last_frame_posted_ = false;
  missing_key_reported_ = false;

  // Start the decoder thread
  if (!decoder_thread_.Start()) {
    DVLOG(0) << class_name() << "::" << __FUNCTION__
             << ": cannot start decoder thread";
    return false;
  }

  SetState(kPrerolling);

  decoder_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&MediaCodecDecoder::ProcessNextFrame, base::Unretained(this)));

  return true;
}

bool MediaCodecDecoder::Start(base::TimeDelta start_timestamp) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__
           << " start_timestamp:" << start_timestamp;

  DecoderState state = GetState();

  if (state != kPrefetched && state != kPrerolled) {
    DVLOG(0) << class_name() << "::" << __FUNCTION__ << ": wrong state "
             << AsString(state) << ", ignoring";
    return false;
  }

  if (!media_codec_bridge_) {
    DVLOG(0) << class_name() << "::" << __FUNCTION__
             << ": not configured, ignoring";
    return false;
  }

  // We only synchronize video stream.
  AssociateCurrentTimeWithPTS(start_timestamp);

  DCHECK(preroll_timestamp_.is_zero());

  // Start the decoder thread
  if (!decoder_thread_.IsRunning()) {
    last_frame_posted_ = false;
    missing_key_reported_ = false;
    if (!decoder_thread_.Start()) {
      DVLOG(1) << class_name() << "::" << __FUNCTION__
               << ": cannot start decoder thread";
      return false;
    }
  }

  SetState(kRunning);

  decoder_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&MediaCodecDecoder::ProcessNextFrame, base::Unretained(this)));

  return true;
}

void MediaCodecDecoder::SyncStop() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__;

  if (GetState() == kError) {
    DVLOG(0) << class_name() << "::" << __FUNCTION__
             << ": wrong state kError, ignoring";
    return;
  }

  DoEmergencyStop();

  ReleaseDelayedBuffers();
}

void MediaCodecDecoder::RequestToStop() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__;

  DecoderState state = GetState();
  switch (state) {
    case kError:
      DVLOG(0) << class_name() << "::" << __FUNCTION__
               << ": wrong state kError, ignoring";
      break;
    case kRunning:
      SetState(kStopping);
      break;
    case kPrerolling:
    case kPrerolled:
      DCHECK(decoder_thread_.IsRunning());
      // Synchronous stop.
      decoder_thread_.Stop();
      SetState(kStopped);
      media_task_runner_->PostTask(FROM_HERE, stop_done_cb_);
      break;
    case kStopping:
    case kStopped:
      break;  // ignore
    case kPrefetching:
    case kPrefetched:
      // There is nothing to wait for, we can sent notification right away.
      DCHECK(!decoder_thread_.IsRunning());
      SetState(kStopped);
      media_task_runner_->PostTask(FROM_HERE, stop_done_cb_);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void MediaCodecDecoder::OnLastFrameRendered(bool eos_encountered) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__
           << " eos_encountered:" << eos_encountered;

  decoder_thread_.Stop();  // synchronous

  SetState(kStopped);
  completed_ = (eos_encountered && !drain_decoder_);

  missing_key_reported_ = false;

  // If the stream is completed during preroll we need to report it since
  // another stream might be running and the player waits for two callbacks.
  if (completed_ && !preroll_done_cb_.is_null()) {
    preroll_timestamp_ = base::TimeDelta();
    media_task_runner_->PostTask(FROM_HERE,
                                 base::ResetAndReturn(&preroll_done_cb_));
  }

  if (eos_encountered && drain_decoder_) {
    drain_decoder_ = false;
    eos_enqueued_ = false;
    ReleaseMediaCodec();
    media_task_runner_->PostTask(FROM_HERE, decoder_drained_cb_);
  }

  media_task_runner_->PostTask(FROM_HERE, stop_done_cb_);
}

void MediaCodecDecoder::OnPrerollDone() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__
           << " state:" << AsString(GetState());

  preroll_timestamp_ = base::TimeDelta();

  // The state might be kStopping (?)
  if (GetState() == kPrerolling)
    SetState(kPrerolled);

  if (!preroll_done_cb_.is_null())
    base::ResetAndReturn(&preroll_done_cb_).Run();
}

void MediaCodecDecoder::OnDemuxerDataAvailable(const DemuxerData& data) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  // If |data| contains an aborted data, the last AU will have kAborted status.
  bool aborted_data =
      !data.access_units.empty() &&
      data.access_units.back().status == DemuxerStream::kAborted;

#ifndef NDEBUG
  const char* explain_if_skipped =
      is_incoming_data_invalid_ ? " skipped as invalid"
                                : (aborted_data ? " skipped as aborted" : "");

  for (const auto& unit : data.access_units)
    DVLOG(2) << class_name() << "::" << __FUNCTION__ << explain_if_skipped
             << " au: " << unit;
  for (const auto& configs : data.demuxer_configs)
    DVLOG(2) << class_name() << "::" << __FUNCTION__ << " configs: " << configs;
#endif

  if (!is_incoming_data_invalid_ && !aborted_data)
    au_queue_.PushBack(data);

  is_incoming_data_invalid_ = false;
  is_data_request_in_progress_ = false;

  // Do not request data if we got kAborted. There is no point to request the
  // data after kAborted and before the OnDemuxerSeekDone.
  if (GetState() == kPrefetching && !aborted_data)
    PrefetchNextChunk();
}

bool MediaCodecDecoder::IsPrerollingForTests() const {
  // UI task runner.
  return GetState() == kPrerolling;
}

void MediaCodecDecoder::SetAlwaysReconfigureForTests() {
  // UI task runner.
  always_reconfigure_for_tests_ = true;
}

void MediaCodecDecoder::SetCodecCreatedCallbackForTests(base::Closure cb) {
  // UI task runner.
  codec_created_for_tests_cb_ = cb;
}

int MediaCodecDecoder::NumDelayedRenderTasks() const {
  return 0;
}

void MediaCodecDecoder::DoEmergencyStop() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << class_name() << "::" << __FUNCTION__;

  // After this method returns, decoder thread will not be running.

  // Set [kInEmergencyStop| state to block already posted ProcessNextFrame().
  SetState(kInEmergencyStop);

  decoder_thread_.Stop();  // synchronous

  SetState(kStopped);

  missing_key_reported_ = false;
}

void MediaCodecDecoder::CheckLastFrame(bool eos_encountered,
                                       bool has_delayed_tasks) {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  bool last_frame_when_stopping = GetState() == kStopping && !has_delayed_tasks;

  if (last_frame_when_stopping || eos_encountered) {
    media_task_runner_->PostTask(
        FROM_HERE, base::Bind(&MediaCodecDecoder::OnLastFrameRendered,
                              weak_factory_.GetWeakPtr(), eos_encountered));
    last_frame_posted_ = true;
  }
}

void MediaCodecDecoder::OnCodecError() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  // Ignore codec errors from the moment surface is changed till the
  // |media_codec_bridge_| is deleted.
  if (needs_reconfigure_) {
    DVLOG(1) << class_name() << "::" << __FUNCTION__
             << ": needs reconfigure, ignoring";
    return;
  }

  SetState(kError);
  error_cb_.Run();
}

void MediaCodecDecoder::RequestData() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  // We request data only in kPrefetching, kPrerolling and kRunning states.
  // For kPrerolling and kRunning this method is posted from Decoder thread,
  // and by the time it arrives the player might be doing something else, e.g.
  // seeking, in which case we should not request more data
  switch (GetState()) {
    case kPrefetching:
    case kPrerolling:
    case kRunning:
      break;  // continue
    default:
      return;  // skip
  }

  // Ensure one data request at a time.
  if (!is_data_request_in_progress_) {
    is_data_request_in_progress_ = true;
    external_request_data_cb_.Run();
  }
}

void MediaCodecDecoder::PrefetchNextChunk() {
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  DVLOG(1) << class_name() << "::" << __FUNCTION__;

  AccessUnitQueue::Info au_info = au_queue_.GetInfo();

  if (eos_enqueued_ || au_info.data_length >= kPrefetchLimit ||
      au_info.has_eos) {
    // We are done prefetching
    SetState(kPrefetched);
    DVLOG(1) << class_name() << "::" << __FUNCTION__ << " posting PrefetchDone";
    media_task_runner_->PostTask(FROM_HERE,
                                 base::ResetAndReturn(&prefetch_done_cb_));
    return;
  }

  request_data_cb_.Run();
}

void MediaCodecDecoder::ProcessNextFrame() {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  DVLOG(2) << class_name() << "::" << __FUNCTION__;

  DecoderState state = GetState();

  if (state != kPrerolling && state != kRunning && state != kStopping) {
    DVLOG(1) << class_name() << "::" << __FUNCTION__
             << ": state: " << AsString(state) << " stopping frame processing";
    return;
  }

  if (state == kStopping) {
    if (NumDelayedRenderTasks() == 0 && !last_frame_posted_) {
      media_task_runner_->PostTask(
          FROM_HERE, base::Bind(&MediaCodecDecoder::OnLastFrameRendered,
                                weak_factory_.GetWeakPtr(), false));
      last_frame_posted_ = true;
    }

    // We can stop processing, the |au_queue_| and MediaCodec queues can freeze.
    // We only need to let finish the delayed rendering tasks.
    DVLOG(1) << class_name() << "::" << __FUNCTION__ << " kStopping, returning";
    return;
  }

  DCHECK(state == kPrerolling || state == kRunning);

  if (!EnqueueInputBuffer())
    return;

  if (!DepleteOutputBufferQueue())
    return;

  // We need a small delay if we want to stop this thread by
  // decoder_thread_.Stop() reliably.
  // The decoder thread message loop processes all pending
  // (but not delayed) tasks before it can quit; without a delay
  // the message loop might be forever processing the pendng tasks.
  decoder_thread_.task_runner()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&MediaCodecDecoder::ProcessNextFrame, base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(kNextFrameDelay));
}

// Returns false if we should stop decoding process. Right now
// it happens if we got MediaCodec error or detected starvation.
bool MediaCodecDecoder::EnqueueInputBuffer() {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  DVLOG(2) << class_name() << "::" << __FUNCTION__;

  if (eos_enqueued_) {
    DVLOG(1) << class_name() << "::" << __FUNCTION__
             << ": EOS enqueued, returning";
    return true;  // Nothing to do
  }

  if (missing_key_reported_) {
    DVLOG(1) << class_name() << "::" << __FUNCTION__
             << ": NO KEY reported, returning";
    return true;  // Nothing to do
  }

  // Keep the number pending video frames low, ideally maintaining
  // the same audio and video duration after stop request
  if (NumDelayedRenderTasks() > 1) {
    DVLOG(2) << class_name() << "::" << __FUNCTION__ << ": # delayed buffers ("
             << NumDelayedRenderTasks() << ") exceeds 1, returning";
    return true;  // Nothing to do
  }

  // Get the next frame from the queue. As we go, request more data and
  // consume |kConfigChanged| units.

  // |drain_decoder_| can be already set here if we could not dequeue the input
  // buffer for it right away.

  AccessUnitQueue::Info au_info;
  if (!drain_decoder_) {
    au_info = AdvanceAccessUnitQueue(&drain_decoder_);
    if (!au_info.length) {
      // Report starvation and return, Start() will be called again later.
      DVLOG(1) << class_name() << "::" << __FUNCTION__
               << ": starvation detected";
      media_task_runner_->PostTask(FROM_HERE, starvation_cb_);
      return true;
    }

    DCHECK(au_info.front_unit);

#ifndef NDEBUG
    if (verify_next_frame_is_key_) {
      verify_next_frame_is_key_ = false;
      VerifyUnitIsKeyFrame(au_info.front_unit);
    }
#endif
  }

  // Dequeue input buffer

  int index = pending_input_buf_index_;

  // Do not dequeue a new input buffer if we failed with MEDIA_CODEC_NO_KEY.
  // That status does not return this buffer back to the pool of
  // available input buffers. We have to reuse it in QueueSecureInputBuffer().
  if (index == -1) {
    base::TimeDelta timeout =
        base::TimeDelta::FromMilliseconds(kInputBufferTimeout);
    MediaCodecStatus status =
        media_codec_bridge_->DequeueInputBuffer(timeout, &index);

    switch (status) {
      case MEDIA_CODEC_ERROR:
        DVLOG(0) << class_name() << "::" << __FUNCTION__
                 << ": MEDIA_CODEC_ERROR DequeueInputBuffer failed";
        media_task_runner_->PostTask(FROM_HERE, internal_error_cb_);
        return false;

      case MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER:
        DVLOG(2)
            << class_name() << "::" << __FUNCTION__
            << ": DequeueInputBuffer returned MediaCodec.INFO_TRY_AGAIN_LATER.";
        return true;

      default:
        break;
    }

    DCHECK_EQ(status, MEDIA_CODEC_OK);
  }

  DVLOG(2) << class_name() << "::" << __FUNCTION__
           << ": using input buffer index:" << index;

  // We got the buffer
  DCHECK_GE(index, 0);

  const AccessUnit* unit = au_info.front_unit;

  if (drain_decoder_ || unit->is_end_of_stream) {
    DVLOG(1) << class_name() << "::" << __FUNCTION__ << ": QueueEOS";

    // Check that we are not using the pending input buffer to queue EOS
    DCHECK(pending_input_buf_index_ == -1);

    media_codec_bridge_->QueueEOS(index);
    eos_enqueued_ = true;
    return true;
  }

  DCHECK(unit);
  DCHECK(!unit->data.empty());

  // Pending input buffer is already filled with data.
  const uint8_t* memory =
      (pending_input_buf_index_ == -1) ? &unit->data[0] : nullptr;

  pending_input_buf_index_ = -1;
  MediaCodecStatus status = MEDIA_CODEC_OK;

  if (unit->key_id.empty() || unit->iv.empty()) {
    DVLOG(2) << class_name() << "::" << __FUNCTION__
             << ": QueueInputBuffer pts:" << unit->timestamp;

    status = media_codec_bridge_->QueueInputBuffer(
        index, memory, unit->data.size(), unit->timestamp);
  } else {
    DVLOG(2) << class_name() << "::" << __FUNCTION__
             << ": QueueSecureInputBuffer pts:" << unit->timestamp
             << " key_id size:" << unit->key_id.size()
             << " iv size:" << unit->iv.size()
             << " subsamples size:" << unit->subsamples.size();

    status = media_codec_bridge_->QueueSecureInputBuffer(
        index, memory, unit->data.size(), unit->key_id, unit->iv,
        unit->subsamples.empty() ? nullptr : &unit->subsamples[0],
        unit->subsamples.size(), unit->timestamp);
  }

  switch (status) {
    case MEDIA_CODEC_OK:
      break;

    case MEDIA_CODEC_ERROR:
      DVLOG(0) << class_name() << "::" << __FUNCTION__
               << ": MEDIA_CODEC_ERROR: QueueInputBuffer failed";
      media_task_runner_->PostTask(FROM_HERE, internal_error_cb_);
      return false;

    case MEDIA_CODEC_NO_KEY:
      DVLOG(1) << class_name() << "::" << __FUNCTION__
               << ": MEDIA_CODEC_NO_KEY";
      media_task_runner_->PostTask(FROM_HERE, waiting_for_decryption_key_cb_);

      // We need to enqueue the same input buffer after we get the key.
      // The buffer is owned by us (not the MediaCodec) and is filled with data.
      pending_input_buf_index_ = index;

      // In response to the |waiting_for_decryption_key_cb_| the player will
      // request to stop decoder. We need to keep running to properly perform
      // the stop, but prevent generating more |waiting_for_decryption_key_cb_|.
      missing_key_reported_ = true;
      return true;

    default:
      NOTREACHED() << class_name() << "::" << __FUNCTION__
                   << ": unexpected error code " << status;
      media_task_runner_->PostTask(FROM_HERE, internal_error_cb_);
      return false;
  }

  // Have successfully queued input buffer, go to next access unit.
  au_queue_.Advance();
  return true;
}

AccessUnitQueue::Info MediaCodecDecoder::AdvanceAccessUnitQueue(
    bool* drain_decoder) {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DVLOG(2) << class_name() << "::" << __FUNCTION__;

  // Retrieve access units from the |au_queue_| in a loop until we either get
  // a non-config front unit or until the queue is empty.

  DCHECK(drain_decoder != nullptr);

  AccessUnitQueue::Info au_info;

  do {
    // Get current frame
    au_info = au_queue_.GetInfo();

    // Request the data from Demuxer
    if (au_info.data_length <= kPlaybackLowLimit && !au_info.has_eos)
      media_task_runner_->PostTask(FROM_HERE, request_data_cb_);

    if (!au_info.length)
      break;  // Starvation

    if (au_info.configs) {
      DVLOG(1) << class_name() << "::" << __FUNCTION__ << ": received configs "
               << (*au_info.configs);

      // Compare the new and current configs.
      if (IsCodecReconfigureNeeded(*au_info.configs)) {
        DVLOG(1) << class_name() << "::" << __FUNCTION__
                 << ": reconfiguration and decoder drain required";
        *drain_decoder = true;
      }

      // Replace the current configs.
      SetDemuxerConfigs(*au_info.configs);

      // Move to the next frame
      au_queue_.Advance();
    }
  } while (au_info.configs);

  return au_info;
}

// Returns false if there was MediaCodec error.
bool MediaCodecDecoder::DepleteOutputBufferQueue() {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  DVLOG(2) << class_name() << "::" << __FUNCTION__;

  int buffer_index = 0;
  size_t offset = 0;
  size_t size = 0;
  base::TimeDelta pts;
  MediaCodecStatus status;
  bool eos_encountered = false;

  RenderMode render_mode;

  base::TimeDelta timeout =
      base::TimeDelta::FromMilliseconds(kOutputBufferTimeout);

  // Extract all output buffers that are available.
  // Usually there will be only one, but sometimes it is preceeded by
  // MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED or MEDIA_CODEC_OUTPUT_FORMAT_CHANGED.
  do {
    status = media_codec_bridge_->DequeueOutputBuffer(
        timeout, &buffer_index, &offset, &size, &pts, &eos_encountered,
        nullptr);

    // Reset the timeout to 0 for the subsequent DequeueOutputBuffer() calls
    // to quickly break the loop after we got all currently available buffers.
    timeout = base::TimeDelta::FromMilliseconds(0);

    switch (status) {
      case MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED:
        // Output buffers are replaced in MediaCodecBridge, nothing to do.
        DVLOG(2) << class_name() << "::" << __FUNCTION__
                 << " MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED";
        break;

      case MEDIA_CODEC_OUTPUT_FORMAT_CHANGED:
        DVLOG(2) << class_name() << "::" << __FUNCTION__
                 << " MEDIA_CODEC_OUTPUT_FORMAT_CHANGED";
        if (!OnOutputFormatChanged()) {
          DVLOG(1) << class_name() << "::" << __FUNCTION__
                   << ": OnOutputFormatChanged failed, stopping frame"
                   << " processing";
          media_task_runner_->PostTask(FROM_HERE, internal_error_cb_);
          return false;
        }
        break;

      case MEDIA_CODEC_OK:
        // We got the decoded frame.

        is_prepared_ = true;

        if (pts < preroll_timestamp_)
          render_mode = kRenderSkip;
        else if (GetState() == kPrerolling)
          render_mode = kRenderAfterPreroll;
        else
          render_mode = kRenderNow;

        if (!Render(buffer_index, offset, size, render_mode, pts,
                    eos_encountered)) {
          DVLOG(1) << class_name() << "::" << __FUNCTION__
                   << " Render failed, stopping frame processing";
          media_task_runner_->PostTask(FROM_HERE, internal_error_cb_);
          return false;
        }

        if (render_mode == kRenderAfterPreroll) {
          DVLOG(1) << class_name() << "::" << __FUNCTION__ << " pts " << pts
                   << " >= preroll timestamp " << preroll_timestamp_
                   << " preroll done, stopping frame processing";
          media_task_runner_->PostTask(FROM_HERE, internal_preroll_done_cb_);
          return false;
        }
        break;

      case MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER:
        // Nothing to do.
        DVLOG(2) << class_name() << "::" << __FUNCTION__
                 << " MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER";
        break;

      case MEDIA_CODEC_ERROR:
        DVLOG(0) << class_name() << "::" << __FUNCTION__
                 << ": MEDIA_CODEC_ERROR from DequeueOutputBuffer";
        media_task_runner_->PostTask(FROM_HERE, internal_error_cb_);
        break;

      default:
        NOTREACHED();
        break;
    }
  } while (status != MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER &&
           status != MEDIA_CODEC_ERROR && !eos_encountered);

  if (eos_encountered) {
    DVLOG(1) << class_name() << "::" << __FUNCTION__
             << " EOS dequeued, stopping frame processing";
    return false;
  }

  if (status == MEDIA_CODEC_ERROR) {
    DVLOG(0) << class_name() << "::" << __FUNCTION__
             << " MediaCodec error, stopping frame processing";
    return false;
  }

  return true;
}

MediaCodecDecoder::DecoderState MediaCodecDecoder::GetState() const {
  base::AutoLock lock(state_lock_);
  return state_;
}

void MediaCodecDecoder::SetState(DecoderState state) {
  DVLOG(1) << class_name() << "::" << __FUNCTION__ << " " << AsString(state);

  base::AutoLock lock(state_lock_);
  state_ = state;
}

#undef RETURN_STRING
#define RETURN_STRING(x) \
  case x:                \
    return #x;

const char* MediaCodecDecoder::AsString(RenderMode render_mode) {
  switch (render_mode) {
    RETURN_STRING(kRenderSkip);
    RETURN_STRING(kRenderAfterPreroll);
    RETURN_STRING(kRenderNow);
  }
  return nullptr;  // crash early
}

const char* MediaCodecDecoder::AsString(DecoderState state) {
  switch (state) {
    RETURN_STRING(kStopped);
    RETURN_STRING(kPrefetching);
    RETURN_STRING(kPrefetched);
    RETURN_STRING(kPrerolling);
    RETURN_STRING(kPrerolled);
    RETURN_STRING(kRunning);
    RETURN_STRING(kStopping);
    RETURN_STRING(kInEmergencyStop);
    RETURN_STRING(kError);
  }
  return nullptr;  // crash early
}

#undef RETURN_STRING

}  // namespace media
