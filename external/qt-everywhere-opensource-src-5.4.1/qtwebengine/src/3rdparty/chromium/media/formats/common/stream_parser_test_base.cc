// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/common/stream_parser_test_base.h"

#include "base/bind.h"
#include "media/base/test_data_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static std::string BufferQueueToString(
    const StreamParser::BufferQueue& buffers) {
  std::stringstream ss;

  ss << "{";
  for (StreamParser::BufferQueue::const_iterator itr = buffers.begin();
       itr != buffers.end();
       ++itr) {
    ss << " " << (*itr)->timestamp().InMilliseconds();
    if ((*itr)->IsKeyframe())
      ss << "K";
  }
  ss << " }";

  return ss.str();
}

StreamParserTestBase::StreamParserTestBase(
    scoped_ptr<StreamParser> stream_parser)
    : parser_(stream_parser.Pass()) {
  parser_->Init(
      base::Bind(&StreamParserTestBase::OnInitDone, base::Unretained(this)),
      base::Bind(&StreamParserTestBase::OnNewConfig, base::Unretained(this)),
      base::Bind(&StreamParserTestBase::OnNewBuffers, base::Unretained(this)),
      true,
      base::Bind(&StreamParserTestBase::OnKeyNeeded, base::Unretained(this)),
      base::Bind(&StreamParserTestBase::OnNewSegment, base::Unretained(this)),
      base::Bind(&StreamParserTestBase::OnEndOfSegment, base::Unretained(this)),
      LogCB());
}

StreamParserTestBase::~StreamParserTestBase() {}

std::string StreamParserTestBase::ParseFile(const std::string& filename,
                                            int append_bytes) {
  results_stream_.clear();
  scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile(filename);
  EXPECT_TRUE(
      AppendDataInPieces(buffer->data(), buffer->data_size(), append_bytes));
  return results_stream_.str();
}

std::string StreamParserTestBase::ParseData(const uint8* data, size_t length) {
  results_stream_.clear();
  EXPECT_TRUE(AppendDataInPieces(data, length, length));
  return results_stream_.str();
}

bool StreamParserTestBase::AppendDataInPieces(const uint8* data,
                                              size_t length,
                                              size_t piece_size) {
  const uint8* start = data;
  const uint8* end = data + length;
  while (start < end) {
    size_t append_size = std::min(piece_size, static_cast<size_t>(end - start));
    if (!parser_->Parse(start, append_size))
      return false;
    start += append_size;
  }
  return true;
}

void StreamParserTestBase::OnInitDone(
    bool success,
    const StreamParser::InitParameters& params) {
  EXPECT_TRUE(params.auto_update_timestamp_offset);
  DVLOG(1) << __FUNCTION__ << "(" << success << ", "
           << params.duration.InMilliseconds() << ", "
           << params.auto_update_timestamp_offset << ")";
}

bool StreamParserTestBase::OnNewConfig(
    const AudioDecoderConfig& audio_config,
    const VideoDecoderConfig& video_config,
    const StreamParser::TextTrackConfigMap& text_config) {
  DVLOG(1) << __FUNCTION__ << "(" << audio_config.IsValidConfig() << ", "
           << video_config.IsValidConfig() << ")";
  EXPECT_TRUE(audio_config.IsValidConfig());
  EXPECT_FALSE(video_config.IsValidConfig());
  last_audio_config_ = audio_config;
  return true;
}

bool StreamParserTestBase::OnNewBuffers(
    const StreamParser::BufferQueue& audio_buffers,
    const StreamParser::BufferQueue& video_buffers,
    const StreamParser::TextBufferQueueMap& text_map) {
  EXPECT_FALSE(audio_buffers.empty());
  EXPECT_TRUE(video_buffers.empty());

  // TODO(wolenetz/acolwell): Add text track support to more MSE parsers. See
  // http://crbug.com/336926.
  EXPECT_TRUE(text_map.empty());

  const std::string buffers_str = BufferQueueToString(audio_buffers);
  DVLOG(1) << __FUNCTION__ << " : " << buffers_str;
  results_stream_ << buffers_str;
  return true;
}

void StreamParserTestBase::OnKeyNeeded(const std::string& type,
                                       const std::vector<uint8>& init_data) {
  DVLOG(1) << __FUNCTION__ << "(" << type << ", " << init_data.size() << ")";
}

void StreamParserTestBase::OnNewSegment() {
  DVLOG(1) << __FUNCTION__;
  results_stream_ << "NewSegment";
}

void StreamParserTestBase::OnEndOfSegment() {
  DVLOG(1) << __FUNCTION__;
  results_stream_ << "EndOfSegment";
}

}  // namespace media
