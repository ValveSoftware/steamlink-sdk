// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cstdlib>

#include "base/bind.h"
#include "base/logging.h"
#include "media/base/decrypt_config.h"
#include "media/formats/webm/cluster_builder.h"
#include "media/formats/webm/webm_cluster_parser.h"
#include "media/formats/webm/webm_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::Return;
using ::testing::_;

namespace media {

typedef WebMTracksParser::TextTracks TextTracks;

enum {
  kTimecodeScale = 1000000,  // Timecode scale for millisecond timestamps.
  kAudioTrackNum = 1,
  kVideoTrackNum = 2,
  kTextTrackNum = 3,
  kTestAudioFrameDefaultDurationInMs = 13,
  kTestVideoFrameDefaultDurationInMs = 17
};

COMPILE_ASSERT(
    static_cast<int>(kTestAudioFrameDefaultDurationInMs) !=
        static_cast<int>(WebMClusterParser::kDefaultAudioBufferDurationInMs),
    test_default_is_same_as_estimation_fallback_audio_duration);
COMPILE_ASSERT(
    static_cast<int>(kTestVideoFrameDefaultDurationInMs) !=
        static_cast<int>(WebMClusterParser::kDefaultVideoBufferDurationInMs),
    test_default_is_same_as_estimation_fallback_video_duration);

struct BlockInfo {
  int track_num;
  int timestamp;

