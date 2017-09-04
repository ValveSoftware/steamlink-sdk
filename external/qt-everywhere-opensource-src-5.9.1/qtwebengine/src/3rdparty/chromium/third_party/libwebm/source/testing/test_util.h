// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef LIBWEBM_TESTING_TEST_UTIL_H_
#define LIBWEBM_TESTING_TEST_UTIL_H_

#include <stdint.h>

#include <cstddef>
#include <string>

namespace test {

// constants for muxer and parser tests
const char kAppString[] = "mkvmuxer_unit_tests";
const char kOpusCodecId[] = "A_OPUS";
const char kVorbisCodecId[] = "A_VORBIS";
const int kAudioTrackNumber = 2;
const int kBitDepth = 2;
const int kChannels = 2;
const double kDuration = 2.345;
const int kFrameLength = 10;
const int kHeight = 180;
const int kInvalidTrackNumber = 100;
const std::uint64_t kOpusCodecDelay = 6500000;
const std::size_t kOpusPrivateDataSizeMinimum = 19;
const std::uint64_t kOpusSeekPreroll = 80000000;
const char kMetadataCodecId[] = "D_WEBVTT/METADATA";
const int kMetadataTrackNumber = 3;
const int kMetadataTrackType = 0x21;
const int kSampleRate = 30;
const int kTimeCodeScale = 1000;
const char kTrackName[] = "unit_test";
const char kVP8CodecId[] = "V_VP8";
const char kVP9CodecId[] = "V_VP9";
const double kVideoFrameRate = 0.5;
const int kVideoTrackNumber = 1;
const int kWidth = 320;

// Returns the path to the test data directory by reading and returning the
// contents the LIBWEBM_TESTDATA_DIR environment variable.
std::string GetTestDataDir();

// Returns the absolute path to the file of |name| in LIBWEBM_TESTDATA_DIR.
std::string GetTestFilePath(const std::string& name);

// Byte-wise comparison of two files |file1| and |file2|. Returns true if the
// files match exactly, false otherwise.
bool CompareFiles(const std::string& file1, const std::string& file2);

}  // namespace test

#endif  // LIBWEBM_TESTING_TEST_UTIL_H_