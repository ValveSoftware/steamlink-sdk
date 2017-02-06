// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp2t/mp2t_stream_parser.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_log.h"
#include "media/base/media_track.h"
#include "media/base/media_tracks.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/test_data_util.h"
#include "media/base/text_track_config.h"
#include "media/base/video_decoder_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace mp2t {

namespace {

bool IsMonotonic(const StreamParser::BufferQueue& buffers) {
  if (buffers.empty())
    return true;

  StreamParser::BufferQueue::const_iterator it1 = buffers.begin();
  StreamParser::BufferQueue::const_iterator it2 = ++it1;
  for ( ; it2 != buffers.end(); ++it1, ++it2) {
    if ((*it2)->GetDecodeTimestamp() < (*it1)->GetDecodeTimestamp())
      return false;
  }
  return true;
}

bool IsAlmostEqual(DecodeTimestamp t0, DecodeTimestamp t1) {
  base::TimeDelta kMaxDeviation = base::TimeDelta::FromMilliseconds(5);
  base::TimeDelta diff = t1 - t0;
  return (diff >= -kMaxDeviation && diff <= kMaxDeviation);
}

}  // namespace

class Mp2tStreamParserTest : public testing::Test {
 public:
  Mp2tStreamParserTest()
      : segment_count_(0),
        config_count_(0),
        audio_frame_count_(0),
        video_frame_count_(0),
        has_video_(true),
        audio_min_dts_(kNoDecodeTimestamp()),
        audio_max_dts_(kNoDecodeTimestamp()),
        video_min_dts_(kNoDecodeTimestamp()),
        video_max_dts_(kNoDecodeTimestamp()) {
    bool has_sbr = false;
    parser_.reset(new Mp2tStreamParser(has_sbr));
  }

 protected:
  std::unique_ptr<Mp2tStreamParser> parser_;
  int segment_count_;
  int config_count_;
  int audio_frame_count_;
  int video_frame_count_;
  bool has_video_;
  DecodeTimestamp audio_min_dts_;
  DecodeTimestamp audio_max_dts_;
  DecodeTimestamp video_min_dts_;
  DecodeTimestamp video_max_dts_;

  void ResetStats() {
    segment_count_ = 0;
    config_count_ = 0;
    audio_frame_count_ = 0;
    video_frame_count_ = 0;
    audio_min_dts_ = kNoDecodeTimestamp();
    audio_max_dts_ = kNoDecodeTimestamp();
    video_min_dts_ = kNoDecodeTimestamp();
    video_max_dts_ = kNoDecodeTimestamp();
  }

  bool AppendData(const uint8_t* data, size_t length) {
    return parser_->Parse(data, length);
  }

  bool AppendDataInPieces(const uint8_t* data,
                          size_t length,
                          size_t piece_size) {
    const uint8_t* start = data;
    const uint8_t* end = data + length;
    while (start < end) {
      size_t append_size = std::min(piece_size,
                                    static_cast<size_t>(end - start));
      if (!AppendData(start, append_size))
        return false;
      start += append_size;
    }
    return true;
  }

  void OnInit(const StreamParser::InitParameters& params) {
    DVLOG(1) << "OnInit: dur=" << params.duration.InMilliseconds()
             << ", autoTimestampOffset=" << params.auto_update_timestamp_offset;
  }

  bool OnNewConfig(std::unique_ptr<MediaTracks> tracks,
                   const StreamParser::TextTrackConfigMap& tc) {
    DVLOG(1) << "OnNewConfig: got " << tracks->tracks().size() << " tracks";
    bool found_audio_track = false;
    bool found_video_track = false;
    for (const auto& track : tracks->tracks()) {
      const auto& track_id = track->bytestream_track_id();
      if (track->type() == MediaTrack::Audio) {
        found_audio_track = true;
        EXPECT_TRUE(tracks->getAudioConfig(track_id).IsValidConfig());
      } else if (track->type() == MediaTrack::Video) {
        found_video_track = true;
        EXPECT_TRUE(tracks->getVideoConfig(track_id).IsValidConfig());
      } else {
        // Unexpected track type.
        LOG(ERROR) << "Unexpected track type " << track->type();
        EXPECT_TRUE(false);
      }
    }
    EXPECT_TRUE(found_audio_track);
    EXPECT_EQ(has_video_, found_video_track);
    config_count_++;
    return true;
  }