  // Negative value is allowed only for block groups (not simple blocks) and
  // directs CreateCluster() to exclude BlockDuration entry from the cluster for
  // this BlockGroup. The absolute value is used for parser verification.
  // For simple blocks, this value must be non-negative, and is used only for
  // parser verification.
  int duration;
  bool use_simple_block;
};

static const BlockInfo kDefaultBlockInfo[] = {
  { kAudioTrackNum, 0, 23, true },
  { kAudioTrackNum, 23, 23, true },
  { kVideoTrackNum, 33, 34, true },  // Assumes not using DefaultDuration
  { kAudioTrackNum, 46, 23, true },
  { kVideoTrackNum, 67, 33, false },
  { kAudioTrackNum, 69, 23, false },
  { kVideoTrackNum, 100, 33, false },
};

static const uint8 kEncryptedFrame[] = {
  0x01,  // Block is encrypted
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08  // IV
};

static scoped_ptr<Cluster> CreateCluster(int timecode,
                                         const BlockInfo* block_info,
                                         int block_count) {
  ClusterBuilder cb;
  cb.SetClusterTimecode(0);

  for (int i = 0; i < block_count; i++) {
    uint8 data[] = { 0x00 };
    if (block_info[i].use_simple_block) {
      CHECK_GE(block_info[i].duration, 0);
      cb.AddSimpleBlock(block_info[i].track_num,
                        block_info[i].timestamp,
                        0, data, sizeof(data));
      continue;
    }

    if (block_info[i].duration < 0) {
      cb.AddBlockGroupWithoutBlockDuration(block_info[i].track_num,
                                           block_info[i].timestamp,
                                           0, data, sizeof(data));
      continue;
    }

    cb.AddBlockGroup(block_info[i].track_num,
                     block_info[i].timestamp,
                     block_info[i].duration,
                     0, data, sizeof(data));
  }

  return cb.Finish();
}

// Creates a Cluster with one encrypted Block. |bytes_to_write| is number of
// bytes of the encrypted frame to write.
static scoped_ptr<Cluster> CreateEncryptedCluster(int bytes_to_write) {
  CHECK_GT(bytes_to_write, 0);
  CHECK_LE(bytes_to_write, static_cast<int>(sizeof(kEncryptedFrame)));

  ClusterBuilder cb;
  cb.SetClusterTimecode(0);
  cb.AddSimpleBlock(kVideoTrackNum, 0, 0, kEncryptedFrame, bytes_to_write);
  return cb.Finish();
}

static bool VerifyBuffers(const WebMClusterParser::BufferQueue& audio_buffers,
                          const WebMClusterParser::BufferQueue& video_buffers,
                          const WebMClusterParser::BufferQueue& text_buffers,
                          const BlockInfo* block_info,
                          int block_count) {
  int buffer_count = audio_buffers.size() + video_buffers.size() +
      text_buffers.size();
  if (block_count != buffer_count) {
    DVLOG(1) << __FUNCTION__ << " : block_count (" << block_count
             << ") mismatches buffer_count (" << buffer_count << ")";
    return false;
  }

  size_t audio_offset = 0;
  size_t video_offset = 0;
  size_t text_offset = 0;
  for (int i = 0; i < block_count; i++) {
    const WebMClusterParser::BufferQueue* buffers = NULL;
    size_t* offset;
    StreamParserBuffer::Type expected_type = DemuxerStream::UNKNOWN;

    if (block_info[i].track_num == kAudioTrackNum) {
      buffers = &audio_buffers;
      offset = &audio_offset;
      expected_type = DemuxerStream::AUDIO;
    } else if (block_info[i].track_num == kVideoTrackNum) {
      buffers = &video_buffers;
      offset = &video_offset;
      expected_type = DemuxerStream::VIDEO;
    } else if (block_info[i].track_num == kTextTrackNum) {
      buffers = &text_buffers;
      offset = &text_offset;
      expected_type = DemuxerStream::TEXT;
    } else {
      LOG(ERROR) << "Unexpected track number " << block_info[i].track_num;
      return false;
    }

    if (*offset >= buffers->size()) {
      DVLOG(1) << __FUNCTION__ << " : Too few buffers (" << buffers->size()
               << ") for track_num (" << block_info[i].track_num
               << "), expected at least " << *offset + 1 << " buffers";
      return false;
    }

    scoped_refptr<StreamParserBuffer> buffer = (*buffers)[(*offset)++];

    EXPECT_EQ(block_info[i].timestamp, buffer->timestamp().InMilliseconds());
    EXPECT_EQ(std::abs(block_info[i].duration),
              buffer->duration().InMilliseconds());
    EXPECT_EQ(expected_type, buffer->type());
    EXPECT_EQ(block_info[i].track_num, buffer->track_id());
  }

  return true;
}

static bool VerifyBuffers(const scoped_ptr<WebMClusterParser>& parser,
                          const BlockInfo* block_info,
                          int block_count) {
  const WebMClusterParser::TextBufferQueueMap& text_map =
      parser->GetTextBuffers();
  const WebMClusterParser::BufferQueue* text_buffers;
  const WebMClusterParser::BufferQueue no_text_buffers;
  if (!text_map.empty())
    text_buffers = &(text_map.rbegin()->second);
  else
    text_buffers = &no_text_buffers;

  return VerifyBuffers(parser->GetAudioBuffers(),
                       parser->GetVideoBuffers(),
                       *text_buffers,
                       block_info,
                       block_count);
}

static bool VerifyTextBuffers(
    const scoped_ptr<WebMClusterParser>& parser,
    const BlockInfo* block_info_ptr,
    int block_count,
    int text_track_num,
    const WebMClusterParser::BufferQueue& text_buffers) {
  const BlockInfo* const block_info_end = block_info_ptr + block_count;

  typedef WebMClusterParser::BufferQueue::const_iterator TextBufferIter;
  TextBufferIter buffer_iter = text_buffers.begin();
  const TextBufferIter buffer_end = text_buffers.end();

  while (block_info_ptr != block_info_end) {
    const BlockInfo& block_info = *block_info_ptr++;

    if (block_info.track_num != text_track_num)
      continue;

    EXPECT_FALSE(block_info.use_simple_block);
    EXPECT_FALSE(buffer_iter == buffer_end);

    const scoped_refptr<StreamParserBuffer> buffer = *buffer_iter++;
    EXPECT_EQ(block_info.timestamp, buffer->timestamp().InMilliseconds());
    EXPECT_EQ(std::abs(block_info.duration),
              buffer->duration().InMilliseconds());
    EXPECT_EQ(DemuxerStream::TEXT, buffer->type());
    EXPECT_EQ(text_track_num, buffer->track_id());
  }

  EXPECT_TRUE(buffer_iter == buffer_end);
  return true;
}

static void VerifyEncryptedBuffer(
    scoped_refptr<StreamParserBuffer> buffer) {
  EXPECT_TRUE(buffer->decrypt_config());
  EXPECT_EQ(static_cast<unsigned long>(DecryptConfig::kDecryptionKeySize),
            buffer->decrypt_config()->iv().length());
}

static void AppendToEnd(const WebMClusterParser::BufferQueue& src,
                        WebMClusterParser::BufferQueue* dest) {
  for (WebMClusterParser::BufferQueue::const_iterator itr = src.begin();
       itr != src.end(); ++itr) {
    dest->push_back(*itr);
  }
}

class WebMClusterParserTest : public testing::Test {
 public:
  WebMClusterParserTest()
      : parser_(new WebMClusterParser(kTimecodeScale,
                                      kAudioTrackNum,
                                      kNoTimestamp(),
                                      kVideoTrackNum,
                                      kNoTimestamp(),
                                      TextTracks(),
                                      std::set<int64>(),
                                      std::string(),
                                      std::string(),
                                      LogCB())) {}

