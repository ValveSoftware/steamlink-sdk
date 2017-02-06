// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/chunk_demuxer.h"

#include <algorithm>
#include <limits>
#include <list>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_tracks.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/frame_processor.h"
#include "media/filters/stream_parser_factory.h"

using base::TimeDelta;

namespace media {

ChunkDemuxerStream::ChunkDemuxerStream(Type type,
                                       bool splice_frames_enabled,
                                       MediaTrack::Id media_track_id)
    : type_(type),
      liveness_(DemuxerStream::LIVENESS_UNKNOWN),
      media_track_id_(media_track_id),
      state_(UNINITIALIZED),
      splice_frames_enabled_(splice_frames_enabled),
      partial_append_window_trimming_enabled_(false) {}

void ChunkDemuxerStream::StartReturningData() {
  DVLOG(1) << "ChunkDemuxerStream::StartReturningData()";
  base::AutoLock auto_lock(lock_);
  DCHECK(read_cb_.is_null());
  ChangeState_Locked(RETURNING_DATA_FOR_READS);
}

void ChunkDemuxerStream::AbortReads() {
  DVLOG(1) << "ChunkDemuxerStream::AbortReads()";
  base::AutoLock auto_lock(lock_);
  ChangeState_Locked(RETURNING_ABORT_FOR_READS);
  if (!read_cb_.is_null())
    base::ResetAndReturn(&read_cb_).Run(kAborted, NULL);
}

void ChunkDemuxerStream::CompletePendingReadIfPossible() {
  base::AutoLock auto_lock(lock_);
  if (read_cb_.is_null())
    return;

  CompletePendingReadIfPossible_Locked();
}

void ChunkDemuxerStream::Shutdown() {
  DVLOG(1) << "ChunkDemuxerStream::Shutdown()";
  base::AutoLock auto_lock(lock_);
  ChangeState_Locked(SHUTDOWN);

  // Pass an end of stream buffer to the pending callback to signal that no more
  // data will be sent.
  if (!read_cb_.is_null()) {
    base::ResetAndReturn(&read_cb_).Run(DemuxerStream::kOk,
                                        StreamParserBuffer::CreateEOSBuffer());
  }
}

bool ChunkDemuxerStream::IsSeekWaitingForData() const {
  base::AutoLock auto_lock(lock_);

  // This method should not be called for text tracks. See the note in
  // MediaSourceState::IsSeekWaitingForData().
  DCHECK_NE(type_, DemuxerStream::TEXT);

  return stream_->IsSeekPending();
}

void ChunkDemuxerStream::Seek(TimeDelta time) {
  DVLOG(1) << "ChunkDemuxerStream::Seek(" << time.InSecondsF() << ")";
  base::AutoLock auto_lock(lock_);
  DCHECK(read_cb_.is_null());
  DCHECK(state_ == UNINITIALIZED || state_ == RETURNING_ABORT_FOR_READS)
      << state_;

  stream_->Seek(time);
}

bool ChunkDemuxerStream::Append(const StreamParser::BufferQueue& buffers) {
  if (buffers.empty())
    return false;

  base::AutoLock auto_lock(lock_);
  DCHECK_NE(state_, SHUTDOWN);
  if (!stream_->Append(buffers)) {
    DVLOG(1) << "ChunkDemuxerStream::Append() : stream append failed";
    return false;
  }

  if (!read_cb_.is_null())
    CompletePendingReadIfPossible_Locked();

  return true;
}

void ChunkDemuxerStream::Remove(TimeDelta start, TimeDelta end,
                                TimeDelta duration) {
  base::AutoLock auto_lock(lock_);
  stream_->Remove(start, end, duration);
}

bool ChunkDemuxerStream::EvictCodedFrames(DecodeTimestamp media_time,
                                          size_t newDataSize) {
  base::AutoLock auto_lock(lock_);
  return stream_->GarbageCollectIfNeeded(media_time, newDataSize);
}

void ChunkDemuxerStream::OnSetDuration(TimeDelta duration) {
  base::AutoLock auto_lock(lock_);
  stream_->OnSetDuration(duration);
}

Ranges<TimeDelta> ChunkDemuxerStream::GetBufferedRanges(
    TimeDelta duration) const {
  base::AutoLock auto_lock(lock_);

  if (type_ == TEXT) {
    // Since text tracks are discontinuous and the lack of cues should not block
    // playback, report the buffered range for text tracks as [0, |duration|) so
    // that intesections with audio & video tracks are computed correctly when
    // no cues are present.
    Ranges<TimeDelta> text_range;
    text_range.Add(TimeDelta(), duration);
    return text_range;
  }

  Ranges<TimeDelta> range = stream_->GetBufferedTime();

  if (range.size() == 0u)
    return range;

  // Clamp the end of the stream's buffered ranges to fit within the duration.
  // This can be done by intersecting the stream's range with the valid time
  // range.
  Ranges<TimeDelta> valid_time_range;
  valid_time_range.Add(range.start(0), duration);
  return range.IntersectionWith(valid_time_range);
}

TimeDelta ChunkDemuxerStream::GetHighestPresentationTimestamp() const {
  return stream_->GetHighestPresentationTimestamp();
}

TimeDelta ChunkDemuxerStream::GetBufferedDuration() const {
  return stream_->GetBufferedDuration();
}

size_t ChunkDemuxerStream::GetBufferedSize() const {
  return stream_->GetBufferedSize();
}

void ChunkDemuxerStream::OnStartOfCodedFrameGroup(
    DecodeTimestamp start_timestamp) {
  DVLOG(2) << "ChunkDemuxerStream::OnStartOfCodedFrameGroup("
           << start_timestamp.InSecondsF() << ")";
  base::AutoLock auto_lock(lock_);
  stream_->OnStartOfCodedFrameGroup(start_timestamp);
}

bool ChunkDemuxerStream::UpdateAudioConfig(
    const AudioDecoderConfig& config,
    const scoped_refptr<MediaLog>& media_log) {
  DCHECK(config.IsValidConfig());
  DCHECK_EQ(type_, AUDIO);
  base::AutoLock auto_lock(lock_);
  if (!stream_) {
    DCHECK_EQ(state_, UNINITIALIZED);

    // On platforms which support splice frames, enable splice frames and
    // partial append window support for most codecs (notably: not opus).
    const bool codec_supported = config.codec() == kCodecMP3 ||
                                 config.codec() == kCodecAAC ||
                                 config.codec() == kCodecVorbis;
    splice_frames_enabled_ = splice_frames_enabled_ && codec_supported;
    partial_append_window_trimming_enabled_ =
        splice_frames_enabled_ && codec_supported;

    stream_.reset(
        new SourceBufferStream(config, media_log, splice_frames_enabled_));
    return true;
  }

  return stream_->UpdateAudioConfig(config);
}

bool ChunkDemuxerStream::UpdateVideoConfig(
    const VideoDecoderConfig& config,
    const scoped_refptr<MediaLog>& media_log) {
  DCHECK(config.IsValidConfig());
  DCHECK_EQ(type_, VIDEO);
  base::AutoLock auto_lock(lock_);

  if (!stream_) {
    DCHECK_EQ(state_, UNINITIALIZED);
    stream_.reset(
        new SourceBufferStream(config, media_log, splice_frames_enabled_));
    return true;
  }

  return stream_->UpdateVideoConfig(config);
}

void ChunkDemuxerStream::UpdateTextConfig(
    const TextTrackConfig& config,
    const scoped_refptr<MediaLog>& media_log) {
  DCHECK_EQ(type_, TEXT);
  base::AutoLock auto_lock(lock_);
  DCHECK(!stream_);
  DCHECK_EQ(state_, UNINITIALIZED);
  stream_.reset(
      new SourceBufferStream(config, media_log, splice_frames_enabled_));
}

void ChunkDemuxerStream::MarkEndOfStream() {
  base::AutoLock auto_lock(lock_);
  stream_->MarkEndOfStream();
}

void ChunkDemuxerStream::UnmarkEndOfStream() {
  base::AutoLock auto_lock(lock_);
  stream_->UnmarkEndOfStream();
}

// DemuxerStream methods.
void ChunkDemuxerStream::Read(const ReadCB& read_cb) {
  base::AutoLock auto_lock(lock_);
  DCHECK_NE(state_, UNINITIALIZED);
  DCHECK(read_cb_.is_null());

  read_cb_ = BindToCurrentLoop(read_cb);
  CompletePendingReadIfPossible_Locked();
}

DemuxerStream::Type ChunkDemuxerStream::type() const { return type_; }

DemuxerStream::Liveness ChunkDemuxerStream::liveness() const {
  base::AutoLock auto_lock(lock_);
  return liveness_;
}

AudioDecoderConfig ChunkDemuxerStream::audio_decoder_config() {
  CHECK_EQ(type_, AUDIO);
  base::AutoLock auto_lock(lock_);
  return stream_->GetCurrentAudioDecoderConfig();
}

VideoDecoderConfig ChunkDemuxerStream::video_decoder_config() {
  CHECK_EQ(type_, VIDEO);
  base::AutoLock auto_lock(lock_);
  return stream_->GetCurrentVideoDecoderConfig();
}

bool ChunkDemuxerStream::SupportsConfigChanges() { return true; }

VideoRotation ChunkDemuxerStream::video_rotation() {
  return VIDEO_ROTATION_0;
}

TextTrackConfig ChunkDemuxerStream::text_track_config() {
  CHECK_EQ(type_, TEXT);
  base::AutoLock auto_lock(lock_);
  return stream_->GetCurrentTextTrackConfig();
}

void ChunkDemuxerStream::SetStreamMemoryLimit(size_t memory_limit) {
  stream_->set_memory_limit(memory_limit);
}

void ChunkDemuxerStream::SetLiveness(Liveness liveness) {
  base::AutoLock auto_lock(lock_);
  liveness_ = liveness;
}

void ChunkDemuxerStream::ChangeState_Locked(State state) {
  lock_.AssertAcquired();
  DVLOG(1) << "ChunkDemuxerStream::ChangeState_Locked() : "
           << "type " << type_
           << " - " << state_ << " -> " << state;
  state_ = state;
}

ChunkDemuxerStream::~ChunkDemuxerStream() {}

void ChunkDemuxerStream::CompletePendingReadIfPossible_Locked() {
  lock_.AssertAcquired();
  DCHECK(!read_cb_.is_null());

  DemuxerStream::Status status = DemuxerStream::kAborted;
  scoped_refptr<StreamParserBuffer> buffer;

  switch (state_) {
    case UNINITIALIZED:
      NOTREACHED();
      return;
    case RETURNING_DATA_FOR_READS:
      switch (stream_->GetNextBuffer(&buffer)) {
        case SourceBufferStream::kSuccess:
          status = DemuxerStream::kOk;
          DVLOG(2) << __FUNCTION__ << ": returning kOk, type " << type_
                   << ", dts " << buffer->GetDecodeTimestamp().InSecondsF()
                   << ", pts " << buffer->timestamp().InSecondsF()
                   << ", dur " << buffer->duration().InSecondsF()
                   << ", key " << buffer->is_key_frame();
          break;
        case SourceBufferStream::kNeedBuffer:
          // Return early without calling |read_cb_| since we don't have
          // any data to return yet.
          DVLOG(2) << __FUNCTION__ << ": returning kNeedBuffer, type "
                   << type_;
          return;
        case SourceBufferStream::kEndOfStream:
          status = DemuxerStream::kOk;
          buffer = StreamParserBuffer::CreateEOSBuffer();
          DVLOG(2) << __FUNCTION__ << ": returning kOk with EOS buffer, type "
                   << type_;
          break;
        case SourceBufferStream::kConfigChange:
          status = kConfigChanged;
          buffer = NULL;
          DVLOG(2) << __FUNCTION__ << ": returning kConfigChange, type "
                   << type_;
          break;
      }
      break;
    case RETURNING_ABORT_FOR_READS:
      // Null buffers should be returned in this state since we are waiting
      // for a seek. Any buffers in the SourceBuffer should NOT be returned
      // because they are associated with the seek.
      status = DemuxerStream::kAborted;
      buffer = NULL;
      DVLOG(2) << __FUNCTION__ << ": returning kAborted, type " << type_;
      break;
    case SHUTDOWN:
      status = DemuxerStream::kOk;
      buffer = StreamParserBuffer::CreateEOSBuffer();
      DVLOG(2) << __FUNCTION__ << ": returning kOk with EOS buffer, type "
               << type_;
      break;
  }

  base::ResetAndReturn(&read_cb_).Run(status, buffer);
}

ChunkDemuxer::ChunkDemuxer(
    const base::Closure& open_cb,
    const EncryptedMediaInitDataCB& encrypted_media_init_data_cb,
    const scoped_refptr<MediaLog>& media_log,
    bool splice_frames_enabled)
    : state_(WAITING_FOR_INIT),
      cancel_next_seek_(false),
      host_(NULL),
      open_cb_(open_cb),
      encrypted_media_init_data_cb_(encrypted_media_init_data_cb),
      enable_text_(false),
      media_log_(media_log),
      pending_source_init_done_count_(0),
      duration_(kNoTimestamp()),
      user_specified_duration_(-1),
      liveness_(DemuxerStream::LIVENESS_UNKNOWN),
      splice_frames_enabled_(splice_frames_enabled),
      detected_audio_track_count_(0),
      detected_video_track_count_(0),
      detected_text_track_count_(0) {
  DCHECK(!open_cb_.is_null());
  DCHECK(!encrypted_media_init_data_cb_.is_null());
}

std::string ChunkDemuxer::GetDisplayName() const {
  return "ChunkDemuxer";
}

void ChunkDemuxer::Initialize(
    DemuxerHost* host,
    const PipelineStatusCB& cb,
    bool enable_text_tracks) {
  DVLOG(1) << "Init()";

  base::AutoLock auto_lock(lock_);

  // The |init_cb_| must only be run after this method returns, so always post.
  init_cb_ = BindToCurrentLoop(cb);
  if (state_ == SHUTDOWN) {
    base::ResetAndReturn(&init_cb_).Run(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }
  DCHECK_EQ(state_, WAITING_FOR_INIT);
  host_ = host;
  enable_text_ = enable_text_tracks;

  ChangeState_Locked(INITIALIZING);

  base::ResetAndReturn(&open_cb_).Run();
}

void ChunkDemuxer::Stop() {
  DVLOG(1) << "Stop()";
  Shutdown();
}

void ChunkDemuxer::Seek(TimeDelta time, const PipelineStatusCB& cb) {
  DVLOG(1) << "Seek(" << time.InSecondsF() << ")";
  DCHECK(time >= TimeDelta());

  base::AutoLock auto_lock(lock_);
  DCHECK(seek_cb_.is_null());

  seek_cb_ = BindToCurrentLoop(cb);
  if (state_ != INITIALIZED && state_ != ENDED) {
    base::ResetAndReturn(&seek_cb_).Run(PIPELINE_ERROR_INVALID_STATE);
    return;
  }

  if (cancel_next_seek_) {
    cancel_next_seek_ = false;
    base::ResetAndReturn(&seek_cb_).Run(PIPELINE_OK);
    return;
  }

  SeekAllSources(time);
  StartReturningData();

  if (IsSeekWaitingForData_Locked()) {
    DVLOG(1) << "Seek() : waiting for more data to arrive.";
    return;
  }

  base::ResetAndReturn(&seek_cb_).Run(PIPELINE_OK);
}

// Demuxer implementation.
base::Time ChunkDemuxer::GetTimelineOffset() const {
  return timeline_offset_;
}

DemuxerStream* ChunkDemuxer::GetStream(DemuxerStream::Type type) {
  DCHECK_NE(type, DemuxerStream::TEXT);
  base::AutoLock auto_lock(lock_);
  if (type == DemuxerStream::VIDEO)
    return video_.get();

  if (type == DemuxerStream::AUDIO)
    return audio_.get();

  return NULL;
}

TimeDelta ChunkDemuxer::GetStartTime() const {
  return TimeDelta();
}

int64_t ChunkDemuxer::GetMemoryUsage() const {
  base::AutoLock auto_lock(lock_);
  return (audio_ ? audio_->GetBufferedSize() : 0) +
         (video_ ? video_->GetBufferedSize() : 0);
}

void ChunkDemuxer::StartWaitingForSeek(TimeDelta seek_time) {
  DVLOG(1) << "StartWaitingForSeek()";
  base::AutoLock auto_lock(lock_);
  DCHECK(state_ == INITIALIZED || state_ == ENDED || state_ == SHUTDOWN ||
         state_ == PARSE_ERROR) << state_;
  DCHECK(seek_cb_.is_null());

  if (state_ == SHUTDOWN || state_ == PARSE_ERROR)
    return;

  AbortPendingReads();
  SeekAllSources(seek_time);

  // Cancel state set in CancelPendingSeek() since we want to
  // accept the next Seek().
  cancel_next_seek_ = false;
}

void ChunkDemuxer::CancelPendingSeek(TimeDelta seek_time) {
  base::AutoLock auto_lock(lock_);
  DCHECK_NE(state_, INITIALIZING);
  DCHECK(seek_cb_.is_null() || IsSeekWaitingForData_Locked());

  if (cancel_next_seek_)
    return;

  AbortPendingReads();
  SeekAllSources(seek_time);

  if (seek_cb_.is_null()) {
    cancel_next_seek_ = true;
    return;
  }

  base::ResetAndReturn(&seek_cb_).Run(PIPELINE_OK);
}

ChunkDemuxer::Status ChunkDemuxer::AddId(const std::string& id,
                                         const std::string& type,
                                         std::vector<std::string>& codecs) {
  base::AutoLock auto_lock(lock_);

  if ((state_ != WAITING_FOR_INIT && state_ != INITIALIZING) || IsValidId(id))
    return kReachedIdLimit;

  bool has_audio = false;
  bool has_video = false;
  std::unique_ptr<media::StreamParser> stream_parser(
      StreamParserFactory::Create(type, codecs, media_log_, &has_audio,
                                  &has_video));

  if (!stream_parser)
    return ChunkDemuxer::kNotSupported;

  if ((has_audio && !source_id_audio_.empty()) ||
      (has_video && !source_id_video_.empty()))
    return kReachedIdLimit;

  if (has_audio)
    source_id_audio_ = id;

  if (has_video)
    source_id_video_ = id;

  std::unique_ptr<FrameProcessor> frame_processor(
      new FrameProcessor(base::Bind(&ChunkDemuxer::IncreaseDurationIfNecessary,
                                    base::Unretained(this)),
                         media_log_));

  std::unique_ptr<MediaSourceState> source_state(new MediaSourceState(
      std::move(stream_parser), std::move(frame_processor),
      base::Bind(&ChunkDemuxer::CreateDemuxerStream, base::Unretained(this)),
      media_log_));

  MediaSourceState::NewTextTrackCB new_text_track_cb;

  if (enable_text_) {
    new_text_track_cb = base::Bind(&ChunkDemuxer::OnNewTextTrack,
                                   base::Unretained(this));
  }

  pending_source_init_done_count_++;

  source_state->Init(
      base::Bind(&ChunkDemuxer::OnSourceInitDone, base::Unretained(this)),
      has_audio, has_video, encrypted_media_init_data_cb_, new_text_track_cb);

  source_state_map_[id] = source_state.release();
  return kOk;
}

void ChunkDemuxer::SetTracksWatcher(
    const std::string& id,
    const MediaTracksUpdatedCB& tracks_updated_cb) {
  base::AutoLock auto_lock(lock_);
  CHECK(IsValidId(id));
  source_state_map_[id]->SetTracksWatcher(tracks_updated_cb);
}

void ChunkDemuxer::RemoveId(const std::string& id) {
  base::AutoLock auto_lock(lock_);
  CHECK(IsValidId(id));

  delete source_state_map_[id];
  source_state_map_.erase(id);

  if (source_id_audio_ == id)
    source_id_audio_.clear();

  if (source_id_video_ == id)
    source_id_video_.clear();
}

Ranges<TimeDelta> ChunkDemuxer::GetBufferedRanges(const std::string& id) const {
  base::AutoLock auto_lock(lock_);
  DCHECK(!id.empty());

  MediaSourceStateMap::const_iterator itr = source_state_map_.find(id);

  DCHECK(itr != source_state_map_.end());
  return itr->second->GetBufferedRanges(duration_, state_ == ENDED);
}

base::TimeDelta ChunkDemuxer::GetHighestPresentationTimestamp(
    const std::string& id) const {
  base::AutoLock auto_lock(lock_);
  DCHECK(!id.empty());

  MediaSourceStateMap::const_iterator itr = source_state_map_.find(id);

  DCHECK(itr != source_state_map_.end());
  return itr->second->GetHighestPresentationTimestamp();
}

bool ChunkDemuxer::EvictCodedFrames(const std::string& id,
                                    base::TimeDelta currentMediaTime,
                                    size_t newDataSize) {
  DVLOG(1) << __FUNCTION__ << "(" << id << ")"
           << " media_time=" << currentMediaTime.InSecondsF()
           << " newDataSize=" << newDataSize;
  base::AutoLock auto_lock(lock_);

  // Note: The direct conversion from PTS to DTS is safe here, since we don't
  // need to know currentTime precisely for GC. GC only needs to know which GOP
  // currentTime points to.
  DecodeTimestamp media_time_dts =
      DecodeTimestamp::FromPresentationTime(currentMediaTime);

  DCHECK(!id.empty());
  MediaSourceStateMap::const_iterator itr = source_state_map_.find(id);
  if (itr == source_state_map_.end()) {
    LOG(WARNING) << __FUNCTION__ << " stream " << id << " not found";
    return false;
  }
  return itr->second->EvictCodedFrames(media_time_dts, newDataSize);
}

bool ChunkDemuxer::AppendData(const std::string& id,
                              const uint8_t* data,
                              size_t length,
                              TimeDelta append_window_start,
                              TimeDelta append_window_end,
                              TimeDelta* timestamp_offset) {
  DVLOG(1) << "AppendData(" << id << ", " << length << ")";

  DCHECK(!id.empty());
  DCHECK(timestamp_offset);

  Ranges<TimeDelta> ranges;

  {
    base::AutoLock auto_lock(lock_);
    DCHECK_NE(state_, ENDED);

    // Capture if any of the SourceBuffers are waiting for data before we start
    // parsing.
    bool old_waiting_for_data = IsSeekWaitingForData_Locked();

    if (length == 0u)
      return true;

    DCHECK(data);

    switch (state_) {
      case INITIALIZING:
      case INITIALIZED:
        DCHECK(IsValidId(id));
        if (!source_state_map_[id]->Append(data, length, append_window_start,
                                           append_window_end,
                                           timestamp_offset)) {
          ReportError_Locked(CHUNK_DEMUXER_ERROR_APPEND_FAILED);
          return false;
        }
        break;

      case PARSE_ERROR:
      case WAITING_FOR_INIT:
      case ENDED:
      case SHUTDOWN:
        DVLOG(1) << "AppendData(): called in unexpected state " << state_;
        return false;
    }

    // Check to see if data was appended at the pending seek point. This
    // indicates we have parsed enough data to complete the seek.
    if (old_waiting_for_data && !IsSeekWaitingForData_Locked() &&
        !seek_cb_.is_null()) {
      base::ResetAndReturn(&seek_cb_).Run(PIPELINE_OK);
    }

    ranges = GetBufferedRanges_Locked();
  }

  host_->OnBufferedTimeRangesChanged(ranges);
  return true;
}

void ChunkDemuxer::ResetParserState(const std::string& id,
                                    TimeDelta append_window_start,
                                    TimeDelta append_window_end,
                                    TimeDelta* timestamp_offset) {
  DVLOG(1) << "ResetParserState(" << id << ")";
  base::AutoLock auto_lock(lock_);
  DCHECK(!id.empty());
  CHECK(IsValidId(id));
  bool old_waiting_for_data = IsSeekWaitingForData_Locked();
  source_state_map_[id]->ResetParserState(append_window_start,
                                          append_window_end,
                                          timestamp_offset);
  // ResetParserState can possibly emit some buffers.
  // Need to check whether seeking can be completed.
  if (old_waiting_for_data && !IsSeekWaitingForData_Locked() &&
      !seek_cb_.is_null()) {
    base::ResetAndReturn(&seek_cb_).Run(PIPELINE_OK);
  }
}

void ChunkDemuxer::Remove(const std::string& id, TimeDelta start,
                          TimeDelta end) {
  DVLOG(1) << "Remove(" << id << ", " << start.InSecondsF()
           << ", " << end.InSecondsF() << ")";
  base::AutoLock auto_lock(lock_);

  DCHECK(!id.empty());
  CHECK(IsValidId(id));
  DCHECK(start >= base::TimeDelta()) << start.InSecondsF();
  DCHECK(start < end) << "start " << start.InSecondsF()
                      << " end " << end.InSecondsF();
  DCHECK(duration_ != kNoTimestamp());
  DCHECK(start <= duration_) << "start " << start.InSecondsF()
                             << " duration " << duration_.InSecondsF();

  if (start == duration_)
    return;

  source_state_map_[id]->Remove(start, end, duration_);
  host_->OnBufferedTimeRangesChanged(GetBufferedRanges_Locked());
}

double ChunkDemuxer::GetDuration() {
  base::AutoLock auto_lock(lock_);
  return GetDuration_Locked();
}

double ChunkDemuxer::GetDuration_Locked() {
  lock_.AssertAcquired();
  if (duration_ == kNoTimestamp())
    return std::numeric_limits<double>::quiet_NaN();

  // Return positive infinity if the resource is unbounded.
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/video.html#dom-media-duration
  if (duration_ == kInfiniteDuration())
    return std::numeric_limits<double>::infinity();

  if (user_specified_duration_ >= 0)
    return user_specified_duration_;

  return duration_.InSecondsF();
}

void ChunkDemuxer::SetDuration(double duration) {
  base::AutoLock auto_lock(lock_);
  DVLOG(1) << "SetDuration(" << duration << ")";
  DCHECK_GE(duration, 0);

  if (duration == GetDuration_Locked())
    return;

  // Compute & bounds check the TimeDelta representation of duration.
  // This can be different if the value of |duration| doesn't fit the range or
  // precision of TimeDelta.
  TimeDelta min_duration = TimeDelta::FromInternalValue(1);
  // Don't use TimeDelta::Max() here, as we want the largest finite time delta.
  TimeDelta max_duration =
      TimeDelta::FromInternalValue(std::numeric_limits<int64_t>::max() - 1);
  double min_duration_in_seconds = min_duration.InSecondsF();
  double max_duration_in_seconds = max_duration.InSecondsF();

  TimeDelta duration_td;
  if (duration == std::numeric_limits<double>::infinity()) {
    duration_td = media::kInfiniteDuration();
  } else if (duration < min_duration_in_seconds) {
    duration_td = min_duration;
  } else if (duration > max_duration_in_seconds) {
    duration_td = max_duration;
  } else {
    duration_td = TimeDelta::FromMicroseconds(
        duration * base::Time::kMicrosecondsPerSecond);
  }

  DCHECK(duration_td > TimeDelta());

  user_specified_duration_ = duration;
  duration_ = duration_td;
  host_->SetDuration(duration_);

  for (MediaSourceStateMap::iterator itr = source_state_map_.begin();
       itr != source_state_map_.end(); ++itr) {
    itr->second->OnSetDuration(duration_);
  }
}

bool ChunkDemuxer::IsParsingMediaSegment(const std::string& id) {
  base::AutoLock auto_lock(lock_);
  DVLOG(1) << "IsParsingMediaSegment(" << id << ")";
  CHECK(IsValidId(id));

  return source_state_map_[id]->parsing_media_segment();
}

void ChunkDemuxer::SetSequenceMode(const std::string& id,
                                   bool sequence_mode) {
  base::AutoLock auto_lock(lock_);
  DVLOG(1) << "SetSequenceMode(" << id << ", " << sequence_mode << ")";
  CHECK(IsValidId(id));
  DCHECK_NE(state_, ENDED);

  source_state_map_[id]->SetSequenceMode(sequence_mode);
}

void ChunkDemuxer::SetGroupStartTimestampIfInSequenceMode(
    const std::string& id,
    base::TimeDelta timestamp_offset) {
  base::AutoLock auto_lock(lock_);
  DVLOG(1) << "SetGroupStartTimestampIfInSequenceMode(" << id << ", "
           << timestamp_offset.InSecondsF() << ")";
  CHECK(IsValidId(id));
  DCHECK_NE(state_, ENDED);

  source_state_map_[id]->SetGroupStartTimestampIfInSequenceMode(
      timestamp_offset);
}


void ChunkDemuxer::MarkEndOfStream(PipelineStatus status) {
  DVLOG(1) << "MarkEndOfStream(" << status << ")";
  base::AutoLock auto_lock(lock_);
  DCHECK_NE(state_, WAITING_FOR_INIT);
  DCHECK_NE(state_, ENDED);

  if (state_ == SHUTDOWN || state_ == PARSE_ERROR)
    return;

  if (state_ == INITIALIZING) {
    ReportError_Locked(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }

  bool old_waiting_for_data = IsSeekWaitingForData_Locked();
  for (MediaSourceStateMap::iterator itr = source_state_map_.begin();
       itr != source_state_map_.end(); ++itr) {
    itr->second->MarkEndOfStream();
  }

  CompletePendingReadsIfPossible();

  // Give a chance to resume the pending seek process.
  if (status != PIPELINE_OK) {
    DCHECK(status == CHUNK_DEMUXER_ERROR_EOS_STATUS_DECODE_ERROR ||
           status == CHUNK_DEMUXER_ERROR_EOS_STATUS_NETWORK_ERROR);
    ReportError_Locked(status);
    return;
  }

  ChangeState_Locked(ENDED);
  DecreaseDurationIfNecessary();

  if (old_waiting_for_data && !IsSeekWaitingForData_Locked() &&
      !seek_cb_.is_null()) {
    base::ResetAndReturn(&seek_cb_).Run(PIPELINE_OK);
  }
}

void ChunkDemuxer::UnmarkEndOfStream() {
  DVLOG(1) << "UnmarkEndOfStream()";
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, ENDED);

  ChangeState_Locked(INITIALIZED);

  for (MediaSourceStateMap::iterator itr = source_state_map_.begin();
       itr != source_state_map_.end(); ++itr) {
    itr->second->UnmarkEndOfStream();
  }
}

void ChunkDemuxer::Shutdown() {
  DVLOG(1) << "Shutdown()";
  base::AutoLock auto_lock(lock_);

  if (state_ == SHUTDOWN)
    return;

  ShutdownAllStreams();

  ChangeState_Locked(SHUTDOWN);

  if (!seek_cb_.is_null())
    base::ResetAndReturn(&seek_cb_).Run(PIPELINE_ERROR_ABORT);
}

void ChunkDemuxer::SetMemoryLimits(DemuxerStream::Type type,
                                   size_t memory_limit) {
  for (MediaSourceStateMap::iterator itr = source_state_map_.begin();
       itr != source_state_map_.end(); ++itr) {
    itr->second->SetMemoryLimits(type, memory_limit);
  }
}

void ChunkDemuxer::ChangeState_Locked(State new_state) {
  lock_.AssertAcquired();
  DVLOG(1) << "ChunkDemuxer::ChangeState_Locked() : "
           << state_ << " -> " << new_state;
  state_ = new_state;
}

ChunkDemuxer::~ChunkDemuxer() {
  DCHECK_NE(state_, INITIALIZED);

  STLDeleteValues(&source_state_map_);
}

void ChunkDemuxer::ReportError_Locked(PipelineStatus error) {
  DVLOG(1) << "ReportError_Locked(" << error << ")";
  lock_.AssertAcquired();
  DCHECK_NE(error, PIPELINE_OK);

  ChangeState_Locked(PARSE_ERROR);

  PipelineStatusCB cb;

  if (!init_cb_.is_null()) {
    std::swap(cb, init_cb_);
  } else {
    if (!seek_cb_.is_null())
      std::swap(cb, seek_cb_);

    ShutdownAllStreams();
  }

  if (!cb.is_null()) {
    cb.Run(error);
    return;
  }

  base::AutoUnlock auto_unlock(lock_);
  host_->OnDemuxerError(error);
}

bool ChunkDemuxer::IsSeekWaitingForData_Locked() const {
  lock_.AssertAcquired();
  for (MediaSourceStateMap::const_iterator itr = source_state_map_.begin();
       itr != source_state_map_.end(); ++itr) {
    if (itr->second->IsSeekWaitingForData())
      return true;
  }

  return false;
}

void ChunkDemuxer::OnSourceInitDone(
    const StreamParser::InitParameters& params) {
  DVLOG(1) << "OnSourceInitDone(" << params.duration.InSecondsF() << ")";
  lock_.AssertAcquired();
  DCHECK_EQ(state_, INITIALIZING);
  if (!audio_ && !video_) {
    ReportError_Locked(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }

  if (!params.duration.is_zero() && duration_ == kNoTimestamp())
    UpdateDuration(params.duration);

  if (!params.timeline_offset.is_null()) {
    if (!timeline_offset_.is_null() &&
        params.timeline_offset != timeline_offset_) {
      MEDIA_LOG(ERROR, media_log_)
          << "Timeline offset is not the same across all SourceBuffers.";
      ReportError_Locked(DEMUXER_ERROR_COULD_NOT_OPEN);
      return;
    }

    timeline_offset_ = params.timeline_offset;
  }

  if (params.liveness != DemuxerStream::LIVENESS_UNKNOWN) {
    if (audio_)
      audio_->SetLiveness(params.liveness);
    if (video_)
      video_->SetLiveness(params.liveness);
  }

  detected_audio_track_count_ += params.detected_audio_track_count;
  detected_video_track_count_ += params.detected_video_track_count;
  detected_text_track_count_ += params.detected_text_track_count;

  // Wait until all streams have initialized.
  pending_source_init_done_count_--;

  if (pending_source_init_done_count_ > 0)
    return;

  DCHECK_EQ(0, pending_source_init_done_count_);
  DCHECK((source_id_audio_.empty() == !audio_) &&
         (source_id_video_.empty() == !video_));

  // Record detected track counts by type corresponding to an MSE playback.
  // Counts are split into 50 buckets, capped into [0,100] range.
  UMA_HISTOGRAM_COUNTS_100("Media.MSE.DetectedTrackCount.Audio",
                           detected_audio_track_count_);
  UMA_HISTOGRAM_COUNTS_100("Media.MSE.DetectedTrackCount.Video",
                           detected_video_track_count_);
  UMA_HISTOGRAM_COUNTS_100("Media.MSE.DetectedTrackCount.Text",
                           detected_text_track_count_);

  if (video_) {
    media_log_->RecordRapporWithSecurityOrigin(
        "Media.OriginUrl.MSE.VideoCodec." +
        GetCodecName(video_->video_decoder_config().codec()));
  }

  SeekAllSources(GetStartTime());
  StartReturningData();

  if (duration_ == kNoTimestamp())
    duration_ = kInfiniteDuration();

  // The demuxer is now initialized after the |start_timestamp_| was set.
  ChangeState_Locked(INITIALIZED);
  base::ResetAndReturn(&init_cb_).Run(PIPELINE_OK);
}

// static
MediaTrack::Id ChunkDemuxer::GenerateMediaTrackId() {
  static unsigned g_track_count = 0;
  return base::UintToString(++g_track_count);
}

ChunkDemuxerStream* ChunkDemuxer::CreateDemuxerStream(
    DemuxerStream::Type type) {
  // New ChunkDemuxerStreams can be created only during initialization segment
  // processing, which happens when a new chunk of data is appended and the
  // lock_ must be held by ChunkDemuxer::AppendData.
  lock_.AssertAcquired();

  MediaTrack::Id media_track_id = GenerateMediaTrackId();

  switch (type) {
    case DemuxerStream::AUDIO:
      if (audio_)
        return NULL;
      audio_.reset(new ChunkDemuxerStream(
          DemuxerStream::AUDIO, splice_frames_enabled_, media_track_id));
      return audio_.get();
      break;
    case DemuxerStream::VIDEO:
      if (video_)
        return NULL;
      video_.reset(new ChunkDemuxerStream(
          DemuxerStream::VIDEO, splice_frames_enabled_, media_track_id));
      return video_.get();
      break;
    case DemuxerStream::TEXT: {
      return new ChunkDemuxerStream(DemuxerStream::TEXT, splice_frames_enabled_,
                                    media_track_id);
      break;
    }
    case DemuxerStream::UNKNOWN:
    case DemuxerStream::NUM_TYPES:
      NOTREACHED();
      return NULL;
  }
  NOTREACHED();
  return NULL;
}

void ChunkDemuxer::OnNewTextTrack(ChunkDemuxerStream* text_stream,
                                  const TextTrackConfig& config) {
  lock_.AssertAcquired();
  DCHECK_NE(state_, SHUTDOWN);
  host_->AddTextStream(text_stream, config);
}

bool ChunkDemuxer::IsValidId(const std::string& source_id) const {
  lock_.AssertAcquired();
  return source_state_map_.count(source_id) > 0u;
}

void ChunkDemuxer::UpdateDuration(TimeDelta new_duration) {
  DCHECK(duration_ != new_duration);
  user_specified_duration_ = -1;
  duration_ = new_duration;
  host_->SetDuration(new_duration);
}

void ChunkDemuxer::IncreaseDurationIfNecessary(TimeDelta new_duration) {
  DCHECK(new_duration != kNoTimestamp());
  DCHECK(new_duration != kInfiniteDuration());

  // Per April 1, 2014 MSE spec editor's draft:
  // https://dvcs.w3.org/hg/html-media/raw-file/d471a4412040/media-source/
  //     media-source.html#sourcebuffer-coded-frame-processing
  // 5. If the media segment contains data beyond the current duration, then run
  //    the duration change algorithm with new duration set to the maximum of
  //    the current duration and the group end timestamp.

  if (new_duration <= duration_)
    return;

  DVLOG(2) << __FUNCTION__ << ": Increasing duration: "
           << duration_.InSecondsF() << " -> " << new_duration.InSecondsF();

  UpdateDuration(new_duration);
}

void ChunkDemuxer::DecreaseDurationIfNecessary() {
  lock_.AssertAcquired();

  TimeDelta max_duration;

  for (MediaSourceStateMap::const_iterator itr = source_state_map_.begin();
       itr != source_state_map_.end(); ++itr) {
    max_duration = std::max(max_duration,
                            itr->second->GetMaxBufferedDuration());
  }

  if (max_duration.is_zero())
    return;

  if (max_duration < duration_)
    UpdateDuration(max_duration);
}

Ranges<TimeDelta> ChunkDemuxer::GetBufferedRanges() const {
  base::AutoLock auto_lock(lock_);
  return GetBufferedRanges_Locked();
}

Ranges<TimeDelta> ChunkDemuxer::GetBufferedRanges_Locked() const {
  lock_.AssertAcquired();

  bool ended = state_ == ENDED;
  // TODO(acolwell): When we start allowing SourceBuffers that are not active,
  // we'll need to update this loop to only add ranges from active sources.
  MediaSourceState::RangesList ranges_list;
  for (MediaSourceStateMap::const_iterator itr = source_state_map_.begin();
       itr != source_state_map_.end(); ++itr) {
    ranges_list.push_back(itr->second->GetBufferedRanges(duration_, ended));
  }

  return MediaSourceState::ComputeRangesIntersection(ranges_list, ended);
}

void ChunkDemuxer::StartReturningData() {
  for (MediaSourceStateMap::iterator itr = source_state_map_.begin();
       itr != source_state_map_.end(); ++itr) {
    itr->second->StartReturningData();
  }
}

void ChunkDemuxer::AbortPendingReads() {
  for (MediaSourceStateMap::iterator itr = source_state_map_.begin();
       itr != source_state_map_.end(); ++itr) {
    itr->second->AbortReads();
  }
}

void ChunkDemuxer::SeekAllSources(TimeDelta seek_time) {
  for (MediaSourceStateMap::iterator itr = source_state_map_.begin();
       itr != source_state_map_.end(); ++itr) {
    itr->second->Seek(seek_time);
  }
}

void ChunkDemuxer::CompletePendingReadsIfPossible() {
  for (MediaSourceStateMap::iterator itr = source_state_map_.begin();
       itr != source_state_map_.end(); ++itr) {
    itr->second->CompletePendingReadIfPossible();
  }
}

void ChunkDemuxer::ShutdownAllStreams() {
  for (MediaSourceStateMap::iterator itr = source_state_map_.begin();
       itr != source_state_map_.end(); ++itr) {
    itr->second->Shutdown();
  }
}

}  // namespace media
