// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/media_source_state.h"

#include "base/callback_helpers.h"
#include "base/stl_util.h"
#include "media/base/media_track.h"
#include "media/base/media_tracks.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/frame_processor.h"
#include "media/filters/source_buffer_stream.h"

namespace media {

enum {
  // Limits the number of MEDIA_LOG() calls warning the user that a muxed stream
  // media segment is missing a block from at least one of the audio or video
  // tracks.
  kMaxMissingTrackInSegmentLogs = 10,
};

static TimeDelta EndTimestamp(const StreamParser::BufferQueue& queue) {
  return queue.back()->timestamp() + queue.back()->duration();
}

// List of time ranges for each SourceBuffer.
// static
Ranges<TimeDelta> MediaSourceState::ComputeRangesIntersection(
    const RangesList& activeRanges,
    bool ended) {
  // TODO(servolk): Perhaps this can be removed in favor of blink implementation
  // (MediaSource::buffered)? Currently this is only used on Android and for
  // updating DemuxerHost's buffered ranges during AppendData() as well as
  // SourceBuffer.buffered property implemetation.
  // Implementation of HTMLMediaElement.buffered algorithm in MSE spec.
  // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#dom-htmlmediaelement.buffered

  // Step 1: If activeSourceBuffers.length equals 0 then return an empty
  //  TimeRanges object and abort these steps.
  if (activeRanges.empty())
    return Ranges<TimeDelta>();

  // Step 2: Let active ranges be the ranges returned by buffered for each
  //  SourceBuffer object in activeSourceBuffers.
  // Step 3: Let highest end time be the largest range end time in the active
  //  ranges.
  TimeDelta highest_end_time;
  for (RangesList::const_iterator itr = activeRanges.begin();
       itr != activeRanges.end(); ++itr) {
    if (!itr->size())
      continue;

    highest_end_time = std::max(highest_end_time, itr->end(itr->size() - 1));
  }

  // Step 4: Let intersection ranges equal a TimeRange object containing a
  //  single range from 0 to highest end time.
  Ranges<TimeDelta> intersection_ranges;
  intersection_ranges.Add(TimeDelta(), highest_end_time);

  // Step 5: For each SourceBuffer object in activeSourceBuffers run the
  //  following steps:
  for (RangesList::const_iterator itr = activeRanges.begin();
       itr != activeRanges.end(); ++itr) {
    // Step 5.1: Let source ranges equal the ranges returned by the buffered
    //  attribute on the current SourceBuffer.
    Ranges<TimeDelta> source_ranges = *itr;

    // Step 5.2: If readyState is "ended", then set the end time on the last
    //  range in source ranges to highest end time.
    if (ended && source_ranges.size() > 0u) {
      source_ranges.Add(source_ranges.start(source_ranges.size() - 1),
                        highest_end_time);
    }

    // Step 5.3: Let new intersection ranges equal the intersection between
    // the intersection ranges and the source ranges.
    // Step 5.4: Replace the ranges in intersection ranges with the new
    // intersection ranges.
    intersection_ranges = intersection_ranges.IntersectionWith(source_ranges);
  }

  return intersection_ranges;
}

MediaSourceState::MediaSourceState(
    std::unique_ptr<StreamParser> stream_parser,
    std::unique_ptr<FrameProcessor> frame_processor,
    const CreateDemuxerStreamCB& create_demuxer_stream_cb,
    const scoped_refptr<MediaLog>& media_log)
    : create_demuxer_stream_cb_(create_demuxer_stream_cb),
      timestamp_offset_during_append_(NULL),
      parsing_media_segment_(false),
      media_segment_contained_audio_frame_(false),
      media_segment_contained_video_frame_(false),
      stream_parser_(stream_parser.release()),
      audio_(NULL),
      video_(NULL),
      frame_processor_(frame_processor.release()),
      media_log_(media_log),
      state_(UNINITIALIZED),
      auto_update_timestamp_offset_(false) {
  DCHECK(!create_demuxer_stream_cb_.is_null());
  DCHECK(frame_processor_);
}

MediaSourceState::~MediaSourceState() {
  Shutdown();

  STLDeleteValues(&text_stream_map_);
}

void MediaSourceState::Init(
    const StreamParser::InitCB& init_cb,
    bool allow_audio,
    bool allow_video,
    const StreamParser::EncryptedMediaInitDataCB& encrypted_media_init_data_cb,
    const NewTextTrackCB& new_text_track_cb) {
  DCHECK_EQ(state_, UNINITIALIZED);
  new_text_track_cb_ = new_text_track_cb;
  init_cb_ = init_cb;

  state_ = PENDING_PARSER_CONFIG;
  stream_parser_->Init(
      base::Bind(&MediaSourceState::OnSourceInitDone, base::Unretained(this)),
      base::Bind(&MediaSourceState::OnNewConfigs, base::Unretained(this),
                 allow_audio, allow_video),
      base::Bind(&MediaSourceState::OnNewBuffers, base::Unretained(this)),
      new_text_track_cb_.is_null(), encrypted_media_init_data_cb,
      base::Bind(&MediaSourceState::OnNewMediaSegment, base::Unretained(this)),
      base::Bind(&MediaSourceState::OnEndOfMediaSegment,
                 base::Unretained(this)),
      media_log_);
}

void MediaSourceState::SetSequenceMode(bool sequence_mode) {
  DCHECK(!parsing_media_segment_);

  frame_processor_->SetSequenceMode(sequence_mode);
}

void MediaSourceState::SetGroupStartTimestampIfInSequenceMode(
    base::TimeDelta timestamp_offset) {
  DCHECK(!parsing_media_segment_);

  frame_processor_->SetGroupStartTimestampIfInSequenceMode(timestamp_offset);
}

void MediaSourceState::SetTracksWatcher(
    const Demuxer::MediaTracksUpdatedCB& tracks_updated_cb) {
  DCHECK(init_segment_received_cb_.is_null());
  DCHECK(!tracks_updated_cb.is_null());
  init_segment_received_cb_ = tracks_updated_cb;
}

bool MediaSourceState::Append(const uint8_t* data,
                              size_t length,
                              TimeDelta append_window_start,
                              TimeDelta append_window_end,
                              TimeDelta* timestamp_offset) {
  append_in_progress_ = true;
  DCHECK(timestamp_offset);
  DCHECK(!timestamp_offset_during_append_);
  append_window_start_during_append_ = append_window_start;
  append_window_end_during_append_ = append_window_end;
  timestamp_offset_during_append_ = timestamp_offset;

  // TODO(wolenetz/acolwell): Curry and pass a NewBuffersCB here bound with
  // append window and timestamp offset pointer. See http://crbug.com/351454.
  bool result = stream_parser_->Parse(data, length);
  if (!result) {
    MEDIA_LOG(ERROR, media_log_)
        << __FUNCTION__ << ": stream parsing failed."
        << " Data size=" << length
        << " append_window_start=" << append_window_start.InSecondsF()
        << " append_window_end=" << append_window_end.InSecondsF();
  }
  timestamp_offset_during_append_ = NULL;
  append_in_progress_ = false;
  return result;
}

void MediaSourceState::ResetParserState(TimeDelta append_window_start,
                                        TimeDelta append_window_end,
                                        base::TimeDelta* timestamp_offset) {
  DCHECK(timestamp_offset);
  DCHECK(!timestamp_offset_during_append_);
  timestamp_offset_during_append_ = timestamp_offset;
  append_window_start_during_append_ = append_window_start;
  append_window_end_during_append_ = append_window_end;

  stream_parser_->Flush();
  timestamp_offset_during_append_ = NULL;

  frame_processor_->Reset();
  parsing_media_segment_ = false;
  media_segment_contained_audio_frame_ = false;
  media_segment_contained_video_frame_ = false;
}

void MediaSourceState::Remove(TimeDelta start,
                              TimeDelta end,
                              TimeDelta duration) {
  if (audio_)
    audio_->Remove(start, end, duration);

  if (video_)
    video_->Remove(start, end, duration);

  for (TextStreamMap::iterator itr = text_stream_map_.begin();
       itr != text_stream_map_.end(); ++itr) {
    itr->second->Remove(start, end, duration);
  }
}

size_t MediaSourceState::EstimateVideoDataSize(
    size_t muxed_data_chunk_size) const {
  DCHECK(audio_);
  DCHECK(video_);

  size_t videoBufferedSize = video_->GetBufferedSize();
  size_t audioBufferedSize = audio_->GetBufferedSize();
  if (videoBufferedSize == 0 || audioBufferedSize == 0) {
    // At this point either audio or video buffer is empty, which means buffer
    // levels are probably low anyway and we should have enough space in the
    // buffers for appending new data, so just take a very rough guess.
    return muxed_data_chunk_size * 7 / 8;
  }

  // We need to estimate how much audio and video data is going to be in the
  // newly appended data chunk to make space for the new data. And we need to do
  // that without parsing the data (which will happen later, in the Append
  // phase). So for now we can only rely on some heuristic here. Let's assume
  // that the proportion of the audio/video in the new data chunk is the same as
  // the current ratio of buffered audio/video.
  // Longer term this should go away once we further change the MSE GC algorithm
  // to work across all streams of a SourceBuffer (see crbug.com/520704).
  double videoBufferedSizeF = static_cast<double>(videoBufferedSize);
  double audioBufferedSizeF = static_cast<double>(audioBufferedSize);

  double totalBufferedSizeF = videoBufferedSizeF + audioBufferedSizeF;
  CHECK_GT(totalBufferedSizeF, 0.0);

  double videoRatio = videoBufferedSizeF / totalBufferedSizeF;
  CHECK_GE(videoRatio, 0.0);
  CHECK_LE(videoRatio, 1.0);
  double estimatedVideoSize = muxed_data_chunk_size * videoRatio;
  return static_cast<size_t>(estimatedVideoSize);
}

bool MediaSourceState::EvictCodedFrames(DecodeTimestamp media_time,
                                        size_t newDataSize) {
  bool success = true;

  DVLOG(3) << __FUNCTION__ << " media_time=" << media_time.InSecondsF()
           << " newDataSize=" << newDataSize
           << " videoBufferedSize=" << (video_ ? video_->GetBufferedSize() : 0)
           << " audioBufferedSize=" << (audio_ ? audio_->GetBufferedSize() : 0);

  size_t newAudioSize = 0;
  size_t newVideoSize = 0;
  if (audio_ && video_) {
    newVideoSize = EstimateVideoDataSize(newDataSize);
    newAudioSize = newDataSize - newVideoSize;
  } else if (video_) {
    newVideoSize = newDataSize;
  } else if (audio_) {
    newAudioSize = newDataSize;
  }

  DVLOG(3) << __FUNCTION__ << " estimated audio/video sizes: "
           << " newVideoSize=" << newVideoSize
           << " newAudioSize=" << newAudioSize;

  if (audio_)
    success = audio_->EvictCodedFrames(media_time, newAudioSize) && success;

  if (video_)
    success = video_->EvictCodedFrames(media_time, newVideoSize) && success;

  for (TextStreamMap::iterator itr = text_stream_map_.begin();
       itr != text_stream_map_.end(); ++itr) {
    success = itr->second->EvictCodedFrames(media_time, 0) && success;
  }

  DVLOG(3) << __FUNCTION__ << " result=" << success
           << " videoBufferedSize=" << (video_ ? video_->GetBufferedSize() : 0)
           << " audioBufferedSize=" << (audio_ ? audio_->GetBufferedSize() : 0);

  return success;
}

Ranges<TimeDelta> MediaSourceState::GetBufferedRanges(TimeDelta duration,
                                                      bool ended) const {
  // TODO(acolwell): When we start allowing disabled tracks we'll need to update
  // this code to only add ranges from active tracks.
  RangesList ranges_list;
  if (audio_)
    ranges_list.push_back(audio_->GetBufferedRanges(duration));

  if (video_)
    ranges_list.push_back(video_->GetBufferedRanges(duration));

  for (TextStreamMap::const_iterator itr = text_stream_map_.begin();
       itr != text_stream_map_.end(); ++itr) {
    ranges_list.push_back(itr->second->GetBufferedRanges(duration));
  }

  return ComputeRangesIntersection(ranges_list, ended);
}

TimeDelta MediaSourceState::GetHighestPresentationTimestamp() const {
  TimeDelta max_pts;

  if (audio_)
    max_pts = std::max(max_pts, audio_->GetHighestPresentationTimestamp());

  if (video_)
    max_pts = std::max(max_pts, video_->GetHighestPresentationTimestamp());

  for (TextStreamMap::const_iterator itr = text_stream_map_.begin();
       itr != text_stream_map_.end(); ++itr) {
    max_pts = std::max(max_pts, itr->second->GetHighestPresentationTimestamp());
  }

  return max_pts;
}

TimeDelta MediaSourceState::GetMaxBufferedDuration() const {
  TimeDelta max_duration;

  if (audio_)
    max_duration = std::max(max_duration, audio_->GetBufferedDuration());

  if (video_)
    max_duration = std::max(max_duration, video_->GetBufferedDuration());

  for (TextStreamMap::const_iterator itr = text_stream_map_.begin();
       itr != text_stream_map_.end(); ++itr) {
    max_duration = std::max(max_duration, itr->second->GetBufferedDuration());
  }

  return max_duration;
}

void MediaSourceState::StartReturningData() {
  if (audio_)
    audio_->StartReturningData();

  if (video_)
    video_->StartReturningData();

  for (TextStreamMap::iterator itr = text_stream_map_.begin();
       itr != text_stream_map_.end(); ++itr) {
    itr->second->StartReturningData();
  }
}

void MediaSourceState::AbortReads() {
  if (audio_)
    audio_->AbortReads();

  if (video_)
    video_->AbortReads();

  for (TextStreamMap::iterator itr = text_stream_map_.begin();
       itr != text_stream_map_.end(); ++itr) {
    itr->second->AbortReads();
  }
}

void MediaSourceState::Seek(TimeDelta seek_time) {
  if (audio_)
    audio_->Seek(seek_time);

  if (video_)
    video_->Seek(seek_time);

  for (TextStreamMap::iterator itr = text_stream_map_.begin();
       itr != text_stream_map_.end(); ++itr) {
    itr->second->Seek(seek_time);
  }
}

void MediaSourceState::CompletePendingReadIfPossible() {
  if (audio_)
    audio_->CompletePendingReadIfPossible();

  if (video_)
    video_->CompletePendingReadIfPossible();

  for (TextStreamMap::iterator itr = text_stream_map_.begin();
       itr != text_stream_map_.end(); ++itr) {
    itr->second->CompletePendingReadIfPossible();
  }
}

void MediaSourceState::OnSetDuration(TimeDelta duration) {
  if (audio_)
    audio_->OnSetDuration(duration);

  if (video_)
    video_->OnSetDuration(duration);

  for (TextStreamMap::iterator itr = text_stream_map_.begin();
       itr != text_stream_map_.end(); ++itr) {
    itr->second->OnSetDuration(duration);
  }
}

void MediaSourceState::MarkEndOfStream() {
  if (audio_)
    audio_->MarkEndOfStream();

  if (video_)
    video_->MarkEndOfStream();

  for (TextStreamMap::iterator itr = text_stream_map_.begin();
       itr != text_stream_map_.end(); ++itr) {
    itr->second->MarkEndOfStream();
  }
}

void MediaSourceState::UnmarkEndOfStream() {
  if (audio_)
    audio_->UnmarkEndOfStream();

  if (video_)
    video_->UnmarkEndOfStream();

  for (TextStreamMap::iterator itr = text_stream_map_.begin();
       itr != text_stream_map_.end(); ++itr) {
    itr->second->UnmarkEndOfStream();
  }
}

void MediaSourceState::Shutdown() {
  if (audio_)
    audio_->Shutdown();

  if (video_)
    video_->Shutdown();

  for (TextStreamMap::iterator itr = text_stream_map_.begin();
       itr != text_stream_map_.end(); ++itr) {
    itr->second->Shutdown();
  }
}

void MediaSourceState::SetMemoryLimits(DemuxerStream::Type type,
                                       size_t memory_limit) {
  switch (type) {
    case DemuxerStream::AUDIO:
      if (audio_)
        audio_->SetStreamMemoryLimit(memory_limit);
      break;
    case DemuxerStream::VIDEO:
      if (video_)
        video_->SetStreamMemoryLimit(memory_limit);
      break;
    case DemuxerStream::TEXT:
      for (TextStreamMap::iterator itr = text_stream_map_.begin();
           itr != text_stream_map_.end(); ++itr) {
        itr->second->SetStreamMemoryLimit(memory_limit);
      }
      break;
    case DemuxerStream::UNKNOWN:
    case DemuxerStream::NUM_TYPES:
      NOTREACHED();
      break;
  }
}

bool MediaSourceState::IsSeekWaitingForData() const {
  if (audio_ && audio_->IsSeekWaitingForData())
    return true;

  if (video_ && video_->IsSeekWaitingForData())
    return true;

  // NOTE: We are intentionally not checking the text tracks
  // because text tracks are discontinuous and may not have data
  // for the seek position. This is ok and playback should not be
  // stalled because we don't have cues. If cues, with timestamps after
  // the seek time, eventually arrive they will be delivered properly
  // in response to ChunkDemuxerStream::Read() calls.

  return false;
}

bool MediaSourceState::OnNewConfigs(
    bool allow_audio,
    bool allow_video,
    std::unique_ptr<MediaTracks> tracks,
    const StreamParser::TextTrackConfigMap& text_configs) {
  DCHECK_GE(state_, PENDING_PARSER_CONFIG);
  DCHECK(tracks.get());

  MediaTrack* audio_track = nullptr;
  MediaTrack* video_track = nullptr;
  AudioDecoderConfig audio_config;
  VideoDecoderConfig video_config;
  for (const auto& track : tracks->tracks()) {
    const auto& track_id = track->bytestream_track_id();

    if (track->type() == MediaTrack::Audio) {
      if (audio_track) {
        MEDIA_LOG(ERROR, media_log_)
            << "Error: more than one audio track is currently not supported.";
        return false;
      }
      audio_track = track.get();
      audio_config = tracks->getAudioConfig(track_id);
      DCHECK(audio_config.IsValidConfig());
    } else if (track->type() == MediaTrack::Video) {
      if (video_track) {
        MEDIA_LOG(ERROR, media_log_)
            << "Error: more than one video track is currently not supported.";
        return false;
      }
      video_track = track.get();
      video_config = tracks->getVideoConfig(track_id);
      DCHECK(video_config.IsValidConfig());
    } else {
      MEDIA_LOG(ERROR, media_log_) << "Error: unsupported media track type "
                                   << track->type();
      return false;
    }
  }

  DVLOG(1) << "OnNewConfigs(" << allow_audio << ", " << allow_video << ", "
           << audio_config.IsValidConfig() << ", "
           << video_config.IsValidConfig() << ")";
  // MSE spec allows new configs to be emitted only during Append, but not
  // during Flush or parser reset operations.
  CHECK(append_in_progress_);

  if (!audio_config.IsValidConfig() && !video_config.IsValidConfig()) {
    DVLOG(1) << "OnNewConfigs() : Audio & video config are not valid!";
    return false;
  }

  // Signal an error if we get configuration info for stream types that weren't
  // specified in AddId() or more configs after a stream is initialized.
  if (allow_audio != audio_config.IsValidConfig()) {
    MEDIA_LOG(ERROR, media_log_)
        << "Initialization segment"
        << (audio_config.IsValidConfig() ? " has" : " does not have")
        << " an audio track, but the mimetype"
        << (allow_audio ? " specifies" : " does not specify")
        << " an audio codec.";
    return false;
  }

  if (allow_video != video_config.IsValidConfig()) {
    MEDIA_LOG(ERROR, media_log_)
        << "Initialization segment"
        << (video_config.IsValidConfig() ? " has" : " does not have")
        << " a video track, but the mimetype"
        << (allow_video ? " specifies" : " does not specify")
        << " a video codec.";
    return false;
  }

  bool success = true;
  if (audio_config.IsValidConfig()) {
    if (!audio_) {
      media_log_->SetBooleanProperty("found_audio_stream", true);
    }
    if (!audio_ ||
        audio_->audio_decoder_config().codec() != audio_config.codec()) {
      media_log_->SetStringProperty("audio_codec_name",
                                    GetCodecName(audio_config.codec()));
    }

    if (!audio_) {
      audio_ = create_demuxer_stream_cb_.Run(DemuxerStream::AUDIO);

      if (!audio_) {
        DVLOG(1) << "Failed to create an audio stream.";
        return false;
      }

      if (!frame_processor_->AddTrack(FrameProcessor::kAudioTrackId, audio_)) {
        DVLOG(1) << "Failed to add audio track to frame processor.";
        return false;
      }
    }

    frame_processor_->OnPossibleAudioConfigUpdate(audio_config);
    success &= audio_->UpdateAudioConfig(audio_config, media_log_);
  }

  if (video_config.IsValidConfig()) {
    if (!video_) {
      media_log_->SetBooleanProperty("found_video_stream", true);
    }
    if (!video_ ||
        video_->video_decoder_config().codec() != video_config.codec()) {
      media_log_->SetStringProperty("video_codec_name",
                                    GetCodecName(video_config.codec()));
    }

    if (!video_) {
      video_ = create_demuxer_stream_cb_.Run(DemuxerStream::VIDEO);

      if (!video_) {
        DVLOG(1) << "Failed to create a video stream.";
        return false;
      }

      if (!frame_processor_->AddTrack(FrameProcessor::kVideoTrackId, video_)) {
        DVLOG(1) << "Failed to add video track to frame processor.";
        return false;
      }
    }

    success &= video_->UpdateVideoConfig(video_config, media_log_);
  }

  typedef StreamParser::TextTrackConfigMap::const_iterator TextConfigItr;
  if (text_stream_map_.empty()) {
    for (TextConfigItr itr = text_configs.begin(); itr != text_configs.end();
         ++itr) {
      ChunkDemuxerStream* const text_stream =
          create_demuxer_stream_cb_.Run(DemuxerStream::TEXT);
      if (!frame_processor_->AddTrack(itr->first, text_stream)) {
        success &= false;
        MEDIA_LOG(ERROR, media_log_) << "Failed to add text track ID "
                                     << itr->first << " to frame processor.";
        break;
      }
      text_stream->UpdateTextConfig(itr->second, media_log_);
      text_stream_map_[itr->first] = text_stream;
      new_text_track_cb_.Run(text_stream, itr->second);
    }
  } else {
    const size_t text_count = text_stream_map_.size();
    if (text_configs.size() != text_count) {
      success &= false;
      MEDIA_LOG(ERROR, media_log_)
          << "The number of text track configs changed.";
    } else if (text_count == 1) {
      TextConfigItr config_itr = text_configs.begin();
      TextStreamMap::iterator stream_itr = text_stream_map_.begin();
      ChunkDemuxerStream* text_stream = stream_itr->second;
      TextTrackConfig old_config = text_stream->text_track_config();
      TextTrackConfig new_config(
          config_itr->second.kind(), config_itr->second.label(),
          config_itr->second.language(), old_config.id());
      if (!new_config.Matches(old_config)) {
        success &= false;
        MEDIA_LOG(ERROR, media_log_)
            << "New text track config does not match old one.";
      } else {
        StreamParser::TrackId old_id = stream_itr->first;
        StreamParser::TrackId new_id = config_itr->first;
        if (new_id != old_id) {
          if (frame_processor_->UpdateTrack(old_id, new_id)) {
            text_stream_map_.clear();
            text_stream_map_[config_itr->first] = text_stream;
          } else {
            success &= false;
            MEDIA_LOG(ERROR, media_log_)
                << "Error remapping single text track number";
          }
        }
      }
    } else {
      for (TextConfigItr config_itr = text_configs.begin();
           config_itr != text_configs.end(); ++config_itr) {
        TextStreamMap::iterator stream_itr =
            text_stream_map_.find(config_itr->first);
        if (stream_itr == text_stream_map_.end()) {
          success &= false;
          MEDIA_LOG(ERROR, media_log_)
              << "Unexpected text track configuration for track ID "
              << config_itr->first;
          break;
        }

        const TextTrackConfig& new_config = config_itr->second;
        ChunkDemuxerStream* stream = stream_itr->second;
        TextTrackConfig old_config = stream->text_track_config();
        if (!new_config.Matches(old_config)) {
          success &= false;
          MEDIA_LOG(ERROR, media_log_) << "New text track config for track ID "
                                       << config_itr->first
                                       << " does not match old one.";
          break;
        }
      }
    }
  }

  frame_processor_->SetAllTrackBuffersNeedRandomAccessPoint();

  if (audio_track) {
    DCHECK(audio_);
    audio_track->set_id(audio_->media_track_id());
  }
  if (video_track) {
    DCHECK(video_);
    video_track->set_id(video_->media_track_id());
  }

  DVLOG(1) << "OnNewConfigs() : " << (success ? "success" : "failed");
  if (success) {
    if (state_ == PENDING_PARSER_CONFIG)
      state_ = PENDING_PARSER_INIT;
    DCHECK(!init_segment_received_cb_.is_null());
    init_segment_received_cb_.Run(std::move(tracks));
  }

  return success;
}

void MediaSourceState::OnNewMediaSegment() {
  DVLOG(2) << "OnNewMediaSegment()";
  DCHECK_EQ(state_, PARSER_INITIALIZED);
  parsing_media_segment_ = true;
  media_segment_contained_audio_frame_ = false;
  media_segment_contained_video_frame_ = false;
}

void MediaSourceState::OnEndOfMediaSegment() {
  DVLOG(2) << "OnEndOfMediaSegment()";
  DCHECK_EQ(state_, PARSER_INITIALIZED);
  parsing_media_segment_ = false;

  const bool missing_audio = audio_ && !media_segment_contained_audio_frame_;
  const bool missing_video = video_ && !media_segment_contained_video_frame_;
  if (!missing_audio && !missing_video)
    return;

  LIMITED_MEDIA_LOG(DEBUG, media_log_, num_missing_track_logs_,
                    kMaxMissingTrackInSegmentLogs)
      << "Media segment did not contain any "
      << (missing_audio && missing_video ? "audio or video"
                                         : missing_audio ? "audio" : "video")
      << " coded frames, mismatching initialization segment. Therefore, MSE "
         "coded frame processing may not interoperably detect discontinuities "
         "in appended media.";
}

bool MediaSourceState::OnNewBuffers(
    const StreamParser::BufferQueue& audio_buffers,
    const StreamParser::BufferQueue& video_buffers,
    const StreamParser::TextBufferQueueMap& text_map) {
  DVLOG(2) << "OnNewBuffers()";
  DCHECK_EQ(state_, PARSER_INITIALIZED);
  DCHECK(timestamp_offset_during_append_);
  DCHECK(parsing_media_segment_);

  media_segment_contained_audio_frame_ |= !audio_buffers.empty();
  media_segment_contained_video_frame_ |= !video_buffers.empty();

  const TimeDelta timestamp_offset_before_processing =
      *timestamp_offset_during_append_;

  // Calculate the new timestamp offset for audio/video tracks if the stream
  // parser has requested automatic updates.
  TimeDelta new_timestamp_offset = timestamp_offset_before_processing;
  if (auto_update_timestamp_offset_) {
    const bool have_audio_buffers = !audio_buffers.empty();
    const bool have_video_buffers = !video_buffers.empty();
    if (have_audio_buffers && have_video_buffers) {
      new_timestamp_offset +=
          std::min(EndTimestamp(audio_buffers), EndTimestamp(video_buffers));
    } else if (have_audio_buffers) {
      new_timestamp_offset += EndTimestamp(audio_buffers);
    } else if (have_video_buffers) {
      new_timestamp_offset += EndTimestamp(video_buffers);
    }
  }

  if (!frame_processor_->ProcessFrames(audio_buffers, video_buffers, text_map,
                                       append_window_start_during_append_,
                                       append_window_end_during_append_,
                                       timestamp_offset_during_append_)) {
    return false;
  }

  // Only update the timestamp offset if the frame processor hasn't already.
  if (auto_update_timestamp_offset_ &&
      timestamp_offset_before_processing == *timestamp_offset_during_append_) {
    *timestamp_offset_during_append_ = new_timestamp_offset;
  }

  return true;
}

void MediaSourceState::OnSourceInitDone(
    const StreamParser::InitParameters& params) {
  DCHECK_EQ(state_, PENDING_PARSER_INIT);
  state_ = PARSER_INITIALIZED;
  auto_update_timestamp_offset_ = params.auto_update_timestamp_offset;
  base::ResetAndReturn(&init_cb_).Run(params);
}

}  // namespace media