 protected:
  void ResetParserToHaveDefaultDurations() {
    base::TimeDelta default_audio_duration = base::TimeDelta::FromMilliseconds(
        kTestAudioFrameDefaultDurationInMs);
    base::TimeDelta default_video_duration = base::TimeDelta::FromMilliseconds(
        kTestVideoFrameDefaultDurationInMs);
    ASSERT_GE(default_audio_duration, base::TimeDelta());
    ASSERT_GE(default_video_duration, base::TimeDelta());
    ASSERT_NE(kNoTimestamp(), default_audio_duration);
    ASSERT_NE(kNoTimestamp(), default_video_duration);

    parser_.reset(new WebMClusterParser(kTimecodeScale,
                                        kAudioTrackNum,
                                        default_audio_duration,
                                        kVideoTrackNum,
                                        default_video_duration,
                                        TextTracks(),
                                        std::set<int64>(),
                                        std::string(),
                                        std::string(),
                                        LogCB()));
  }

  scoped_ptr<WebMClusterParser> parser_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebMClusterParserTest);
};

TEST_F(WebMClusterParserTest, HeldBackBufferHoldsBackAllTracks) {
  // If a buffer is missing duration and is being held back, then all other
  // tracks' buffers that have same or higher (decode) timestamp should be held
  // back too to keep the timestamps emitted for a cluster monotonically
  // non-decreasing and in same order as parsed.
  InSequence s;

  // Reset the parser to have 3 tracks: text, video (no default frame duration),
  // and audio (with a default frame duration).
  TextTracks text_tracks;
  text_tracks.insert(std::make_pair(TextTracks::key_type(kTextTrackNum),
                                    TextTrackConfig(kTextSubtitles, "", "",
                                                    "")));
  base::TimeDelta default_audio_duration =
      base::TimeDelta::FromMilliseconds(kTestAudioFrameDefaultDurationInMs);
  ASSERT_GE(default_audio_duration, base::TimeDelta());
  ASSERT_NE(kNoTimestamp(), default_audio_duration);
  parser_.reset(new WebMClusterParser(kTimecodeScale,
                                      kAudioTrackNum,
                                      default_audio_duration,
                                      kVideoTrackNum,
                                      kNoTimestamp(),
                                      text_tracks,
                                      std::set<int64>(),
                                      std::string(),
                                      std::string(),
                                      LogCB()));

  const BlockInfo kBlockInfo[] = {
    { kVideoTrackNum, 0, 33, true },
    { kAudioTrackNum, 0, 23, false },
    { kTextTrackNum, 10, 42, false },
    { kAudioTrackNum, 23, kTestAudioFrameDefaultDurationInMs, true },
    { kVideoTrackNum, 33, 33, true },
    { kAudioTrackNum, 36, kTestAudioFrameDefaultDurationInMs, true },
    { kVideoTrackNum, 66, 33, true },
    { kAudioTrackNum, 70, kTestAudioFrameDefaultDurationInMs, true },
    { kAudioTrackNum, 83, kTestAudioFrameDefaultDurationInMs, true },
  };

  const int kExpectedBuffersOnPartialCluster[] = {
    0,  // Video simple block without DefaultDuration should be held back
    0,  // Audio buffer ready, but not emitted because its TS >= held back video
    0,  // Text buffer ready, but not emitted because its TS >= held back video
    0,  // 2nd audio buffer ready, also not emitted for same reason as first
    4,  // All previous buffers emitted, 2nd video held back with no duration
    4,  // 2nd video still has no duration, 3rd audio ready but not emitted
    6,  // All previous buffers emitted, 3rd video held back with no duration
    6,  // 3rd video still has no duration, 4th audio ready but not emitted
    9,  // Cluster end emits all buffers and 3rd video's duration is estimated
  };

  ASSERT_EQ(arraysize(kBlockInfo), arraysize(kExpectedBuffersOnPartialCluster));
  int block_count = arraysize(kBlockInfo);

  // Iteratively create a cluster containing the first N+1 blocks and parse all
  // but the last byte of the cluster (except when N==|block_count|, just parse
  // the whole cluster). Verify that the corresponding entry in
  // |kExpectedBuffersOnPartialCluster| identifies the exact subset of
  // |kBlockInfo| returned by the parser.
  for (int i = 0; i < block_count; ++i) {
    if (i > 0)
      parser_->Reset();
    // Since we don't know exactly the offsets of each block in the full
    // cluster, build a cluster with exactly one additional block so that
    // parse of all but one byte should deterministically parse all but the
    // last full block. Don't |exceed block_count| blocks though.
    int blocks_in_cluster = std::min(i + 2, block_count);
    scoped_ptr<Cluster> cluster(CreateCluster(0, kBlockInfo,
                                              blocks_in_cluster));
    // Parse all but the last byte unless we need to parse the full cluster.
    bool parse_full_cluster = i == (block_count - 1);
    int result = parser_->Parse(cluster->data(), parse_full_cluster ?
                                cluster->size() : cluster->size() - 1);
    if (parse_full_cluster) {
      DVLOG(1) << "Verifying parse result of full cluster of "
               << blocks_in_cluster << " blocks";
      EXPECT_EQ(cluster->size(), result);
    } else {
      DVLOG(1) << "Verifying parse result of cluster of "
               << blocks_in_cluster << " blocks with last block incomplete";
      EXPECT_GT(cluster->size(), result);
      EXPECT_LT(0, result);
    }

    EXPECT_TRUE(VerifyBuffers(parser_, kBlockInfo,
                              kExpectedBuffersOnPartialCluster[i]));
  }
}