  void DumpBuffers(const std::string& label,
                   const StreamParser::BufferQueue& buffers) {
    DVLOG(2) << "DumpBuffers: " << label << " size " << buffers.size();
    for (StreamParser::BufferQueue::const_iterator buf = buffers.begin();
         buf != buffers.end(); buf++) {
      DVLOG(3) << "  n=" << buf - buffers.begin()
               << ", size=" << (*buf)->data_size()
               << ", dur=" << (*buf)->duration().InMilliseconds();
    }
  }

  bool OnNewBuffers(const StreamParser::BufferQueue& audio_buffers,
                    const StreamParser::BufferQueue& video_buffers,
                    const StreamParser::TextBufferQueueMap& text_map) {
    EXPECT_GT(config_count_, 0);
    DumpBuffers("audio_buffers", audio_buffers);
    DumpBuffers("video_buffers", video_buffers);

    // TODO(wolenetz/acolwell): Add text track support to more MSE parsers. See
    // http://crbug.com/336926.
    if (!text_map.empty())
      return false;

    // Verify monotonicity.
    if (!IsMonotonic(video_buffers))
      return false;
    if (!IsMonotonic(audio_buffers))
      return false;

    if (!video_buffers.empty()) {
      DecodeTimestamp first_dts = video_buffers.front()->GetDecodeTimestamp();
      DecodeTimestamp last_dts = video_buffers.back()->GetDecodeTimestamp();
      if (video_max_dts_ != kNoDecodeTimestamp() && first_dts < video_max_dts_)
        return false;
      if (video_min_dts_ == kNoDecodeTimestamp())
        video_min_dts_ = first_dts;
      video_max_dts_ = last_dts;
    }
    if (!audio_buffers.empty()) {
      DecodeTimestamp first_dts = audio_buffers.front()->GetDecodeTimestamp();
      DecodeTimestamp last_dts = audio_buffers.back()->GetDecodeTimestamp();
      if (audio_max_dts_ != kNoDecodeTimestamp() && first_dts < audio_max_dts_)
        return false;
      if (audio_min_dts_ == kNoDecodeTimestamp())
        audio_min_dts_ = first_dts;
      audio_max_dts_ = last_dts;
    }

    audio_frame_count_ += audio_buffers.size();
    video_frame_count_ += video_buffers.size();
    return true;
  }

  void OnKeyNeeded(EmeInitDataType type,
                   const std::vector<uint8_t>& init_data) {
    LOG(ERROR) << "OnKeyNeeded not expected in the Mpeg2 TS parser";
    EXPECT_TRUE(false);
  }

  void OnNewSegment() {
    DVLOG(1) << "OnNewSegment";
    segment_count_++;
  }

  void OnEndOfSegment() {
    LOG(ERROR) << "OnEndOfSegment not expected in the Mpeg2 TS parser";
    EXPECT_TRUE(false);
  }

  void InitializeParser() {
    parser_->Init(
        base::Bind(&Mp2tStreamParserTest::OnInit, base::Unretained(this)),
        base::Bind(&Mp2tStreamParserTest::OnNewConfig, base::Unretained(this)),
        base::Bind(&Mp2tStreamParserTest::OnNewBuffers, base::Unretained(this)),
        true,
        base::Bind(&Mp2tStreamParserTest::OnKeyNeeded, base::Unretained(this)),
        base::Bind(&Mp2tStreamParserTest::OnNewSegment, base::Unretained(this)),
        base::Bind(&Mp2tStreamParserTest::OnEndOfSegment,
                   base::Unretained(this)),
        new MediaLog());
  }

  bool ParseMpeg2TsFile(const std::string& filename, int append_bytes) {
    scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile(filename);
    EXPECT_TRUE(AppendDataInPieces(buffer->data(),
                                   buffer->data_size(),
                                   append_bytes));
    return true;
  }
};

