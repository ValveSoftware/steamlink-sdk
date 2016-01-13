// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "media/base/media.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

namespace media {

static AVIndexEntry kIndexEntries[] = {
  // pos, timestamp, flags, size, min_distance
  {     0,     0, AVINDEX_KEYFRAME, 0, 0 },
  {  2000,  1000, AVINDEX_KEYFRAME, 0, 0 },
  {  3000,  2000,                0, 0, 0 },
  {  5000,  3000, AVINDEX_KEYFRAME, 0, 0 },
  {  6000,  4000,                0, 0, 0 },
  {  8000,  5000, AVINDEX_KEYFRAME, 0, 0 },
  {  9000,  6000, AVINDEX_KEYFRAME, 0, 0 },
  { 11500,  7000, AVINDEX_KEYFRAME, 0, 0 },
};

static const AVRational kTimeBase = { 1, 1000 };

class FFmpegCommonTest : public testing::Test {
 public:
  FFmpegCommonTest();
  virtual ~FFmpegCommonTest();

 protected:
  AVStream stream_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegCommonTest);
};

static bool InitFFmpeg() {
  static bool initialized = false;
  if (initialized) {
    return true;
  }
  base::FilePath path;
  PathService::Get(base::DIR_MODULE, &path);
  return media::InitializeMediaLibrary(path);
}

FFmpegCommonTest::FFmpegCommonTest() {
  CHECK(InitFFmpeg());
  stream_.time_base = kTimeBase;
  stream_.index_entries = kIndexEntries;
  stream_.index_entries_allocated_size = sizeof(kIndexEntries);
  stream_.nb_index_entries = arraysize(kIndexEntries);
}

FFmpegCommonTest::~FFmpegCommonTest() {}

TEST_F(FFmpegCommonTest, TimeBaseConversions) {
  int64 test_data[][5] = {
    {1, 2, 1, 500000, 1 },
    {1, 3, 1, 333333, 1 },
    {1, 3, 2, 666667, 2 },
  };

  for (size_t i = 0; i < arraysize(test_data); ++i) {
    SCOPED_TRACE(i);

    AVRational time_base;
    time_base.num = static_cast<int>(test_data[i][0]);
    time_base.den = static_cast<int>(test_data[i][1]);

    TimeDelta time_delta = ConvertFromTimeBase(time_base, test_data[i][2]);

    EXPECT_EQ(time_delta.InMicroseconds(), test_data[i][3]);
    EXPECT_EQ(ConvertToTimeBase(time_base, time_delta), test_data[i][4]);
  }
}

TEST_F(FFmpegCommonTest, VerifyFormatSizes) {
  for (AVSampleFormat format = AV_SAMPLE_FMT_NONE;
       format < AV_SAMPLE_FMT_NB;
       format = static_cast<AVSampleFormat>(format + 1)) {
    SampleFormat sample_format = AVSampleFormatToSampleFormat(format);
    if (sample_format == kUnknownSampleFormat) {
      // This format not supported, so skip it.
      continue;
    }

    // Have FFMpeg compute the size of a buffer of 1 channel / 1 frame
    // with 1 byte alignment to make sure the sizes match.
    int single_buffer_size = av_samples_get_buffer_size(NULL, 1, 1, format, 1);
    int bytes_per_channel = SampleFormatToBytesPerChannel(sample_format);
    EXPECT_EQ(bytes_per_channel, single_buffer_size);
  }
}

TEST_F(FFmpegCommonTest, UTCDateToTime_Valid) {
  base::Time result;
  EXPECT_TRUE(FFmpegUTCDateToTime("2012-11-10 12:34:56", &result));

  base::Time::Exploded exploded;
  result.UTCExplode(&exploded);
  EXPECT_TRUE(exploded.HasValidValues());
  EXPECT_EQ(2012, exploded.year);
  EXPECT_EQ(11, exploded.month);
  EXPECT_EQ(6, exploded.day_of_week);
  EXPECT_EQ(10, exploded.day_of_month);
  EXPECT_EQ(12, exploded.hour);
  EXPECT_EQ(34, exploded.minute);
  EXPECT_EQ(56, exploded.second);
  EXPECT_EQ(0, exploded.millisecond);
}

TEST_F(FFmpegCommonTest, UTCDateToTime_Invalid) {
  const char* invalid_date_strings[] = {
    "",
    "2012-11-10",
    "12:34:56",
    "-- ::",
    "2012-11-10 12:34:",
    "2012-11-10 12::56",
    "2012-11-10 :34:56",
    "2012-11- 12:34:56",
    "2012--10 12:34:56",
    "-11-10 12:34:56",
    "2012-11 12:34:56",
    "2012-11-10-12 12:34:56",
    "2012-11-10 12:34",
    "2012-11-10 12:34:56:78",
    "ABCD-11-10 12:34:56",
    "2012-EF-10 12:34:56",
    "2012-11-GH 12:34:56",
    "2012-11-10 IJ:34:56",
    "2012-11-10 12:JL:56",
    "2012-11-10 12:34:MN",
    "2012-11-10 12:34:56.123",
    "2012-11-1012:34:56",
    "2012-11-10 12:34:56 UTC",
  };

  for (size_t i = 0; i < arraysize(invalid_date_strings); ++i) {
    const char* date_string = invalid_date_strings[i];
    base::Time result;
    EXPECT_FALSE(FFmpegUTCDateToTime(date_string, &result))
        << "date_string '" << date_string << "'";
    EXPECT_TRUE(result.is_null());
  }
}


}  // namespace media