TEST_F(WebMClusterParserTest, Reset) {
  InSequence s;

  int block_count = arraysize(kDefaultBlockInfo);
  scoped_ptr<Cluster> cluster(CreateCluster(0, kDefaultBlockInfo, block_count));

  // Send slightly less than the full cluster so all but the last block is
  // parsed.
  int result = parser_->Parse(cluster->data(), cluster->size() - 1);
  EXPECT_GT(result, 0);
  EXPECT_LT(result, cluster->size());

  ASSERT_TRUE(VerifyBuffers(parser_, kDefaultBlockInfo, block_count - 1));
  parser_->Reset();

  // Now parse a whole cluster to verify that all the blocks will get parsed.
  result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kDefaultBlockInfo, block_count));
}

TEST_F(WebMClusterParserTest, ParseClusterWithSingleCall) {
  int block_count = arraysize(kDefaultBlockInfo);
  scoped_ptr<Cluster> cluster(CreateCluster(0, kDefaultBlockInfo, block_count));

  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kDefaultBlockInfo, block_count));
}

TEST_F(WebMClusterParserTest, ParseClusterWithMultipleCalls) {
  int block_count = arraysize(kDefaultBlockInfo);
  scoped_ptr<Cluster> cluster(CreateCluster(0, kDefaultBlockInfo, block_count));

  WebMClusterParser::BufferQueue audio_buffers;
  WebMClusterParser::BufferQueue video_buffers;
  const WebMClusterParser::BufferQueue no_text_buffers;

  const uint8* data = cluster->data();
  int size = cluster->size();
  int default_parse_size = 3;
  int parse_size = std::min(default_parse_size, size);

  while (size > 0) {
    int result = parser_->Parse(data, parse_size);
    ASSERT_GE(result, 0);
    ASSERT_LE(result, parse_size);

    if (result == 0) {
      // The parser needs more data so increase the parse_size a little.
      parse_size += default_parse_size;
      parse_size = std::min(parse_size, size);
      continue;
    }

    AppendToEnd(parser_->GetAudioBuffers(), &audio_buffers);
    AppendToEnd(parser_->GetVideoBuffers(), &video_buffers);

    parse_size = default_parse_size;

    data += result;
    size -= result;
  }
  ASSERT_TRUE(VerifyBuffers(audio_buffers, video_buffers,
                            no_text_buffers, kDefaultBlockInfo,
                            block_count));
}

// Verify that both BlockGroups with the BlockDuration before the Block
// and BlockGroups with the BlockDuration after the Block are supported
// correctly.
// Note: Raw bytes are use here because ClusterBuilder only generates
// one of these scenarios.
TEST_F(WebMClusterParserTest, ParseBlockGroup) {
  const BlockInfo kBlockInfo[] = {
    { kAudioTrackNum, 0, 23, false },
    { kVideoTrackNum, 33, 34, false },
  };
  int block_count = arraysize(kBlockInfo);

  const uint8 kClusterData[] = {
    0x1F, 0x43, 0xB6, 0x75, 0x9B,  // Cluster(size=27)
    0xE7, 0x81, 0x00,  // Timecode(size=1, value=0)
    // BlockGroup with BlockDuration before Block.
    0xA0, 0x8A,  // BlockGroup(size=10)
    0x9B, 0x81, 0x17,  // BlockDuration(size=1, value=23)
    0xA1, 0x85, 0x81, 0x00, 0x00, 0x00, 0xaa,  // Block(size=5, track=1, ts=0)
    // BlockGroup with BlockDuration after Block.
    0xA0, 0x8A,  // BlockGroup(size=10)
    0xA1, 0x85, 0x82, 0x00, 0x21, 0x00, 0x55,  // Block(size=5, track=2, ts=33)
    0x9B, 0x81, 0x22,  // BlockDuration(size=1, value=34)
  };
  const int kClusterSize = sizeof(kClusterData);

  int result = parser_->Parse(kClusterData, kClusterSize);
  EXPECT_EQ(kClusterSize, result);
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo, block_count));
}

TEST_F(WebMClusterParserTest, ParseSimpleBlockAndBlockGroupMixture) {
  const BlockInfo kBlockInfo[] = {
    { kAudioTrackNum, 0, 23, true },
    { kAudioTrackNum, 23, 23, false },
    { kVideoTrackNum, 33, 34, true },
    { kAudioTrackNum, 46, 23, false },
    { kVideoTrackNum, 67, 33, false },
  };
  int block_count = arraysize(kBlockInfo);
  scoped_ptr<Cluster> cluster(CreateCluster(0, kBlockInfo, block_count));

  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo, block_count));
}