TEST_F(Mp2tStreamParserTest, UnalignedAppend17) {
  // Test small, non-segment-aligned appends.
  InitializeParser();
  ParseMpeg2TsFile("bear-1280x720.ts", 17);
  parser_->Flush();
  EXPECT_EQ(video_frame_count_, 82);
  // This stream has no mid-stream configuration change.
  EXPECT_EQ(config_count_, 1);
  EXPECT_EQ(segment_count_, 1);
}

TEST_F(Mp2tStreamParserTest, UnalignedAppend512) {
  // Test small, non-segment-aligned appends.
  InitializeParser();
  ParseMpeg2TsFile("bear-1280x720.ts", 512);
  parser_->Flush();
  EXPECT_EQ(video_frame_count_, 82);
  // This stream has no mid-stream configuration change.
  EXPECT_EQ(config_count_, 1);
  EXPECT_EQ(segment_count_, 1);
}

TEST_F(Mp2tStreamParserTest, AppendAfterFlush512) {
  InitializeParser();
  ParseMpeg2TsFile("bear-1280x720.ts", 512);
  parser_->Flush();
  EXPECT_EQ(video_frame_count_, 82);
  EXPECT_EQ(config_count_, 1);
  EXPECT_EQ(segment_count_, 1);

  ResetStats();
  ParseMpeg2TsFile("bear-1280x720.ts", 512);
  parser_->Flush();
  EXPECT_EQ(video_frame_count_, 82);
  EXPECT_EQ(config_count_, 1);
  EXPECT_EQ(segment_count_, 1);
}

TEST_F(Mp2tStreamParserTest, TimestampWrapAround) {
  // "bear-1280x720_ptswraparound.ts" has been transcoded
  // from bear-1280x720.mp4 by applying a time offset of 95442s
  // (close to 2^33 / 90000) which results in timestamps wrap around
  // in the Mpeg2 TS stream.
  InitializeParser();
  ParseMpeg2TsFile("bear-1280x720_ptswraparound.ts", 512);
  parser_->Flush();
  EXPECT_EQ(video_frame_count_, 82);

  EXPECT_TRUE(IsAlmostEqual(video_min_dts_,
                            DecodeTimestamp::FromSecondsD(95443.376)));
  EXPECT_TRUE(IsAlmostEqual(video_max_dts_,
                            DecodeTimestamp::FromSecondsD(95446.079)));

  // Note: for audio, AdtsStreamParser considers only the PTS (which is then
  // used as the DTS).
  // TODO(damienv): most of the time, audio streams just have PTS. Here, only
  // the first PES packet has a DTS, all the other PES packets have PTS only.
  // Reconsider the expected value for |audio_min_dts_| if DTS are used as part
  // of the ADTS stream parser.
  //
  // Note: the last pts for audio is 95445.931 but this PES packet includes
  // 9 ADTS frames with 1 AAC frame in each ADTS frame.
  // So the PTS of the last AAC frame is:
  // 95445.931 + 8 * (1024 / 44100) = 95446.117
  EXPECT_TRUE(IsAlmostEqual(audio_min_dts_,
                            DecodeTimestamp::FromSecondsD(95443.400)));
  EXPECT_TRUE(IsAlmostEqual(audio_max_dts_,
                            DecodeTimestamp::FromSecondsD(95446.117)));
}

TEST_F(Mp2tStreamParserTest, AudioInPrivateStream1) {
  // Test small, non-segment-aligned appends.
  InitializeParser();
  has_video_ = false;
  ParseMpeg2TsFile("bear_adts_in_private_stream_1.ts", 512);
  parser_->Flush();
  EXPECT_EQ(audio_frame_count_, 40);
  EXPECT_EQ(video_frame_count_, 0);
  // This stream has no mid-stream configuration change.
  EXPECT_EQ(config_count_, 1);
  EXPECT_EQ(segment_count_, 1);
}

}  // namespace mp2t
}  // namespace media
