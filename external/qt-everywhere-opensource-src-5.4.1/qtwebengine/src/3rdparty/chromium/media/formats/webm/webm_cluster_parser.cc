// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/webm/webm_cluster_parser.h"

#include <vector>

#include "base/logging.h"
#include "base/sys_byteorder.h"
#include "media/base/buffers.h"
#include "media/base/decrypt_config.h"
#include "media/filters/webvtt_util.h"
#include "media/formats/webm/webm_constants.h"
#include "media/formats/webm/webm_crypto_helpers.h"
#include "media/formats/webm/webm_webvtt_parser.h"

namespace media {

WebMClusterParser::WebMClusterParser(
    int64 timecode_scale,
    int audio_track_num,
    base::TimeDelta audio_default_duration,
    int video_track_num,
    base::TimeDelta video_default_duration,
    const WebMTracksParser::TextTracks& text_tracks,
    const std::set<int64>& ignored_tracks,
    const std::string& audio_encryption_key_id,
    const std::string& video_encryption_key_id,
    const LogCB& log_cb)
    : timecode_multiplier_(timecode_scale / 1000.0),
      ignored_tracks_(ignored_tracks),
      audio_encryption_key_id_(audio_encryption_key_id),
      video_encryption_key_id_(video_encryption_key_id),
      parser_(kWebMIdCluster, this),
      last_block_timecode_(-1),
      block_data_size_(-1),
      block_duration_(-1),
      block_add_id_(-1),
      block_additional_data_size_(-1),
      discard_padding_(-1),
      cluster_timecode_(-1),
      cluster_start_time_(kNoTimestamp()),
      cluster_ended_(false),
      audio_(audio_track_num, false, audio_default_duration, log_cb),
      video_(video_track_num, true, video_default_duration, log_cb),
      ready_buffer_upper_bound_(kNoTimestamp()),
      log_cb_(log_cb) {
  for (WebMTracksParser::TextTracks::const_iterator it = text_tracks.begin();
       it != text_tracks.end();
       ++it) {
    text_track_map_.insert(std::make_pair(
        it->first, Track(it->first, false, kNoTimestamp(), log_cb_)));
  }
}

WebMClusterParser::~WebMClusterParser() {}

void WebMClusterParser::Reset() {
  last_block_timecode_ = -1;
  cluster_timecode_ = -1;
  cluster_start_time_ = kNoTimestamp();
  cluster_ended_ = false;
  parser_.Reset();
  audio_.Reset();
  video_.Reset();
  ResetTextTracks();
  ready_buffer_upper_bound_ = kNoTimestamp();
}

int WebMClusterParser::Parse(const uint8* buf, int size) {
  audio_.ClearReadyBuffers();
  video_.ClearReadyBuffers();
  ClearTextTrackReadyBuffers();
  ready_buffer_upper_bound_ = kNoTimestamp();

  int result = parser_.Parse(buf, size);

  if (result < 0) {
    cluster_ended_ = false;
    return result;
  }

  cluster_ended_ = parser_.IsParsingComplete();
  if (cluster_ended_) {
    // If there were no buffers in this cluster, set the cluster start time to
    // be the |cluster_timecode_|.
    if (cluster_start_time_ == kNoTimestamp()) {
      // If the cluster did not even have a |cluster_timecode_|, signal parse
      // error.
      if (cluster_timecode_ < 0)
        return -1;

      cluster_start_time_ = base::TimeDelta::FromMicroseconds(
          cluster_timecode_ * timecode_multiplier_);
    }

    // Reset the parser if we're done parsing so that
    // it is ready to accept another cluster on the next
    // call.
    parser_.Reset();

    last_block_timecode_ = -1;
    cluster_timecode_ = -1;
  }

  return result;
}

const WebMClusterParser::BufferQueue& WebMClusterParser::GetAudioBuffers() {
  if (ready_buffer_upper_bound_ == kNoTimestamp())
    UpdateReadyBuffers();

  return audio_.ready_buffers();
}

const WebMClusterParser::BufferQueue& WebMClusterParser::GetVideoBuffers() {
  if (ready_buffer_upper_bound_ == kNoTimestamp())
    UpdateReadyBuffers();

  return video_.ready_buffers();
}

const WebMClusterParser::TextBufferQueueMap&
WebMClusterParser::GetTextBuffers() {
  if (ready_buffer_upper_bound_ == kNoTimestamp())
    UpdateReadyBuffers();

  // Translate our |text_track_map_| into |text_buffers_map_|, inserting rows in
  // the output only for non-empty ready_buffer() queues in |text_track_map_|.
  text_buffers_map_.clear();
  for (TextTrackMap::const_iterator itr = text_track_map_.begin();
       itr != text_track_map_.end();
       ++itr) {
    const BufferQueue& text_buffers = itr->second.ready_buffers();
    if (!text_buffers.empty())
      text_buffers_map_.insert(std::make_pair(itr->first, text_buffers));
  }

  return text_buffers_map_;
}

WebMParserClient* WebMClusterParser::OnListStart(int id) {
  if (id == kWebMIdCluster) {
    cluster_timecode_ = -1;
    cluster_start_time_ = kNoTimestamp();
  } else if (id == kWebMIdBlockGroup) {
    block_data_.reset();
    block_data_size_ = -1;
    block_duration_ = -1;
    discard_padding_ = -1;
    discard_padding_set_ = false;
  } else if (id == kWebMIdBlockAdditions) {
    block_add_id_ = -1;
    block_additional_data_.reset();
    block_additional_data_size_ = -1;
  }

  return this;
}

bool WebMClusterParser::OnListEnd(int id) {
  if (id != kWebMIdBlockGroup)
    return true;

  // Make sure the BlockGroup actually had a Block.
  if (block_data_size_ == -1) {
    MEDIA_LOG(log_cb_) << "Block missing from BlockGroup.";
    return false;
  }

  bool result = ParseBlock(false, block_data_.get(), block_data_size_,
                           block_additional_data_.get(),
                           block_additional_data_size_, block_duration_,
                           discard_padding_set_ ? discard_padding_ : 0);
  block_data_.reset();
  block_data_size_ = -1;
  block_duration_ = -1;
  block_add_id_ = -1;
  block_additional_data_.reset();
  block_additional_data_size_ = -1;
  discard_padding_ = -1;
  discard_padding_set_ = false;
  return result;
}

bool WebMClusterParser::OnUInt(int id, int64 val) {
  int64* dst;
  switch (id) {
    case kWebMIdTimecode:
      dst = &cluster_timecode_;
      break;
    case kWebMIdBlockDuration:
      dst = &block_duration_;
      break;
    case kWebMIdBlockAddID:
      dst = &block_add_id_;
      break;
    default:
      return true;
  }
  if (*dst != -1)
    return false;
  *dst = val;
  return true;
}

bool WebMClusterParser::ParseBlock(bool is_simple_block, const uint8* buf,
                                   int size, const uint8* additional,
                                   int additional_size, int duration,
                                   int64 discard_padding) {
  if (size < 4)
    return false;

  // Return an error if the trackNum > 127. We just aren't
  // going to support large track numbers right now.
  if (!(buf[0] & 0x80)) {
    MEDIA_LOG(log_cb_) << "TrackNumber over 127 not supported";
    return false;
  }

  int track_num = buf[0] & 0x7f;
  int timecode = buf[1] << 8 | buf[2];
  int flags = buf[3] & 0xff;
  int lacing = (flags >> 1) & 0x3;

  if (lacing) {
    MEDIA_LOG(log_cb_) << "Lacing " << lacing << " is not supported yet.";
    return false;
  }

  // Sign extend negative timecode offsets.
  if (timecode & 0x8000)
    timecode |= ~0xffff;

  const uint8* frame_data = buf + 4;
  int frame_size = size - (frame_data - buf);
  return OnBlock(is_simple_block, track_num, timecode, duration, flags,
                 frame_data, frame_size, additional, additional_size,
                 discard_padding);
}

bool WebMClusterParser::OnBinary(int id, const uint8* data, int size) {
  switch (id) {
    case kWebMIdSimpleBlock:
      return ParseBlock(true, data, size, NULL, -1, -1, 0);

    case kWebMIdBlock:
      if (block_data_) {
        MEDIA_LOG(log_cb_) << "More than 1 Block in a BlockGroup is not "
                              "supported.";
        return false;
      }
      block_data_.reset(new uint8[size]);
      memcpy(block_data_.get(), data, size);
      block_data_size_ = size;
      return true;

    case kWebMIdBlockAdditional: {
      uint64 block_add_id = base::HostToNet64(block_add_id_);
      if (block_additional_data_) {
        // TODO(vigneshv): Technically, more than 1 BlockAdditional is allowed
        // as per matroska spec. But for now we don't have a use case to
        // support parsing of such files. Take a look at this again when such a
        // case arises.
        MEDIA_LOG(log_cb_) << "More than 1 BlockAdditional in a BlockGroup is "
                              "not supported.";
        return false;
      }
      // First 8 bytes of side_data in DecoderBuffer is the BlockAddID
      // element's value in Big Endian format. This is done to mimic ffmpeg
      // demuxer's behavior.
      block_additional_data_size_ = size + sizeof(block_add_id);
      block_additional_data_.reset(new uint8[block_additional_data_size_]);
      memcpy(block_additional_data_.get(), &block_add_id,
             sizeof(block_add_id));
      memcpy(block_additional_data_.get() + 8, data, size);
      return true;
    }
    case kWebMIdDiscardPadding: {
      if (discard_padding_set_ || size <= 0 || size > 8)
        return false;
      discard_padding_set_ = true;

      // Read in the big-endian integer.
      discard_padding_ = static_cast<int8>(data[0]);
      for (int i = 1; i < size; ++i)
        discard_padding_ = (discard_padding_ << 8) | data[i];

      return true;
    }
    default:
      return true;
  }
}

bool WebMClusterParser::OnBlock(bool is_simple_block, int track_num,
                                int timecode,
                                int  block_duration,
                                int flags,
                                const uint8* data, int size,
                                const uint8* additional, int additional_size,
                                int64 discard_padding) {
  DCHECK_GE(size, 0);
  if (cluster_timecode_ == -1) {
    MEDIA_LOG(log_cb_) << "Got a block before cluster timecode.";
    return false;
  }

  // TODO(acolwell): Should relative negative timecode offsets be rejected?  Or
  // only when the absolute timecode is negative?  See http://crbug.com/271794
  if (timecode < 0) {
    MEDIA_LOG(log_cb_) << "Got a block with negative timecode offset "
                       << timecode;
    return false;
  }

  if (last_block_timecode_ != -1 && timecode < last_block_timecode_) {
    MEDIA_LOG(log_cb_)
        << "Got a block with a timecode before the previous block.";
    return false;
  }

  Track* track = NULL;
  StreamParserBuffer::Type buffer_type = DemuxerStream::AUDIO;
  std::string encryption_key_id;
  if (track_num == audio_.track_num()) {
    track = &audio_;
    encryption_key_id = audio_encryption_key_id_;
  } else if (track_num == video_.track_num()) {
    track = &video_;
    encryption_key_id = video_encryption_key_id_;
    buffer_type = DemuxerStream::VIDEO;
  } else if (ignored_tracks_.find(track_num) != ignored_tracks_.end()) {
    return true;
  } else if (Track* const text_track = FindTextTrack(track_num)) {
    if (is_simple_block)  // BlockGroup is required for WebVTT cues
      return false;
    if (block_duration < 0)  // not specified
      return false;
    track = text_track;
    buffer_type = DemuxerStream::TEXT;
  } else {
    MEDIA_LOG(log_cb_) << "Unexpected track number " << track_num;
    return false;
  }

  last_block_timecode_ = timecode;

  base::TimeDelta timestamp = base::TimeDelta::FromMicroseconds(
      (cluster_timecode_ + timecode) * timecode_multiplier_);

  scoped_refptr<StreamParserBuffer> buffer;
  if (buffer_type != DemuxerStream::TEXT) {
    // The first bit of the flags is set when a SimpleBlock contains only
    // keyframes. If this is a Block, then inspection of the payload is
    // necessary to determine whether it contains a keyframe or not.
    // http://www.matroska.org/technical/specs/index.html
    bool is_keyframe =
        is_simple_block ? (flags & 0x80) != 0 : track->IsKeyframe(data, size);

    // Every encrypted Block has a signal byte and IV prepended to it. Current
    // encrypted WebM request for comments specification is here
    // http://wiki.webmproject.org/encryption/webm-encryption-rfc
    scoped_ptr<DecryptConfig> decrypt_config;
    int data_offset = 0;
    if (!encryption_key_id.empty() &&
        !WebMCreateDecryptConfig(
             data, size,
             reinterpret_cast<const uint8*>(encryption_key_id.data()),
             encryption_key_id.size(),
             &decrypt_config, &data_offset)) {
      return false;
    }

    // TODO(wolenetz/acolwell): Validate and use a common cross-parser TrackId
    // type with remapped bytestream track numbers and allow multiple tracks as
    // applicable. See https://crbug.com/341581.
    buffer = StreamParserBuffer::CopyFrom(
        data + data_offset, size - data_offset,
        additional, additional_size,
        is_keyframe, buffer_type, track_num);

    if (decrypt_config)
      buffer->set_decrypt_config(decrypt_config.Pass());
  } else {
    std::string id, settings, content;
    WebMWebVTTParser::Parse(data, size, &id, &settings, &content);

    std::vector<uint8> side_data;
    MakeSideData(id.begin(), id.end(),
                 settings.begin(), settings.end(),
                 &side_data);

    // TODO(wolenetz/acolwell): Validate and use a common cross-parser TrackId
    // type with remapped bytestream track numbers and allow multiple tracks as
    // applicable. See https://crbug.com/341581.
    buffer = StreamParserBuffer::CopyFrom(
        reinterpret_cast<const uint8*>(content.data()),
        content.length(),
        &side_data[0],
        side_data.size(),
        true, buffer_type, track_num);
  }

  buffer->set_timestamp(timestamp);
  if (cluster_start_time_ == kNoTimestamp())
    cluster_start_time_ = timestamp;

  if (block_duration >= 0) {
    buffer->set_duration(base::TimeDelta::FromMicroseconds(
        block_duration * timecode_multiplier_));
  } else {
    DCHECK_NE(buffer_type, DemuxerStream::TEXT);
    buffer->set_duration(track->default_duration());
  }

  if (discard_padding != 0) {
    buffer->set_discard_padding(std::make_pair(
        base::TimeDelta(),
        base::TimeDelta::FromMicroseconds(discard_padding / 1000)));
  }

  return track->AddBuffer(buffer);
}

WebMClusterParser::Track::Track(int track_num,
                                bool is_video,
                                base::TimeDelta default_duration,
                                const LogCB& log_cb)
    : track_num_(track_num),
      is_video_(is_video),
      default_duration_(default_duration),
      estimated_next_frame_duration_(kNoTimestamp()),
      log_cb_(log_cb) {
  DCHECK(default_duration_ == kNoTimestamp() ||
         default_duration_ > base::TimeDelta());
}

WebMClusterParser::Track::~Track() {}

base::TimeDelta WebMClusterParser::Track::GetReadyUpperBound() {
  DCHECK(ready_buffers_.empty());
  if (last_added_buffer_missing_duration_)
    return last_added_buffer_missing_duration_->GetDecodeTimestamp();

  return kInfiniteDuration();
}

void WebMClusterParser::Track::ExtractReadyBuffers(
    const base::TimeDelta before_timestamp) {
  DCHECK(ready_buffers_.empty());
  DCHECK(base::TimeDelta() <= before_timestamp);
  DCHECK(kNoTimestamp() != before_timestamp);

  if (buffers_.empty())
    return;

  if (buffers_.back()->GetDecodeTimestamp() < before_timestamp) {
    // All of |buffers_| are ready.
    ready_buffers_.swap(buffers_);
    DVLOG(3) << __FUNCTION__ << " : " << track_num_ << " All "
             << ready_buffers_.size() << " are ready: before upper bound ts "
             << before_timestamp.InSecondsF();
    return;
  }

  // Not all of |buffers_| are ready yet. Move any that are ready to
  // |ready_buffers_|.
  while (true) {
    const scoped_refptr<StreamParserBuffer>& buffer = buffers_.front();
    if (buffer->GetDecodeTimestamp() >= before_timestamp)
      break;
    ready_buffers_.push_back(buffer);
    buffers_.pop_front();
    DCHECK(!buffers_.empty());
  }

  DVLOG(3) << __FUNCTION__ << " : " << track_num_ << " Only "
           << ready_buffers_.size() << " ready, " << buffers_.size()
           << " at or after upper bound ts " << before_timestamp.InSecondsF();
}

bool WebMClusterParser::Track::AddBuffer(
    const scoped_refptr<StreamParserBuffer>& buffer) {
  DVLOG(2) << "AddBuffer() : " << track_num_
           << " ts " << buffer->timestamp().InSecondsF()
           << " dur " << buffer->duration().InSecondsF()
           << " kf " << buffer->IsKeyframe()
           << " size " << buffer->data_size();

  if (last_added_buffer_missing_duration_) {
    base::TimeDelta derived_duration =
        buffer->timestamp() - last_added_buffer_missing_duration_->timestamp();
    last_added_buffer_missing_duration_->set_duration(derived_duration);

    DVLOG(2) << "AddBuffer() : applied derived duration to held-back buffer : "
             << " ts "
             << last_added_buffer_missing_duration_->timestamp().InSecondsF()
             << " dur "
             << last_added_buffer_missing_duration_->duration().InSecondsF()
             << " kf " << last_added_buffer_missing_duration_->IsKeyframe()
             << " size " << last_added_buffer_missing_duration_->data_size();
    scoped_refptr<StreamParserBuffer> updated_buffer =
        last_added_buffer_missing_duration_;
    last_added_buffer_missing_duration_ = NULL;
    if (!QueueBuffer(updated_buffer))
      return false;
  }

  if (buffer->duration() == kNoTimestamp()) {
    last_added_buffer_missing_duration_ = buffer;
    DVLOG(2) << "AddBuffer() : holding back buffer that is missing duration";
    return true;
  }

  return QueueBuffer(buffer);
}

void WebMClusterParser::Track::ApplyDurationEstimateIfNeeded() {
  if (!last_added_buffer_missing_duration_)
    return;

  last_added_buffer_missing_duration_->set_duration(GetDurationEstimate());

  DVLOG(2) << "ApplyDurationEstimateIfNeeded() : new dur : "
           << " ts "
           << last_added_buffer_missing_duration_->timestamp().InSecondsF()
           << " dur "
           << last_added_buffer_missing_duration_->duration().InSecondsF()
           << " kf " << last_added_buffer_missing_duration_->IsKeyframe()
           << " size " << last_added_buffer_missing_duration_->data_size();

  // Don't use the applied duration as a future estimation (don't use
  // QueueBuffer() here.)
  buffers_.push_back(last_added_buffer_missing_duration_);
  last_added_buffer_missing_duration_ = NULL;
}

void WebMClusterParser::Track::ClearReadyBuffers() {
  // Note that |buffers_| are kept and |estimated_next_frame_duration_| is not
  // reset here.
  ready_buffers_.clear();
}

void WebMClusterParser::Track::Reset() {
  ClearReadyBuffers();
  buffers_.clear();
  last_added_buffer_missing_duration_ = NULL;
}

bool WebMClusterParser::Track::IsKeyframe(const uint8* data, int size) const {
  // For now, assume that all blocks are keyframes for datatypes other than
  // video. This is a valid assumption for Vorbis, WebVTT, & Opus.
  if (!is_video_)
    return true;

  // Make sure the block is big enough for the minimal keyframe header size.
  if (size < 7)
    return false;

  // The LSb of the first byte must be a 0 for a keyframe.
  // http://tools.ietf.org/html/rfc6386 Section 19.1
  if ((data[0] & 0x01) != 0)
    return false;

  // Verify VP8 keyframe startcode.
  // http://tools.ietf.org/html/rfc6386 Section 19.1
  if (data[3] != 0x9d || data[4] != 0x01 || data[5] != 0x2a)
    return false;

  return true;
}

bool WebMClusterParser::Track::QueueBuffer(
    const scoped_refptr<StreamParserBuffer>& buffer) {
  DCHECK(!last_added_buffer_missing_duration_);

  // WebMClusterParser::OnBlock() gives MEDIA_LOG and parse error on decreasing
  // block timecode detection within a cluster. Therefore, we should not see
  // those here.
  base::TimeDelta previous_buffers_timestamp = buffers_.empty() ?
      base::TimeDelta() : buffers_.back()->GetDecodeTimestamp();
  CHECK(previous_buffers_timestamp <= buffer->GetDecodeTimestamp());

  base::TimeDelta duration = buffer->duration();
  if (duration < base::TimeDelta() || duration == kNoTimestamp()) {
    MEDIA_LOG(log_cb_) << "Invalid buffer duration: " << duration.InSecondsF();
    return false;
  }

  // The estimated frame duration is the minimum non-zero duration since the
  // last initialization segment.  The minimum is used to ensure frame durations
  // aren't overestimated.
  if (duration > base::TimeDelta()) {
    if (estimated_next_frame_duration_ == kNoTimestamp()) {
      estimated_next_frame_duration_ = duration;
    } else {
      estimated_next_frame_duration_ =
          std::min(duration, estimated_next_frame_duration_);
    }
  }

  buffers_.push_back(buffer);
  return true;
}

base::TimeDelta WebMClusterParser::Track::GetDurationEstimate() {
  base::TimeDelta duration = estimated_next_frame_duration_;
  if (duration != kNoTimestamp()) {
    DVLOG(3) << __FUNCTION__ << " : using estimated duration";
  } else {
    DVLOG(3) << __FUNCTION__ << " : using hardcoded default duration";
    if (is_video_) {
      duration = base::TimeDelta::FromMilliseconds(
          kDefaultVideoBufferDurationInMs);
    } else {
      duration = base::TimeDelta::FromMilliseconds(
          kDefaultAudioBufferDurationInMs);
    }
  }

  DCHECK(duration > base::TimeDelta());
  DCHECK(duration != kNoTimestamp());
  return duration;
}

void WebMClusterParser::ClearTextTrackReadyBuffers() {
  text_buffers_map_.clear();
  for (TextTrackMap::iterator it = text_track_map_.begin();
       it != text_track_map_.end();
       ++it) {
    it->second.ClearReadyBuffers();
  }
}

void WebMClusterParser::ResetTextTracks() {
  ClearTextTrackReadyBuffers();
  for (TextTrackMap::iterator it = text_track_map_.begin();
       it != text_track_map_.end();
       ++it) {
    it->second.Reset();
  }
}

void WebMClusterParser::UpdateReadyBuffers() {
  DCHECK(ready_buffer_upper_bound_ == kNoTimestamp());
  DCHECK(text_buffers_map_.empty());

  if (cluster_ended_) {
    audio_.ApplyDurationEstimateIfNeeded();
    video_.ApplyDurationEstimateIfNeeded();
    // Per OnBlock(), all text buffers should already have valid durations, so
    // there is no need to call ApplyDurationEstimateIfNeeded() on text tracks
    // here.
    ready_buffer_upper_bound_ = kInfiniteDuration();
    DCHECK(ready_buffer_upper_bound_ == audio_.GetReadyUpperBound());
    DCHECK(ready_buffer_upper_bound_ == video_.GetReadyUpperBound());
  } else {
    ready_buffer_upper_bound_ = std::min(audio_.GetReadyUpperBound(),
                                         video_.GetReadyUpperBound());
    DCHECK(base::TimeDelta() <= ready_buffer_upper_bound_);
    DCHECK(kNoTimestamp() != ready_buffer_upper_bound_);
  }

  // Prepare each track's ready buffers for retrieval.
  audio_.ExtractReadyBuffers(ready_buffer_upper_bound_);
  video_.ExtractReadyBuffers(ready_buffer_upper_bound_);
  for (TextTrackMap::iterator itr = text_track_map_.begin();
       itr != text_track_map_.end();
       ++itr) {
    itr->second.ExtractReadyBuffers(ready_buffer_upper_bound_);
  }
}

WebMClusterParser::Track*
WebMClusterParser::FindTextTrack(int track_num) {
  const TextTrackMap::iterator it = text_track_map_.find(track_num);

  if (it == text_track_map_.end())
    return NULL;

  return &it->second;
}

}  // namespace media