TEST_F(WebMClusterParserTest, IgnoredTracks) {
  std::set<int64> ignored_tracks;
  ignored_tracks.insert(kTextTrackNum);

  parser_.reset(new WebMClusterParser(kTimecodeScale,
                                      kAudioTrackNum,
                                      kNoTimestamp(),
                                      kVideoTrackNum,
                                      kNoTimestamp(),
                                      TextTracks(),
                                      ignored_tracks,
                                      std::string(),
                                      std::string(),
                                      LogCB()));

  const BlockInfo kInputBlockInfo[] = {
    { kAudioTrackNum, 0,  23, true },
    { kAudioTrackNum, 23, 23, true },
    { kVideoTrackNum, 33, 34, true },
    { kTextTrackNum,  33, 99, true },
    { kAudioTrackNum, 46, 23, true },
    { kVideoTrackNum, 67, 34, true },
  };
  int input_block_count = arraysize(kInputBlockInfo);

  const BlockInfo kOutputBlockInfo[] = {
    { kAudioTrackNum, 0,  23, true },
    { kAudioTrackNum, 23, 23, true },
    { kVideoTrackNum, 33, 34, true },
    { kAudioTrackNum, 46, 23, true },
    { kVideoTrackNum, 67, 34, true },
  };
  int output_block_count = arraysize(kOutputBlockInfo);

  scoped_ptr<Cluster> cluster(
      CreateCluster(0, kInputBlockInfo, input_block_count));

  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kOutputBlockInfo, output_block_count));
}

TEST_F(WebMClusterParserTest, ParseTextTracks) {
  TextTracks text_tracks;

  text_tracks.insert(std::make_pair(TextTracks::key_type(kTextTrackNum),
                                    TextTrackConfig(kTextSubtitles, "", "",
                                                    "")));

  parser_.reset(new WebMClusterParser(kTimecodeScale,
                                      kAudioTrackNum,
                                      kNoTimestamp(),
                                      kVideoTrackNum,
                                      kNoTimestamp(),
                                      text_tracks,
                                      std::set<int64>(),
                                      std::string(),
                                      std::string(),
                                      LogCB()));

  const BlockInfo kInputBlockInfo[] = {
    { kAudioTrackNum, 0,  23, true },
    { kAudioTrackNum, 23, 23, true },
    { kVideoTrackNum, 33, 34, true },
    { kTextTrackNum,  33, 42, false },
    { kAudioTrackNum, 46, 23, true },
    { kTextTrackNum, 55, 44, false },
    { kVideoTrackNum, 67, 34, true },
  };
  int input_block_count = arraysize(kInputBlockInfo);

  scoped_ptr<Cluster> cluster(
      CreateCluster(0, kInputBlockInfo, input_block_count));

  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kInputBlockInfo, input_block_count));
}

TEST_F(WebMClusterParserTest, TextTracksSimpleBlock) {
  TextTracks text_tracks;

  text_tracks.insert(std::make_pair(TextTracks::key_type(kTextTrackNum),
                                    TextTrackConfig(kTextSubtitles, "", "",
                                                    "")));

  parser_.reset(new WebMClusterParser(kTimecodeScale,
                                      kAudioTrackNum,
                                      kNoTimestamp(),
                                      kVideoTrackNum,
                                      kNoTimestamp(),
                                      text_tracks,
                                      std::set<int64>(),
                                      std::string(),
                                      std::string(),
                                      LogCB()));

  const BlockInfo kInputBlockInfo[] = {
    { kTextTrackNum,  33, 42, true },
  };
  int input_block_count = arraysize(kInputBlockInfo);

  scoped_ptr<Cluster> cluster(
      CreateCluster(0, kInputBlockInfo, input_block_count));

  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_LT(result, 0);
}

