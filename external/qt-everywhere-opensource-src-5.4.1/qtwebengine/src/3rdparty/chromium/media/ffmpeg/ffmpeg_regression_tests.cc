// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Regression tests for FFmpeg.  Security test files can be found in the
// internal media test data directory:
//
//    svn://svn.chromium.org/chrome-internal/trunk/data/media/security/
//
// Simply add the custom_dep below to your gclient and sync:
//
//    "src/media/test/data/security":
//        "svn://chrome-svn/chrome-internal/trunk/data/media/security"
//
// Many of the files here do not cause issues outside of tooling, so you'll need
// to run this test under ASAN, TSAN, and Valgrind to ensure that all issues are
// caught.
//
// Test cases labeled FLAKY may not always pass, but they should never crash or
// cause any kind of warnings or errors under tooling.
//
// Frame hashes must be generated with --video-threads=1 for correctness.
//
// Known issues:
//    Cr47325 will generate an UninitValue error under Valgrind inside of the
//    MD5 hashing code.  The error occurs due to some problematic error
//    resilence code for H264 inside of FFmpeg.  See http://crbug.com/119020
//
//    Some OGG files leak ~30 bytes of memory, upstream tracking bug:
//    https://ffmpeg.org/trac/ffmpeg/ticket/1244
//
//    Some OGG files leak hundreds of kilobytes of memory, upstream bug:
//    https://ffmpeg.org/trac/ffmpeg/ticket/1931

#include "media/filters/pipeline_integration_test_base.h"

#include "base/bind.h"
#include "media/base/test_data_util.h"

namespace media {

struct RegressionTestData {
  RegressionTestData(const char* filename, PipelineStatus init_status,
                     PipelineStatus end_status, const char* video_md5,
                     const char* audio_md5)
      : video_md5(video_md5),
        audio_md5(audio_md5),
        filename(filename),
        init_status(init_status),
        end_status(end_status) {
  }

  const char* video_md5;
  const char* audio_md5;
  const char* filename;
  PipelineStatus init_status;
  PipelineStatus end_status;
};

// Used for tests which just need to run without crashing or tooling errors, but
// which may have undefined behavior for hashing, etc.
struct FlakyRegressionTestData {
  FlakyRegressionTestData(const char* filename)
      : filename(filename) {
  }

  const char* filename;
};

class FFmpegRegressionTest
    : public testing::TestWithParam<RegressionTestData>,
      public PipelineIntegrationTestBase {
};

class FlakyFFmpegRegressionTest
    : public testing::TestWithParam<FlakyRegressionTestData>,
      public PipelineIntegrationTestBase {
};

#define FFMPEG_TEST_CASE(name, fn, init_status, end_status, video_md5, \
                         audio_md5) \
    INSTANTIATE_TEST_CASE_P(name, FFmpegRegressionTest, \
                            testing::Values(RegressionTestData(fn, \
                                                               init_status, \
                                                               end_status, \
                                                               video_md5, \
                                                               audio_md5)));

#define FLAKY_FFMPEG_TEST_CASE(name, fn) \
    INSTANTIATE_TEST_CASE_P(FLAKY_##name, FlakyFFmpegRegressionTest, \
                            testing::Values(FlakyRegressionTestData(fn)));

// Test cases from issues.
FFMPEG_TEST_CASE(Cr47325, "security/47325.mp4", PIPELINE_OK, PIPELINE_OK,
                 "2a7a938c6b5979621cec998f02d9bbb6",
                 "3.61,1.64,-3.24,0.12,1.50,-0.86,");
FFMPEG_TEST_CASE(Cr47761, "content/crbug47761.ogg", PIPELINE_OK, PIPELINE_OK,
                 kNullVideoHash,
                 "8.89,8.55,8.88,8.01,8.23,7.69,");
FFMPEG_TEST_CASE(Cr50045, "content/crbug50045.mp4", PIPELINE_OK, PIPELINE_OK,
                 "c345e9ef9ebfc6bfbcbe3f0ddc3125ba",
                 "2.72,-6.27,-6.11,-3.17,-5.58,1.26,");
FFMPEG_TEST_CASE(Cr62127, "content/crbug62127.webm", PIPELINE_OK,
                 PIPELINE_OK, "a064b2776fc5aef3e9cba47967a75db9",
                 kNullAudioHash);
FFMPEG_TEST_CASE(Cr93620, "security/93620.ogg", PIPELINE_OK, PIPELINE_OK,
                 kNullVideoHash,
                 "-10.55,-10.10,-10.42,-10.35,-10.29,-10.72,");
