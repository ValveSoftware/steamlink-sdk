// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include <stdint.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <memory>
#include <ostream>
#include <string>

#include "gtest/gtest.h"

#include "common/file_util.h"
#include "common/libwebm_util.h"
#include "mkvmuxer/mkvmuxer.h"
#include "mkvmuxer/mkvwriter.h"
#include "mkvparser/mkvreader.h"
#include "testing/test_util.h"

using mkvmuxer::AudioTrack;
using mkvmuxer::Chapter;
using mkvmuxer::Frame;
using mkvmuxer::MkvWriter;
using mkvmuxer::Segment;
using mkvmuxer::SegmentInfo;
using mkvmuxer::Track;
using mkvmuxer::VideoTrack;

namespace test {

// Base class containing boiler plate stuff.
class MuxerTest : public testing::Test {
 public:
  MuxerTest() { Init(); }

  ~MuxerTest() { CloseWriter(); }

  // Simple init function for use by constructor. Calls made here to allow use
  // of ASSERT_* macros-- this is necessary here because all failures in Init()
  // are fatal, but the ASSERT_* gtest macros cannot be used in a constructor.
  void Init() {
    ASSERT_TRUE(GetTestDataDir().length() > 0);
    filename_ = libwebm::GetTempFileName();
    ASSERT_GT(filename_.length(), 0u);
    temp_file_ = libwebm::FilePtr(std::fopen(filename_.c_str(), "wb"),
                                  libwebm::FILEDeleter());
    ASSERT_TRUE(temp_file_.get() != nullptr);
    writer_.reset(new MkvWriter(temp_file_.get()));
    is_writer_open_ = true;
    memset(dummy_data_, 0, kFrameLength);
  }

  void AddDummyFrameAndFinalize(int track_number) {
    EXPECT_TRUE(segment_.AddFrame(&dummy_data_[0], kFrameLength, track_number,
                                  0, false));
    EXPECT_TRUE(segment_.Finalize());
  }

  void AddVideoTrack() {
    const int vid_track = static_cast<int>(
        segment_.AddVideoTrack(kWidth, kHeight, kVideoTrackNumber));
    ASSERT_EQ(kVideoTrackNumber, vid_track);
    VideoTrack* const video =
        dynamic_cast<VideoTrack*>(segment_.GetTrackByNumber(vid_track));
    ASSERT_TRUE(video != NULL);
    video->set_uid(kVideoTrackNumber);
  }

  void AddAudioTrack() {
    const int aud_track = static_cast<int>(
        segment_.AddAudioTrack(kSampleRate, kChannels, kAudioTrackNumber));
    ASSERT_EQ(kAudioTrackNumber, aud_track);
    AudioTrack* const audio =
        dynamic_cast<AudioTrack*>(segment_.GetTrackByNumber(aud_track));
    ASSERT_TRUE(audio != NULL);
    audio->set_uid(kAudioTrackNumber);
    audio->set_codec_id(kOpusCodecId);
  }

  void CloseWriter() {
    if (is_writer_open_)
      writer_->Close();
    is_writer_open_ = false;
  }

  bool SegmentInit(bool output_cues, bool accurate_cluster_duration,
                   bool fixed_size_cluster_timecode) {
    if (!segment_.Init(writer_.get()))
      return false;
    SegmentInfo* const info = segment_.GetSegmentInfo();
    info->set_writing_app(kAppString);
    info->set_muxing_app(kAppString);
    segment_.OutputCues(output_cues);
    segment_.AccurateClusterDuration(accurate_cluster_duration);
    segment_.UseFixedSizeClusterTimecode(fixed_size_cluster_timecode);
    return true;
  }