TEST_F(WebMClusterParserTest, ParseMultipleTextTracks) {
  TextTracks text_tracks;

  const int kSubtitleTextTrackNum = kTextTrackNum;
  const int kCaptionTextTrackNum = kTextTrackNum + 1;

  text_tracks.insert(std::make_pair(TextTracks::key_type(kSubtitleTextTrackNum),
                                    TextTrackConfig(kTextSubtitles, "", "",
                                                    "")));

  text_tracks.insert(std::make_pair(TextTracks::key_type(kCaptionTextTrackNum),
                                    TextTrackConfig(kTextCaptions, "", "",
                                                    "")));

  parser_.reset(new WebMClusterParser(kTimecodeScale,
                                      kAudioTrackNum,
                                      kNoTimestamp(),
                                      kVideoTrackNum,
                                      kNoTimestamp(),
                                      text_tracks,
                                      std::set<int64>(),
                                      std::string(),
                                      std::string(),
                                      LogCB()));

  const BlockInfo kInputBlockInfo[] = {
    { kAudioTrackNum, 0,  23, true },
    { kAudioTrackNum, 23, 23, true },
    { kVideoTrackNum, 33, 34, true },
    { kSubtitleTextTrackNum,  33, 42, false },
    { kAudioTrackNum, 46, 23, true },
    { kCaptionTextTrackNum, 55, 44, false },
    { kVideoTrackNum, 67, 34, true },
    { kSubtitleTextTrackNum,  67, 33, false },
  };
  int input_block_count = arraysize(kInputBlockInfo);

  scoped_ptr<Cluster> cluster(
      CreateCluster(0, kInputBlockInfo, input_block_count));

  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);

  const WebMClusterParser::TextBufferQueueMap& text_map =
      parser_->GetTextBuffers();
  for (WebMClusterParser::TextBufferQueueMap::const_iterator itr =
           text_map.begin();
       itr != text_map.end();
       ++itr) {
    const TextTracks::const_iterator find_result =
        text_tracks.find(itr->first);
    ASSERT_TRUE(find_result != text_tracks.end());
    ASSERT_TRUE(VerifyTextBuffers(parser_, kInputBlockInfo, input_block_count,
                                  itr->first, itr->second));
  }
}

TEST_F(WebMClusterParserTest, ParseEncryptedBlock) {
  scoped_ptr<Cluster> cluster(CreateEncryptedCluster(sizeof(kEncryptedFrame)));

  parser_.reset(new WebMClusterParser(kTimecodeScale,
                                      kAudioTrackNum,
                                      kNoTimestamp(),
                                      kVideoTrackNum,
                                      kNoTimestamp(),
                                      TextTracks(),
                                      std::set<int64>(),
                                      std::string(),
                                      "video_key_id",
                                      LogCB()));
  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);
  ASSERT_EQ(1UL, parser_->GetVideoBuffers().size());
  scoped_refptr<StreamParserBuffer> buffer = parser_->GetVideoBuffers()[0];
  VerifyEncryptedBuffer(buffer);
}

TEST_F(WebMClusterParserTest, ParseBadEncryptedBlock) {
  scoped_ptr<Cluster> cluster(
      CreateEncryptedCluster(sizeof(kEncryptedFrame) - 1));

  parser_.reset(new WebMClusterParser(kTimecodeScale,
                                      kAudioTrackNum,
                                      kNoTimestamp(),
                                      kVideoTrackNum,
                                      kNoTimestamp(),
                                      TextTracks(),
                                      std::set<int64>(),
                                      std::string(),
                                      "video_key_id",
                                      LogCB()));
  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(-1, result);
}

TEST_F(WebMClusterParserTest, ParseInvalidZeroSizedCluster) {
  const uint8 kBuffer[] = {
    0x1F, 0x43, 0xB6, 0x75, 0x80,  // CLUSTER (size = 0)
  };

  EXPECT_EQ(-1, parser_->Parse(kBuffer, sizeof(kBuffer)));
}

TEST_F(WebMClusterParserTest, ParseInvalidUnknownButActuallyZeroSizedCluster) {
  const uint8 kBuffer[] = {
    0x1F, 0x43, 0xB6, 0x75, 0xFF,  // CLUSTER (size = "unknown")
    0x1F, 0x43, 0xB6, 0x75, 0x85,  // CLUSTER (size = 5)
  };

  EXPECT_EQ(-1, parser_->Parse(kBuffer, sizeof(kBuffer)));
}

TEST_F(WebMClusterParserTest, ParseInvalidTextBlockGroupWithoutDuration) {
  // Text track frames must have explicitly specified BlockGroup BlockDurations.
  TextTracks text_tracks;

  text_tracks.insert(std::make_pair(TextTracks::key_type(kTextTrackNum),
                                    TextTrackConfig(kTextSubtitles, "", "",
                                                    "")));

  parser_.reset(new WebMClusterParser(kTimecodeScale,
                                      kAudioTrackNum,
                                      kNoTimestamp(),
                                      kVideoTrackNum,
                                      kNoTimestamp(),
                                      text_tracks,
                                      std::set<int64>(),
                                      std::string(),
                                      std::string(),
                                      LogCB()));

  const BlockInfo kBlockInfo[] = {
    { kTextTrackNum,  33, -42, false },
  };
  int block_count = arraysize(kBlockInfo);
  scoped_ptr<Cluster> cluster(CreateCluster(0, kBlockInfo, block_count));
  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_LT(result, 0);
}