FFMPEG_TEST_CASE(Cr100492, "security/100492.webm", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED, kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(Cr100543, "security/100543.webm", PIPELINE_OK, PIPELINE_OK,
                 "c16691cc9178db3adbf7e562cadcd6e6",
                 "1211.73,304.89,1311.54,371.34,1283.06,299.63,");
FFMPEG_TEST_CASE(Cr101458, "security/101458.webm", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED, kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(Cr108416, "security/108416.webm", PIPELINE_OK, PIPELINE_OK,
                 "5cb3a934795cd552753dec7687928291",
                 "-17.87,-37.20,-23.33,45.57,8.13,-9.92,");
FFMPEG_TEST_CASE(Cr110849, "security/110849.mkv",
                 DEMUXER_ERROR_COULD_NOT_OPEN,
                 DEMUXER_ERROR_NO_SUPPORTED_STREAMS,
                 kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(Cr112384, "security/112384.webm",
                 DEMUXER_ERROR_COULD_NOT_PARSE, DEMUXER_ERROR_COULD_NOT_PARSE,
                 kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(Cr117912, "security/117912.webm", DEMUXER_ERROR_COULD_NOT_OPEN,
                 DEMUXER_ERROR_COULD_NOT_OPEN, kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(Cr123481, "security/123481.ogv", PIPELINE_OK,
                 PIPELINE_OK, "e6dd853fcbd746c8bb2ab2b8fc376fc7",
                 "1.28,-0.32,-0.81,0.08,1.66,0.89,");
FFMPEG_TEST_CASE(Cr132779, "security/132779.webm",
                 DEMUXER_ERROR_COULD_NOT_PARSE, DEMUXER_ERROR_COULD_NOT_PARSE,
                 kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(Cr140165, "security/140165.ogg", PIPELINE_ERROR_DECODE,
                 PIPELINE_ERROR_DECODE, kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(Cr140647, "security/140647.ogv", DEMUXER_ERROR_COULD_NOT_OPEN,
                 DEMUXER_ERROR_COULD_NOT_OPEN, kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(Cr142738, "content/crbug142738.ogg", PIPELINE_OK, PIPELINE_OK,
                 kNullVideoHash,
                 "-1.22,0.45,1.79,1.80,-0.30,-1.21,");
FFMPEG_TEST_CASE(Cr152691, "security/152691.mp3", PIPELINE_ERROR_DECODE,
                 PIPELINE_ERROR_DECODE, kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(Cr161639, "security/161639.m4a", PIPELINE_ERROR_DECODE,
                 PIPELINE_ERROR_DECODE, kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(Cr222754, "security/222754.mp4", PIPELINE_ERROR_DECODE,
                 PIPELINE_ERROR_DECODE, kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(Cr234630a, "security/234630a.mov", PIPELINE_OK,
                 PIPELINE_OK, kNullVideoHash,
                 "-15.52,-18.90,-15.33,-16.68,-14.41,-15.89,");
FFMPEG_TEST_CASE(Cr234630b, "security/234630b.mov", PIPELINE_ERROR_DECODE,
                 PIPELINE_ERROR_DECODE, kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(Cr242786, "security/242786.webm", PIPELINE_OK,
                 PIPELINE_OK, kNullVideoHash,
                 "-1.72,-0.83,0.84,1.70,1.23,-0.53,");
// Test for out-of-bounds access with slightly corrupt file (detection logic
// thinks it's a MONO file, but actually contains STEREO audio).
FFMPEG_TEST_CASE(Cr275590, "security/275590.m4a",
                 DECODER_ERROR_NOT_SUPPORTED, DEMUXER_ERROR_COULD_NOT_OPEN,
                 kNullVideoHash, kNullAudioHash);

// General MP4 test cases.
FFMPEG_TEST_CASE(MP4_0, "security/aac.10419.mp4", DEMUXER_ERROR_COULD_NOT_OPEN,
                 DEMUXER_ERROR_COULD_NOT_OPEN, kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(MP4_1, "security/clockh264aac_200021889.mp4",
                 DEMUXER_ERROR_COULD_NOT_OPEN, DEMUXER_ERROR_COULD_NOT_OPEN,
                 kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(MP4_2, "security/clockh264aac_200701257.mp4", PIPELINE_OK,
                 PIPELINE_OK, kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(MP4_5, "security/clockh264aac_3022500.mp4",
                 DEMUXER_ERROR_NO_SUPPORTED_STREAMS,
                 DEMUXER_ERROR_NO_SUPPORTED_STREAMS,
                 kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(MP4_6, "security/clockh264aac_344289.mp4", PIPELINE_OK,
                 PIPELINE_OK, kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(MP4_7, "security/clockh264mp3_187697.mp4",
                 DEMUXER_ERROR_NO_SUPPORTED_STREAMS,
                 DEMUXER_ERROR_NO_SUPPORTED_STREAMS,
                 kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(MP4_8, "security/h264.705767.mp4",
                 DEMUXER_ERROR_COULD_NOT_PARSE, DEMUXER_ERROR_COULD_NOT_PARSE,
                 kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(MP4_9, "security/smclockmp4aac_1_0.mp4",
                 DEMUXER_ERROR_COULD_NOT_OPEN, DEMUXER_ERROR_COULD_NOT_OPEN,
                 kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(MP4_16, "security/looping2.mov",
                 DEMUXER_ERROR_COULD_NOT_OPEN, DEMUXER_ERROR_COULD_NOT_OPEN,
                 kNullVideoHash, kNullAudioHash);

// General OGV test cases.
FFMPEG_TEST_CASE(OGV_1, "security/out.163.ogv", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED, kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(OGV_2, "security/out.391.ogv", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED, kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(OGV_5, "security/smclocktheora_1_0.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(OGV_7, "security/smclocktheora_1_102.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(OGV_8, "security/smclocktheora_1_104.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(OGV_9, "security/smclocktheora_1_110.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(OGV_10, "security/smclocktheora_1_179.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(OGV_11, "security/smclocktheora_1_20.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(OGV_12, "security/smclocktheora_1_723.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(OGV_14, "security/smclocktheora_2_10405.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(OGV_15, "security/smclocktheora_2_10619.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(OGV_16, "security/smclocktheora_2_1075.ogv",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(OGV_18, "security/wav.711.ogv", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED, kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(OGV_19, "security/null1.ogv", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED, kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(OGV_20, "security/null2.ogv", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED, kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(OGV_21, "security/assert1.ogv", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED, kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(OGV_22, "security/assert2.ogv", DECODER_ERROR_NOT_SUPPORTED,
                 DECODER_ERROR_NOT_SUPPORTED, kNullVideoHash, kNullAudioHash);

// General WebM test cases.
FFMPEG_TEST_CASE(WEBM_1, "security/no-bug.webm", PIPELINE_OK, PIPELINE_OK,
                 "39e92700cbb77478fd63f49db855e7e5", kNullAudioHash);
FFMPEG_TEST_CASE(WEBM_3, "security/out.webm.139771.2965",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(WEBM_4, "security/out.webm.68798.1929",
                 DECODER_ERROR_NOT_SUPPORTED, DECODER_ERROR_NOT_SUPPORTED,
                 kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(WEBM_5, "content/frame_size_change.webm", PIPELINE_OK,
                 PIPELINE_OK, "d8fcf2896b7400a2261bac9e9ea930f8",
                 kNullAudioHash);

// Audio Functional Tests
FFMPEG_TEST_CASE(AUDIO_GAMING_0, "content/gaming/a_220_00.mp3", PIPELINE_OK,
                 PIPELINE_OK, kNullVideoHash,
                 "0.36,1.25,2.98,4.29,4.19,2.76,");
FFMPEG_TEST_CASE(AUDIO_GAMING_1, "content/gaming/a_220_00_v2.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullVideoHash,
                 "2.17,3.31,5.15,6.33,5.97,4.35,");
FFMPEG_TEST_CASE(AUDIO_GAMING_2, "content/gaming/ai_laser1.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullVideoHash,
                 "7.70,10.81,13.19,10.07,7.39,7.56,");
FFMPEG_TEST_CASE(AUDIO_GAMING_3, "content/gaming/ai_laser2.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullVideoHash,
                 "5.99,8.04,9.71,8.69,7.81,7.52,");
FFMPEG_TEST_CASE(AUDIO_GAMING_4, "content/gaming/ai_laser3.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullVideoHash,
                 "-0.32,1.44,3.75,5.88,6.32,3.22,");
FFMPEG_TEST_CASE(AUDIO_GAMING_5, "content/gaming/ai_laser4.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullVideoHash,
                 "4.75,4.16,2.21,3.01,5.51,6.11,");
FFMPEG_TEST_CASE(AUDIO_GAMING_6, "content/gaming/ai_laser5.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullVideoHash,
                 "6.04,7.46,8.78,7.32,4.16,3.97,");
FFMPEG_TEST_CASE(AUDIO_GAMING_7, "content/gaming/footstep1.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullVideoHash,
                 "-0.50,0.29,2.35,4.79,5.14,2.24,");
FFMPEG_TEST_CASE(AUDIO_GAMING_8, "content/gaming/footstep3.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullVideoHash,
                 "-2.87,-3.05,-4.10,-3.20,-2.20,-2.20,");
FFMPEG_TEST_CASE(AUDIO_GAMING_9, "content/gaming/footstep4.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullVideoHash,
                 "10.35,10.74,11.60,12.83,12.69,10.67,");
FFMPEG_TEST_CASE(AUDIO_GAMING_10, "content/gaming/laser1.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullVideoHash,
                 "-9.48,-12.94,-1.75,7.66,5.61,-0.58,");
FFMPEG_TEST_CASE(AUDIO_GAMING_11, "content/gaming/laser2.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullVideoHash,
                 "-7.53,-6.28,3.37,0.73,-5.83,-4.70,");
FFMPEG_TEST_CASE(AUDIO_GAMING_12, "content/gaming/laser3.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullVideoHash,
                 "-13.62,-6.55,2.52,-10.10,-10.68,-5.43,");
FFMPEG_TEST_CASE(AUDIO_GAMING_13, "content/gaming/leg1.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullVideoHash,
                 "5.62,5.79,5.81,5.60,6.18,6.15,");
FFMPEG_TEST_CASE(AUDIO_GAMING_14, "content/gaming/leg2.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullVideoHash,
                 "-0.88,1.32,2.74,3.07,0.88,-0.03,");
FFMPEG_TEST_CASE(AUDIO_GAMING_15, "content/gaming/leg3.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullVideoHash,
                 "17.77,18.59,19.57,18.84,17.62,17.22,");
FFMPEG_TEST_CASE(AUDIO_GAMING_16, "content/gaming/lock_on.ogg", PIPELINE_OK,
                 PIPELINE_OK, kNullVideoHash,
                 "3.08,-4.33,-5.04,-0.24,1.83,5.16,");
FFMPEG_TEST_CASE(AUDIO_GAMING_17, "content/gaming/enemy_lock_on.ogg",
                 PIPELINE_OK, PIPELINE_OK, kNullVideoHash,
                 "-2.24,-1.00,-2.75,-0.87,1.11,-0.58,");
FFMPEG_TEST_CASE(AUDIO_GAMING_18, "content/gaming/rocket_launcher.mp3",
                 PIPELINE_OK, PIPELINE_OK, kNullVideoHash,
                 "-3.08,0.18,2.49,1.98,-2.20,-4.74,");

// Allocate gigabytes of memory, likely can't be run on 32bit machines.
FFMPEG_TEST_CASE(BIG_MEM_1, "security/bigmem1.mov",
                 DEMUXER_ERROR_COULD_NOT_OPEN, DEMUXER_ERROR_COULD_NOT_OPEN,
                 kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(BIG_MEM_2, "security/looping1.mov",
                 DEMUXER_ERROR_COULD_NOT_OPEN, DEMUXER_ERROR_COULD_NOT_OPEN,
                 kNullVideoHash, kNullAudioHash);
FFMPEG_TEST_CASE(BIG_MEM_5, "security/looping5.mov",
                 DEMUXER_ERROR_COULD_NOT_OPEN, DEMUXER_ERROR_COULD_NOT_OPEN,
                 kNullVideoHash, kNullAudioHash);
FLAKY_FFMPEG_TEST_CASE(BIG_MEM_3, "security/looping3.mov");
FLAKY_FFMPEG_TEST_CASE(BIG_MEM_4, "security/looping4.mov");

// Flaky under threading or for other reasons.  Per rbultje, most of these will
// never be reliable since FFmpeg does not guarantee consistency in error cases.
// We only really care that these don't cause crashes or errors under tooling.
FLAKY_FFMPEG_TEST_CASE(Cr99652, "security/99652.webm");
FLAKY_FFMPEG_TEST_CASE(Cr100464, "security/100464.webm");
FLAKY_FFMPEG_TEST_CASE(Cr111342, "security/111342.ogm");
FLAKY_FFMPEG_TEST_CASE(Cr368980, "security/368980.mp4");
FLAKY_FFMPEG_TEST_CASE(OGV_0, "security/big_dims.ogv");
FLAKY_FFMPEG_TEST_CASE(OGV_3, "security/smclock_1_0.ogv");
FLAKY_FFMPEG_TEST_CASE(OGV_4, "security/smclock.ogv.1.0.ogv");
FLAKY_FFMPEG_TEST_CASE(OGV_6, "security/smclocktheora_1_10000.ogv");
FLAKY_FFMPEG_TEST_CASE(OGV_13, "security/smclocktheora_1_790.ogv");
FLAKY_FFMPEG_TEST_CASE(MP4_3, "security/clockh264aac_300413969.mp4");
FLAKY_FFMPEG_TEST_CASE(MP4_4, "security/clockh264aac_301350139.mp4");
FLAKY_FFMPEG_TEST_CASE(MP4_12, "security/assert1.mov");
// Not really flaky, but can't pass the seek test.
FLAKY_FFMPEG_TEST_CASE(MP4_10, "security/null1.m4a");

// TODO(wolenetz/dalecurtis): The following have flaky audio hash result.
// See http://crbug.com/237371
FLAKY_FFMPEG_TEST_CASE(Cr112976, "security/112976.ogg");
FLAKY_FFMPEG_TEST_CASE(MKV_0, "security/nested_tags_lang.mka.627.628");
FLAKY_FFMPEG_TEST_CASE(MKV_1, "security/nested_tags_lang.mka.667.628");
FLAKY_FFMPEG_TEST_CASE(MP4_11, "security/null1.mp4");

// TODO(wolenetz/dalecurtis): The following have flaky init status: on mac
// ia32 Chrome, observed PIPELINE_OK instead of DECODER_ERROR_NOT_SUPPORTED.
FLAKY_FFMPEG_TEST_CASE(Cr112670, "security/112670.mp4");
FLAKY_FFMPEG_TEST_CASE(OGV_17, "security/vorbis.482086.ogv");

// TODO(wolenetz/dalecurtis): The following have flaky init status: on mac
// ia32 Chrome, observed DUMUXER_ERROR_NO_SUPPORTED_STREAMS instead of
// DECODER_ERROR_NOT_SUPPORTED.
FLAKY_FFMPEG_TEST_CASE(Cr116927, "security/116927.ogv");
FLAKY_FFMPEG_TEST_CASE(WEBM_2, "security/uninitialize.webm");

// Videos with massive gaps between frame timestamps that result in long hangs
// with our pipeline.  Should be uncommented when we support clockless playback.
// FFMPEG_TEST_CASE(WEBM_0, "security/memcpy.webm", PIPELINE_OK, PIPELINE_OK,
//                  kNullVideoHash, kNullAudioHash);
// FFMPEG_TEST_CASE(MP4_17, "security/assert2.mov", PIPELINE_OK, PIPELINE_OK,
//                  kNullVideoHash, kNullAudioHash);
// FFMPEG_TEST_CASE(OGV_23, "security/assert2.ogv", PIPELINE_OK, PIPELINE_OK,
//                  kNullVideoHash, kNullAudioHash);

TEST_P(FFmpegRegressionTest, BasicPlayback) {
  if (GetParam().init_status == PIPELINE_OK) {
    ASSERT_TRUE(Start(GetTestDataFilePath(GetParam().filename),
                      GetParam().init_status, kHashed));
    Play();
    ASSERT_EQ(WaitUntilEndedOrError(), GetParam().end_status);
    EXPECT_EQ(GetParam().video_md5, GetVideoHash());
    EXPECT_EQ(GetParam().audio_md5, GetAudioHash());

    // Check for ended if the pipeline is expected to finish okay.
    if (GetParam().end_status == PIPELINE_OK) {
      ASSERT_TRUE(ended_);

      // Tack a seek on the end to catch any seeking issues.
      Seek(base::TimeDelta::FromMilliseconds(0));
    }
  } else {
    ASSERT_FALSE(Start(GetTestDataFilePath(GetParam().filename),
                       GetParam().init_status, kHashed));
    EXPECT_EQ(GetParam().video_md5, GetVideoHash());
    EXPECT_EQ(GetParam().audio_md5, GetAudioHash());
  }
}

TEST_P(FlakyFFmpegRegressionTest, BasicPlayback) {
  if (Start(GetTestDataFilePath(GetParam().filename))) {
    Play();
    WaitUntilEndedOrError();
  }
}

}  // namespace media