 protected:
  std::unique_ptr<MkvWriter> writer_;
  bool is_writer_open_ = false;
  Segment segment_;
  std::string filename_;
  libwebm::FilePtr temp_file_;
  std::uint8_t dummy_data_[kFrameLength];
};

TEST_F(MuxerTest, SegmentInfo) {
  EXPECT_TRUE(SegmentInit(false, false, false));
  SegmentInfo* const info = segment_.GetSegmentInfo();
  info->set_timecode_scale(kTimeCodeScale);
  info->set_duration(2.345);
  EXPECT_STREQ(kAppString, info->muxing_app());
  EXPECT_STREQ(kAppString, info->writing_app());
  EXPECT_EQ(static_cast<uint64_t>(kTimeCodeScale), info->timecode_scale());
  EXPECT_DOUBLE_EQ(2.345, info->duration());
  AddVideoTrack();

  AddDummyFrameAndFinalize(kVideoTrackNumber);
  CloseWriter();

  EXPECT_TRUE(CompareFiles(GetTestFilePath("segment_info.webm"), filename_));
}

TEST_F(MuxerTest, AddTracks) {
  EXPECT_TRUE(SegmentInit(false, false, false));

  // Add a Video Track
  AddVideoTrack();
  VideoTrack* const video =
      dynamic_cast<VideoTrack*>(segment_.GetTrackByNumber(kVideoTrackNumber));
  ASSERT_TRUE(video != NULL);
  EXPECT_EQ(static_cast<uint64_t>(kWidth), video->width());
  EXPECT_EQ(static_cast<uint64_t>(kHeight), video->height());
  video->set_name(kTrackName);
  video->set_display_width(kWidth - 10);
  video->set_display_height(kHeight - 10);
  video->set_frame_rate(0.5);
  EXPECT_STREQ(kTrackName, video->name());
  const uint64_t kDisplayWidth = kWidth - 10;
  EXPECT_EQ(kDisplayWidth, video->display_width());
  const uint64_t kDisplayHeight = kHeight - 10;
  EXPECT_EQ(kDisplayHeight, video->display_height());
  EXPECT_DOUBLE_EQ(0.5, video->frame_rate());
  EXPECT_EQ(static_cast<uint64_t>(kVideoTrackNumber), video->uid());

  // Add an Audio Track
  const int aud_track = static_cast<int>(
      segment_.AddAudioTrack(kSampleRate, kChannels, kAudioTrackNumber));
  EXPECT_EQ(kAudioTrackNumber, aud_track);
  AudioTrack* const audio =
      dynamic_cast<AudioTrack*>(segment_.GetTrackByNumber(aud_track));
  EXPECT_EQ(kSampleRate, audio->sample_rate());
  EXPECT_EQ(static_cast<uint64_t>(kChannels), audio->channels());
  ASSERT_TRUE(audio != NULL);
  audio->set_name(kTrackName);
  audio->set_bit_depth(kBitDepth);
  audio->set_uid(kAudioTrackNumber);
  EXPECT_STREQ(kTrackName, audio->name());
  EXPECT_EQ(static_cast<uint64_t>(kBitDepth), audio->bit_depth());
  EXPECT_EQ(static_cast<uint64_t>(kAudioTrackNumber), audio->uid());

  AddDummyFrameAndFinalize(kVideoTrackNumber);
  CloseWriter();

  EXPECT_TRUE(CompareFiles(GetTestFilePath("tracks.webm"), filename_));
}

TEST_F(MuxerTest, AddChapters) {
  EXPECT_TRUE(SegmentInit(false, false, false));
  AddVideoTrack();

  // Add a Chapter
  Chapter* chapter = segment_.AddChapter();
  EXPECT_TRUE(chapter->set_id("unit_test"));
  chapter->set_time(segment_, 0, 1000000000);
  EXPECT_TRUE(chapter->add_string("unit_test", "english", "us"));
  chapter->set_uid(1);

  AddDummyFrameAndFinalize(kVideoTrackNumber);
  CloseWriter();

  EXPECT_TRUE(CompareFiles(GetTestFilePath("chapters.webm"), filename_));
}

TEST_F(MuxerTest, SimpleBlock) {
  EXPECT_TRUE(SegmentInit(false, false, false));
  AddVideoTrack();

  // Valid Frame
  EXPECT_TRUE(segment_.AddFrame(dummy_data_, kFrameLength, kVideoTrackNumber, 0,
                                false));

  // Valid Frame
  EXPECT_TRUE(segment_.AddFrame(dummy_data_, kFrameLength, kVideoTrackNumber,
                                2000000, false));

  // Invalid Frame - Non monotonically increasing timestamp
  EXPECT_FALSE(segment_.AddFrame(dummy_data_, kFrameLength, kVideoTrackNumber,
                                 1, false));

  // Invalid Frame - Null pointer
  EXPECT_FALSE(segment_.AddFrame(NULL, 0, kVideoTrackNumber, 8000000, false));

  // Invalid Frame - Invalid track number
  EXPECT_FALSE(segment_.AddFrame(NULL, 0, kInvalidTrackNumber, 8000000, false));

  segment_.Finalize();
  CloseWriter();

  EXPECT_TRUE(CompareFiles(GetTestFilePath("simple_block.webm"), filename_));
}

TEST_F(MuxerTest, SimpleBlockWithAddGenericFrame) {
  EXPECT_TRUE(SegmentInit(false, false, false));
  AddVideoTrack();

  Frame frame;
  frame.Init(dummy_data_, kFrameLength);
  frame.set_track_number(kVideoTrackNumber);
  frame.set_is_key(false);

  // Valid Frame
  frame.set_timestamp(0);
  EXPECT_TRUE(segment_.AddGenericFrame(&frame));

  // Valid Frame
  frame.set_timestamp(2000000);
  EXPECT_TRUE(segment_.AddGenericFrame(&frame));

  // Invalid Frame - Non monotonically increasing timestamp
  frame.set_timestamp(1);
  EXPECT_FALSE(segment_.AddGenericFrame(&frame));

  // Invalid Frame - Invalid track number
  frame.set_track_number(kInvalidTrackNumber);
  frame.set_timestamp(8000000);
  EXPECT_FALSE(segment_.AddGenericFrame(&frame));

  segment_.Finalize();
  CloseWriter();

  EXPECT_TRUE(CompareFiles(GetTestFilePath("simple_block.webm"), filename_));
}

TEST_F(MuxerTest, MetadataBlock) {
  EXPECT_TRUE(SegmentInit(false, false, false));
  Track* const track = segment_.AddTrack(kMetadataTrackNumber);
  track->set_uid(kMetadataTrackNumber);
  track->set_type(kMetadataTrackType);
  track->set_codec_id(kMetadataCodecId);

  // Valid Frame
  EXPECT_TRUE(segment_.AddMetadata(dummy_data_, kFrameLength,
                                   kMetadataTrackNumber, 0, 2000000));

  // Valid Frame
  EXPECT_TRUE(segment_.AddMetadata(dummy_data_, kFrameLength,
                                   kMetadataTrackNumber, 2000000, 6000000));

  // Invalid Frame - Non monotonically increasing timestamp
  EXPECT_FALSE(segment_.AddMetadata(dummy_data_, kFrameLength,
                                    kMetadataTrackNumber, 1, 2000000));

  // Invalid Frame - Null pointer
  EXPECT_FALSE(segment_.AddMetadata(NULL, 0, kMetadataTrackNumber, 0, 8000000));

  // Invalid Frame - Invalid track number
  EXPECT_FALSE(segment_.AddMetadata(NULL, 0, kInvalidTrackNumber, 0, 8000000));

  segment_.Finalize();
  CloseWriter();

  EXPECT_TRUE(CompareFiles(GetTestFilePath("metadata_block.webm"), filename_));
}

TEST_F(MuxerTest, TrackType) {
  EXPECT_TRUE(SegmentInit(false, false, false));
  Track* const track = segment_.AddTrack(kMetadataTrackNumber);
  track->set_uid(kMetadataTrackNumber);
  track->set_codec_id(kMetadataCodecId);

  // Invalid Frame - Incomplete track information (Track Type not set).
  EXPECT_FALSE(segment_.AddMetadata(dummy_data_, kFrameLength,
                                    kMetadataTrackNumber, 0, 2000000));

  track->set_type(kMetadataTrackType);

  // Valid Frame
  EXPECT_TRUE(segment_.AddMetadata(dummy_data_, kFrameLength,
                                   kMetadataTrackNumber, 0, 2000000));

  segment_.Finalize();
  CloseWriter();
}

TEST_F(MuxerTest, BlockWithAdditional) {
  EXPECT_TRUE(SegmentInit(false, false, false));
  AddVideoTrack();

  // Valid Frame
  EXPECT_TRUE(segment_.AddFrameWithAdditional(dummy_data_, kFrameLength,
                                              dummy_data_, kFrameLength, 1,
                                              kVideoTrackNumber, 0, true));

  // Valid Frame
  EXPECT_TRUE(segment_.AddFrameWithAdditional(
      dummy_data_, kFrameLength, dummy_data_, kFrameLength, 1,
      kVideoTrackNumber, 2000000, false));

  // Invalid Frame - Non monotonically increasing timestamp
  EXPECT_FALSE(segment_.AddFrameWithAdditional(dummy_data_, kFrameLength,
                                               dummy_data_, kFrameLength, 1,
                                               kVideoTrackNumber, 1, false));

  // Invalid Frame - Null frame pointer
  EXPECT_FALSE(
      segment_.AddFrameWithAdditional(NULL, 0, dummy_data_, kFrameLength, 1,
                                      kVideoTrackNumber, 3000000, false));

  // Invalid Frame - Null additional pointer
  EXPECT_FALSE(segment_.AddFrameWithAdditional(dummy_data_, kFrameLength, NULL,
                                               0, 1, kVideoTrackNumber, 4000000,
                                               false));

  // Invalid Frame - Invalid track number
  EXPECT_FALSE(segment_.AddFrameWithAdditional(
      dummy_data_, kFrameLength, dummy_data_, kFrameLength, 1,
      kInvalidTrackNumber, 8000000, false));

  segment_.Finalize();
  CloseWriter();

  EXPECT_TRUE(
      CompareFiles(GetTestFilePath("block_with_additional.webm"), filename_));
}

TEST_F(MuxerTest, BlockAdditionalWithAddGenericFrame) {
  EXPECT_TRUE(SegmentInit(false, false, false));
  AddVideoTrack();

  Frame frame;
  frame.Init(dummy_data_, kFrameLength);
  frame.AddAdditionalData(dummy_data_, kFrameLength, 1);
  frame.set_track_number(kVideoTrackNumber);
  frame.set_is_key(true);

  // Valid Frame
  frame.set_timestamp(0);
  EXPECT_TRUE(segment_.AddGenericFrame(&frame));

  // Valid Frame
  frame.set_timestamp(2000000);
  frame.set_is_key(false);
  EXPECT_TRUE(segment_.AddGenericFrame(&frame));

  // Invalid Frame - Non monotonically increasing timestamp
  frame.set_timestamp(1);
  EXPECT_FALSE(segment_.AddGenericFrame(&frame));

  // Invalid Frame - Invalid track number
  frame.set_track_number(kInvalidTrackNumber);
  frame.set_timestamp(4000000);
  EXPECT_FALSE(segment_.AddGenericFrame(&frame));

  segment_.Finalize();
  CloseWriter();

  EXPECT_TRUE(
      CompareFiles(GetTestFilePath("block_with_additional.webm"), filename_));
}

TEST_F(MuxerTest, SegmentDurationComputation) {
  EXPECT_TRUE(SegmentInit(false, false, false));
  AddVideoTrack();

  Frame frame;
  frame.Init(dummy_data_, kFrameLength);
  frame.set_track_number(kVideoTrackNumber);
  frame.set_timestamp(0);
  frame.set_is_key(false);
  EXPECT_TRUE(segment_.AddGenericFrame(&frame));
  frame.set_timestamp(2000000);
  EXPECT_TRUE(segment_.AddGenericFrame(&frame));
  frame.set_timestamp(4000000);
  EXPECT_TRUE(segment_.AddGenericFrame(&frame));
  frame.set_timestamp(6000000);
  frame.set_duration(2000000);
  EXPECT_TRUE(segment_.AddGenericFrame(&frame));
  segment_.Finalize();

  // SegmentInfo's duration is in timecode scale
  EXPECT_EQ(8, segment_.GetSegmentInfo()->duration());

  CloseWriter();

  EXPECT_TRUE(
      CompareFiles(GetTestFilePath("segment_duration.webm"), filename_));
}

TEST_F(MuxerTest, ForceNewCluster) {
  EXPECT_TRUE(SegmentInit(false, false, false));
  AddVideoTrack();

  EXPECT_TRUE(segment_.AddFrame(dummy_data_, kFrameLength, kVideoTrackNumber, 0,
                                false));
  segment_.ForceNewClusterOnNextFrame();
  EXPECT_TRUE(segment_.AddFrame(dummy_data_, kFrameLength, kVideoTrackNumber,
                                2000000, false));
  EXPECT_TRUE(segment_.AddFrame(dummy_data_, kFrameLength, kVideoTrackNumber,
                                4000000, false));
  segment_.ForceNewClusterOnNextFrame();
  EXPECT_TRUE(segment_.AddFrame(dummy_data_, kFrameLength, kVideoTrackNumber,
                                6000000, false));
  segment_.Finalize();

  CloseWriter();

  EXPECT_TRUE(
      CompareFiles(GetTestFilePath("force_new_cluster.webm"), filename_));
}

TEST_F(MuxerTest, OutputCues) {
  EXPECT_TRUE(SegmentInit(true, false, false));
  AddVideoTrack();

  EXPECT_TRUE(
      segment_.AddFrame(dummy_data_, kFrameLength, kVideoTrackNumber, 0, true));
  EXPECT_TRUE(segment_.AddFrame(dummy_data_, kFrameLength, kVideoTrackNumber,
                                2000000, false));
  EXPECT_TRUE(segment_.AddFrame(dummy_data_, kFrameLength, kVideoTrackNumber,
                                4000000, false));
  EXPECT_TRUE(segment_.AddFrame(dummy_data_, kFrameLength, kVideoTrackNumber,
                                6000000, true));
  EXPECT_TRUE(segment_.AddCuePoint(4000000, kVideoTrackNumber));
  segment_.Finalize();

  CloseWriter();

  EXPECT_TRUE(CompareFiles(GetTestFilePath("output_cues.webm"), filename_));
}

TEST_F(MuxerTest, CuesBeforeClusters) {
  EXPECT_TRUE(SegmentInit(true, false, false));
  AddVideoTrack();

  EXPECT_TRUE(
      segment_.AddFrame(dummy_data_, kFrameLength, kVideoTrackNumber, 0, true));
  EXPECT_TRUE(segment_.AddFrame(dummy_data_, kFrameLength, kVideoTrackNumber,
                                2000000, false));
  EXPECT_TRUE(segment_.AddFrame(dummy_data_, kFrameLength, kVideoTrackNumber,
                                4000000, false));
  EXPECT_TRUE(segment_.AddFrame(dummy_data_, kFrameLength, kVideoTrackNumber,
                                6000000, true));
  segment_.Finalize();
  CloseWriter();
#ifdef _MSC_VER
  // Close the output file: the MS run time won't allow mkvparser::MkvReader
  // to open a file for reading when it's still open for writing.
  temp_file_.reset();
#endif
  mkvparser::MkvReader reader;
  ASSERT_EQ(0, reader.Open(filename_.c_str()));
  MkvWriter cues_writer;
  std::string cues_filename = libwebm::GetTempFileName();
  ASSERT_GT(cues_filename.length(), 0u);
  cues_writer.Open(cues_filename.c_str());
  EXPECT_TRUE(segment_.CopyAndMoveCuesBeforeClusters(&reader, &cues_writer));
  reader.Close();
  cues_writer.Close();

  EXPECT_TRUE(CompareFiles(GetTestFilePath("cues_before_clusters.webm"),
                           cues_filename));
}

TEST_F(MuxerTest, MaxClusterSize) {
  EXPECT_TRUE(SegmentInit(false, false, false));
  AddVideoTrack();
  const uint64_t kMaxClusterSize = 20;
  segment_.set_max_cluster_size(kMaxClusterSize);
  EXPECT_EQ(kMaxClusterSize, segment_.max_cluster_size());
  EXPECT_TRUE(segment_.AddFrame(dummy_data_, 1, kVideoTrackNumber, 0, false));
  EXPECT_TRUE(
      segment_.AddFrame(dummy_data_, 1, kVideoTrackNumber, 2000000, false));
  EXPECT_TRUE(
      segment_.AddFrame(dummy_data_, 1, kVideoTrackNumber, 4000000, false));
  EXPECT_TRUE(segment_.AddFrame(dummy_data_, kFrameLength, kVideoTrackNumber,
                                6000000, false));
  EXPECT_TRUE(segment_.AddFrame(dummy_data_, kFrameLength, kVideoTrackNumber,
                                8000000, false));
  EXPECT_TRUE(segment_.AddFrame(dummy_data_, kFrameLength, kVideoTrackNumber,
                                9000000, false));
  segment_.Finalize();

  CloseWriter();

  EXPECT_TRUE(
      CompareFiles(GetTestFilePath("max_cluster_size.webm"), filename_));
}

TEST_F(MuxerTest, MaxClusterDuration) {
  EXPECT_TRUE(SegmentInit(false, false, false));
  AddVideoTrack();
  const uint64_t kMaxClusterDuration = 4000000;
  segment_.set_max_cluster_duration(kMaxClusterDuration);

  EXPECT_EQ(kMaxClusterDuration, segment_.max_cluster_duration());
  EXPECT_TRUE(segment_.AddFrame(dummy_data_, kFrameLength, kVideoTrackNumber, 0,
                                false));
  EXPECT_TRUE(segment_.AddFrame(dummy_data_, kFrameLength, kVideoTrackNumber,
                                2000000, false));
  EXPECT_TRUE(segment_.AddFrame(dummy_data_, kFrameLength, kVideoTrackNumber,
                                4000000, false));
  EXPECT_TRUE(segment_.AddFrame(dummy_data_, kFrameLength, kVideoTrackNumber,
                                6000000, false));
  EXPECT_TRUE(segment_.AddFrame(dummy_data_, kFrameLength, kVideoTrackNumber,
                                8000000, false));
  EXPECT_TRUE(segment_.AddFrame(dummy_data_, kFrameLength, kVideoTrackNumber,
                                9000000, false));
  segment_.Finalize();

  CloseWriter();

  EXPECT_TRUE(
      CompareFiles(GetTestFilePath("max_cluster_duration.webm"), filename_));
}

TEST_F(MuxerTest, SetCuesTrackNumber) {
  const uint64_t kTrackNumber = 10;
  EXPECT_TRUE(SegmentInit(true, false, false));
  const uint64_t vid_track =
      segment_.AddVideoTrack(kWidth, kHeight, kTrackNumber);
  EXPECT_EQ(kTrackNumber, vid_track);
  segment_.GetTrackByNumber(vid_track)->set_uid(kVideoTrackNumber);
  EXPECT_TRUE(segment_.CuesTrack(vid_track));

  EXPECT_EQ(vid_track, segment_.cues_track());
  EXPECT_TRUE(
      segment_.AddFrame(dummy_data_, kFrameLength, kTrackNumber, 0, true));
  EXPECT_TRUE(segment_.AddFrame(dummy_data_, kFrameLength, kTrackNumber,
                                2000000, false));
  EXPECT_TRUE(segment_.AddFrame(dummy_data_, kFrameLength, kTrackNumber,
                                4000000, false));
  EXPECT_TRUE(segment_.AddFrame(dummy_data_, kFrameLength, kTrackNumber,
                                6000000, true));
  EXPECT_TRUE(segment_.AddFrame(dummy_data_, kFrameLength, kTrackNumber,
                                8000000, false));
  EXPECT_TRUE(segment_.AddFrame(dummy_data_, kFrameLength, kTrackNumber,
                                9000000, false));
  segment_.Finalize();

  CloseWriter();

  EXPECT_TRUE(
      CompareFiles(GetTestFilePath("set_cues_track_number.webm"), filename_));
}

TEST_F(MuxerTest, BlockWithDiscardPadding) {
  EXPECT_TRUE(SegmentInit(false, false, false));
  AddAudioTrack();

  int timecode = 1000;
  // 12810000 == 0xc37710, should be 0-extended to avoid changing the sign.
  // The next two should be written as 1 byte.
  std::array<int, 3> values = {{12810000, 127, -128}};
  for (const std::int64_t discard_padding : values) {
    EXPECT_TRUE(segment_.AddFrameWithDiscardPadding(
        dummy_data_, kFrameLength, discard_padding, kAudioTrackNumber, timecode,
        true))
        << "discard_padding: " << discard_padding;
    timecode += 1000;
  }

  segment_.Finalize();

  CloseWriter();

  EXPECT_TRUE(CompareFiles(GetTestFilePath("discard_padding.webm"), filename_));
}

TEST_F(MuxerTest, AccurateClusterDuration) {
  EXPECT_TRUE(SegmentInit(false, true, false));
  AddVideoTrack();

  Frame frame;
  frame.Init(dummy_data_, kFrameLength);
  frame.set_track_number(kVideoTrackNumber);
  frame.set_timestamp(0);
  frame.set_is_key(true);
  EXPECT_TRUE(segment_.AddGenericFrame(&frame));
  segment_.ForceNewClusterOnNextFrame();
  frame.set_timestamp(2000000);
  frame.set_is_key(false);
  EXPECT_TRUE(segment_.AddGenericFrame(&frame));
  frame.set_timestamp(4000000);
  EXPECT_TRUE(segment_.AddGenericFrame(&frame));
  segment_.ForceNewClusterOnNextFrame();
  frame.set_timestamp(6000000);
  EXPECT_TRUE(segment_.AddGenericFrame(&frame));
  segment_.Finalize();

  CloseWriter();

  EXPECT_TRUE(CompareFiles(GetTestFilePath("accurate_cluster_duration.webm"),
                           filename_));
}

// Tests AccurateClusterDuration flag with the duration of the very last block
// of the file set explicitly.
TEST_F(MuxerTest, AccurateClusterDurationExplicitLastFrameDuration) {
  EXPECT_TRUE(SegmentInit(false, true, false));
  AddVideoTrack();

  Frame frame;
  frame.Init(dummy_data_, kFrameLength);
  frame.set_track_number(kVideoTrackNumber);
  frame.set_timestamp(0);
  frame.set_is_key(true);
  EXPECT_TRUE(segment_.AddGenericFrame(&frame));
  segment_.ForceNewClusterOnNextFrame();
  frame.set_timestamp(2000000);
  frame.set_is_key(false);
  EXPECT_TRUE(segment_.AddGenericFrame(&frame));
  frame.set_timestamp(4000000);
  EXPECT_TRUE(segment_.AddGenericFrame(&frame));
  segment_.ForceNewClusterOnNextFrame();
  frame.set_timestamp(6000000);
  frame.set_duration(2000000);
  EXPECT_TRUE(segment_.AddGenericFrame(&frame));
  segment_.Finalize();

  // SegmentInfo's duration is in timecode scale
  EXPECT_EQ(8, segment_.GetSegmentInfo()->duration());

  CloseWriter();

  EXPECT_TRUE(CompareFiles(
      GetTestFilePath("accurate_cluster_duration_last_frame.webm"), filename_));
}

TEST_F(MuxerTest, AccurateClusterDurationTwoTracks) {
  EXPECT_TRUE(SegmentInit(false, true, false));
  AddVideoTrack();
  AddAudioTrack();

  Frame video_frame;
  video_frame.Init(dummy_data_, kFrameLength);
  video_frame.set_track_number(kVideoTrackNumber);
  Frame audio_frame;
  audio_frame.Init(dummy_data_, kFrameLength);
  audio_frame.set_track_number(kAudioTrackNumber);
  std::array<std::uint64_t, 2> cluster_timestamps = {{0, 40000000}};
  for (const std::uint64_t cluster_timestamp : cluster_timestamps) {
    // Add video and audio frames with timestamp 0.
    video_frame.set_timestamp(cluster_timestamp);
    video_frame.set_is_key(true);
    EXPECT_TRUE(segment_.AddGenericFrame(&video_frame));
    audio_frame.set_timestamp(cluster_timestamp);
    audio_frame.set_is_key(true);
    EXPECT_TRUE(segment_.AddGenericFrame(&audio_frame));

    // Add 3 consecutive audio frames.
    std::array<std::uint64_t, 3> audio_timestamps = {
        {10000000, 20000000, 30000000}};
    for (const std::uint64_t audio_timestamp : audio_timestamps) {
      audio_frame.set_timestamp(cluster_timestamp + audio_timestamp);
      // Explicitly set duration for the very last audio frame.
      if (cluster_timestamp == 40000000 && audio_timestamp == 30000000) {
        audio_frame.set_duration(10000000);
      }
      EXPECT_TRUE(segment_.AddGenericFrame(&audio_frame));
    }

    // Add a video frame with timestamp 33ms.
    video_frame.set_is_key(false);
    // Explicitly set duration for the very last video frame.
    if (cluster_timestamp == 40000000) {
      video_frame.set_duration(7000000);
    }
    video_frame.set_timestamp(cluster_timestamp + 33000000);
    EXPECT_TRUE(segment_.AddGenericFrame(&video_frame));
    segment_.ForceNewClusterOnNextFrame();
  }
  segment_.Finalize();

  // SegmentInfo's duration is in timecode scale
  EXPECT_EQ(80, segment_.GetSegmentInfo()->duration());

  CloseWriter();

  EXPECT_TRUE(CompareFiles(
      GetTestFilePath("accurate_cluster_duration_two_tracks.webm"), filename_));
}

TEST_F(MuxerTest, UseFixedSizeClusterTimecode) {
  EXPECT_TRUE(SegmentInit(false, false, true));
  AddVideoTrack();

  Frame frame;
  frame.Init(dummy_data_, kFrameLength);
  frame.set_track_number(kVideoTrackNumber);
  frame.set_timestamp(0);
  frame.set_is_key(true);
  EXPECT_TRUE(segment_.AddGenericFrame(&frame));
  segment_.ForceNewClusterOnNextFrame();
  frame.set_timestamp(2000000);
  EXPECT_TRUE(segment_.AddGenericFrame(&frame));
  segment_.ForceNewClusterOnNextFrame();
  frame.set_timestamp(4000000);
  EXPECT_TRUE(segment_.AddGenericFrame(&frame));
  segment_.Finalize();

  CloseWriter();

  EXPECT_TRUE(CompareFiles(GetTestFilePath("fixed_size_cluster_timecode.webm"),
                           filename_));
}

TEST_F(MuxerTest, DocTypeWebm) {
  EXPECT_TRUE(SegmentInit(false, false, false));
  AddVideoTrack();
  Track* const vid_track = segment_.GetTrackByNumber(kVideoTrackNumber);
  vid_track->set_codec_id(kVP9CodecId);
  AddDummyFrameAndFinalize(kVideoTrackNumber);
  EXPECT_TRUE(CompareFiles(GetTestFilePath("webm_doctype.webm"), filename_));
}

TEST_F(MuxerTest, DocTypeMatroska) {
  EXPECT_TRUE(SegmentInit(false, false, false));
  AddVideoTrack();
  Track* const vid_track = segment_.GetTrackByNumber(kVideoTrackNumber);
  vid_track->set_codec_id("V_SOMETHING_NOT_IN_WEBM");
  AddDummyFrameAndFinalize(kVideoTrackNumber);
  EXPECT_TRUE(CompareFiles(GetTestFilePath("matroska_doctype.mkv"), filename_));
}

}  // namespace test

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