TEST_F(WebMClusterParserTest, ParseWithDefaultDurationsSimpleBlocks) {
  InSequence s;
  ResetParserToHaveDefaultDurations();

  EXPECT_LT(kTestAudioFrameDefaultDurationInMs, 23);
  EXPECT_LT(kTestVideoFrameDefaultDurationInMs, 33);

  const BlockInfo kBlockInfo[] = {
    { kAudioTrackNum, 0, kTestAudioFrameDefaultDurationInMs, true },
    { kAudioTrackNum, 23, kTestAudioFrameDefaultDurationInMs, true },
    { kVideoTrackNum, 33, kTestVideoFrameDefaultDurationInMs, true },
    { kAudioTrackNum, 46, kTestAudioFrameDefaultDurationInMs, true },
    { kVideoTrackNum, 67, kTestVideoFrameDefaultDurationInMs, true },
    { kAudioTrackNum, 69, kTestAudioFrameDefaultDurationInMs, true },
    { kVideoTrackNum, 100, kTestVideoFrameDefaultDurationInMs, true },
  };

  int block_count = arraysize(kBlockInfo);
  scoped_ptr<Cluster> cluster(CreateCluster(0, kBlockInfo, block_count));

  // Send slightly less than the full cluster so all but the last block is
  // parsed. Though all the blocks are simple blocks, none should be held aside
  // for duration estimation prior to end of cluster detection because all the
  // tracks have DefaultDurations.
  int result = parser_->Parse(cluster->data(), cluster->size() - 1);
  EXPECT_GT(result, 0);
  EXPECT_LT(result, cluster->size());
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo, block_count - 1));

  parser_->Reset();

  // Now parse a whole cluster to verify that all the blocks will get parsed.
  result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo, block_count));
}

TEST_F(WebMClusterParserTest, ParseWithoutAnyDurationsSimpleBlocks) {
  InSequence s;

  // Absent DefaultDuration information, SimpleBlock durations are derived from
  // inter-buffer track timestamp delta if within the cluster, and are estimated
  // as the lowest non-zero duration seen so far if the last buffer in the track
  // in the cluster (independently for each track in the cluster).
  const BlockInfo kBlockInfo1[] = {
    { kAudioTrackNum, 0, 23, true },
    { kAudioTrackNum, 23, 22, true },
    { kVideoTrackNum, 33, 33, true },
    { kAudioTrackNum, 45, 23, true },
    { kVideoTrackNum, 66, 34, true },
    { kAudioTrackNum, 68, 22, true },  // Estimated from minimum audio dur
    { kVideoTrackNum, 100, 33, true },  // Estimated from minimum video dur
  };

  int block_count1 = arraysize(kBlockInfo1);
  scoped_ptr<Cluster> cluster1(CreateCluster(0, kBlockInfo1, block_count1));

  // Send slightly less than the first full cluster so all but the last video
  // block is parsed. Verify the last fully parsed audio and video buffer are
  // both missing from the result (parser should hold them aside for duration
  // estimation prior to end of cluster detection in the absence of
  // DefaultDurations.)
  int result = parser_->Parse(cluster1->data(), cluster1->size() - 1);
  EXPECT_GT(result, 0);
  EXPECT_LT(result, cluster1->size());
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo1, block_count1 - 3));
  EXPECT_EQ(3UL, parser_->GetAudioBuffers().size());
  EXPECT_EQ(1UL, parser_->GetVideoBuffers().size());

  parser_->Reset();

  // Now parse the full first cluster and verify all the blocks are parsed.
  result = parser_->Parse(cluster1->data(), cluster1->size());
  EXPECT_EQ(cluster1->size(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo1, block_count1));

  // Verify that the estimated frame duration is tracked across clusters for
  // each track.
  const BlockInfo kBlockInfo2[] = {
    { kAudioTrackNum, 200, 22, true },  // Estimate carries over across clusters
    { kVideoTrackNum, 201, 33, true },  // Estimate carries over across clusters
  };

  int block_count2 = arraysize(kBlockInfo2);
  scoped_ptr<Cluster> cluster2(CreateCluster(0, kBlockInfo2, block_count2));
  result = parser_->Parse(cluster2->data(), cluster2->size());
  EXPECT_EQ(cluster2->size(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo2, block_count2));
}

TEST_F(WebMClusterParserTest, ParseWithoutAnyDurationsBlockGroups) {
  InSequence s;

  // Absent DefaultDuration and BlockDuration information, BlockGroup block
  // durations are derived from inter-buffer track timestamp delta if within the
  // cluster, and are estimated as the lowest non-zero duration seen so far if
  // the last buffer in the track in the cluster (independently for each track
  // in the cluster).
    const BlockInfo kBlockInfo1[] = {
    { kAudioTrackNum, 0, -23, false },
    { kAudioTrackNum, 23, -22, false },
    { kVideoTrackNum, 33, -33, false },
    { kAudioTrackNum, 45, -23, false },
    { kVideoTrackNum, 66, -34, false },
    { kAudioTrackNum, 68, -22, false },  // Estimated from minimum audio dur
    { kVideoTrackNum, 100, -33, false },  // Estimated from minimum video dur
  };

  int block_count1 = arraysize(kBlockInfo1);
  scoped_ptr<Cluster> cluster1(CreateCluster(0, kBlockInfo1, block_count1));

  // Send slightly less than the first full cluster so all but the last video
  // block is parsed. Verify the last fully parsed audio and video buffer are
  // both missing from the result (parser should hold them aside for duration
  // estimation prior to end of cluster detection in the absence of
  // DefaultDurations.)
  int result = parser_->Parse(cluster1->data(), cluster1->size() - 1);
  EXPECT_GT(result, 0);
  EXPECT_LT(result, cluster1->size());
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo1, block_count1 - 3));
  EXPECT_EQ(3UL, parser_->GetAudioBuffers().size());
  EXPECT_EQ(1UL, parser_->GetVideoBuffers().size());

  parser_->Reset();

  // Now parse the full first cluster and verify all the blocks are parsed.
  result = parser_->Parse(cluster1->data(), cluster1->size());
  EXPECT_EQ(cluster1->size(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo1, block_count1));

  // Verify that the estimated frame duration is tracked across clusters for
  // each track.
  const BlockInfo kBlockInfo2[] = {
    { kAudioTrackNum, 200, -22, false },
    { kVideoTrackNum, 201, -33, false },
  };

  int block_count2 = arraysize(kBlockInfo2);
  scoped_ptr<Cluster> cluster2(CreateCluster(0, kBlockInfo2, block_count2));
  result = parser_->Parse(cluster2->data(), cluster2->size());
  EXPECT_EQ(cluster2->size(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo2, block_count2));
}

// TODO(wolenetz): Is parser behavior correct? See http://crbug.com/363433.
TEST_F(WebMClusterParserTest,
       ParseWithDefaultDurationsBlockGroupsWithoutDurations) {
  InSequence s;
  ResetParserToHaveDefaultDurations();

  EXPECT_LT(kTestAudioFrameDefaultDurationInMs, 23);
  EXPECT_LT(kTestVideoFrameDefaultDurationInMs, 33);

  const BlockInfo kBlockInfo[] = {
    { kAudioTrackNum, 0, -kTestAudioFrameDefaultDurationInMs, false },
    { kAudioTrackNum, 23, -kTestAudioFrameDefaultDurationInMs, false },
    { kVideoTrackNum, 33, -kTestVideoFrameDefaultDurationInMs, false },
    { kAudioTrackNum, 46, -kTestAudioFrameDefaultDurationInMs, false },
    { kVideoTrackNum, 67, -kTestVideoFrameDefaultDurationInMs, false },
    { kAudioTrackNum, 69, -kTestAudioFrameDefaultDurationInMs, false },
    { kVideoTrackNum, 100, -kTestVideoFrameDefaultDurationInMs, false },
  };

  int block_count = arraysize(kBlockInfo);
  scoped_ptr<Cluster> cluster(CreateCluster(0, kBlockInfo, block_count));

  // Send slightly less than the full cluster so all but the last block is
  // parsed. None should be held aside for duration estimation prior to end of
  // cluster detection because all the tracks have DefaultDurations.
  int result = parser_->Parse(cluster->data(), cluster->size() - 1);
  EXPECT_GT(result, 0);
  EXPECT_LT(result, cluster->size());
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo, block_count - 1));

  parser_->Reset();

  // Now parse a whole cluster to verify that all the blocks will get parsed.
  result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo, block_count));
}

TEST_F(WebMClusterParserTest,
       ParseDegenerateClusterYieldsHardcodedEstimatedDurations) {
  const BlockInfo kBlockInfo[] = {
    {
      kAudioTrackNum,
      0,
      WebMClusterParser::kDefaultAudioBufferDurationInMs,
      true
    }, {
      kVideoTrackNum,
      0,
      WebMClusterParser::kDefaultVideoBufferDurationInMs,
      true
    },
  };

  int block_count = arraysize(kBlockInfo);
  scoped_ptr<Cluster> cluster(CreateCluster(0, kBlockInfo, block_count));
  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo, block_count));
}

TEST_F(WebMClusterParserTest,
       ParseDegenerateClusterWithDefaultDurationsYieldsDefaultDurations) {
  ResetParserToHaveDefaultDurations();

  const BlockInfo kBlockInfo[] = {
    { kAudioTrackNum, 0, kTestAudioFrameDefaultDurationInMs, true },
    { kVideoTrackNum, 0, kTestVideoFrameDefaultDurationInMs, true },
  };

  int block_count = arraysize(kBlockInfo);
  scoped_ptr<Cluster> cluster(CreateCluster(0, kBlockInfo, block_count));
  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo, block_count));
}

}  // namespace media
