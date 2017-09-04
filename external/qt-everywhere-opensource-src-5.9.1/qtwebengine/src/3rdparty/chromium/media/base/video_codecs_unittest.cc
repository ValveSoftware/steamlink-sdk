// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_codecs.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
TEST(ParseHEVCCodecIdTest, InvalidHEVCCodecIds) {
  VideoCodecProfile profile = VIDEO_CODEC_PROFILE_UNKNOWN;
  uint8_t level_idc = 0;

  // Both hev1 and hvc1 should be supported
  EXPECT_TRUE(ParseHEVCCodecId("hev1.1.6.L93.B0", &profile, &level_idc));
  EXPECT_EQ(profile, HEVCPROFILE_MAIN);
  EXPECT_EQ(level_idc, 93);
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L93.B0", &profile, &level_idc));
  EXPECT_EQ(profile, HEVCPROFILE_MAIN);
  EXPECT_EQ(level_idc, 93);

  // Check that codec id string with insufficient number of dot-separated
  // elements are rejected. There must be at least 4 elements: hev1/hvc1 prefix,
  // profile, profile_compatibility, tier+level.
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L93", &profile, &level_idc));
  EXPECT_EQ(profile, HEVCPROFILE_MAIN);
  EXPECT_EQ(level_idc, 93);
  EXPECT_FALSE(ParseHEVCCodecId("hvc1", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hev1", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1..", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1...", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1....", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1..", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1...", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6..", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6...", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1..L93", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1..L93.", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1..L93..", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1..6", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1..6.", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1..6..", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1..6...", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1..6.L93", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1..6.L93.", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1..6.L93..", &profile, &level_idc));

  // Check that codec ids with empty constraint bytes are rejected.
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93.", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93..", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93...", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93....", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93.....", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93......", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93.......", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93.......0", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93.0.", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93.0..", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L93.0..0", &profile, &level_idc));
  EXPECT_FALSE(
      ParseHEVCCodecId("hvc1.1.6.L93.0..0.0.0.0.0", &profile, &level_idc));
  EXPECT_FALSE(
      ParseHEVCCodecId("hvc1.1.6.L93.0.0.0.0.0.0.", &profile, &level_idc));

  // Different variations of general_profile_space (empty, 'A', 'B', 'C')
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L93.B0", &profile, &level_idc));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.A1.6.L93.B0", &profile, &level_idc));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.B1.6.L93.B0", &profile, &level_idc));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.C1.6.L93.B0", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.D1.6.L93.B0", &profile, &level_idc));

  // general_profile_idc (the number after the first dot) must be a 5-bit
  // decimal-encoded number (between 0 and 31)
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.0.6.L93.B0", &profile, &level_idc));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.31.6.L93.B0", &profile, &level_idc));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L93.B0", &profile, &level_idc));
  EXPECT_EQ(profile, HEVCPROFILE_MAIN);
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.2.2.L93.B0", &profile, &level_idc));
  EXPECT_EQ(profile, HEVCPROFILE_MAIN10);
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.3.4.L93.B0", &profile, &level_idc));
  EXPECT_EQ(profile, HEVCPROFILE_MAIN_STILL_PICTURE);
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.-1.6.L93.B0", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.32.6.L93.B0", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.999.6.L93.B0", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.A.6.L93.B0", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1F.6.L93.B0", &profile, &level_idc));

  // general_profile_compatibility_flags is a 32-bit hex number
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.0.L93.B0", &profile, &level_idc));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.FF.L93.B0", &profile, &level_idc));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.FFFF.L93.B0", &profile, &level_idc));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.FFFFFFFF.L93.B0", &profile, &level_idc));
  EXPECT_FALSE(
      ParseHEVCCodecId("hvc1.1.100000000.L93.B0", &profile, &level_idc));
  EXPECT_FALSE(
      ParseHEVCCodecId("hvc1.1.FFFFFFFFF.L93.B0", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.-1.L93.B0", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.0G.L93.B0", &profile, &level_idc));

  // general_tier_flag is encoded as either character 'L' (general_tier_flag==0)
  // or character 'H' (general_tier_flag==1) in the fourth element of the string
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L93.B0", &profile, &level_idc));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.0.H93.B0", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.0.93.B0", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.0.A93.B0", &profile, &level_idc));

  // general_level_idc is 8-bit decimal-encoded number after general_tier_flag.
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L0.B0", &profile, &level_idc));
  EXPECT_EQ(level_idc, 0);
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L1.B0", &profile, &level_idc));
  EXPECT_EQ(level_idc, 1);
  // Level 3.1 (93 == 3.1 * 30)
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L93.B0", &profile, &level_idc));
  EXPECT_EQ(level_idc, 93);
  // Level 5 (150 == 5 * 30)
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L150.B0", &profile, &level_idc));
  EXPECT_EQ(level_idc, 150);
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L255.B0", &profile, &level_idc));
  EXPECT_EQ(level_idc, 255);
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L256.B0", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L999.B0", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L-1.B0", &profile, &level_idc));

  // The elements after the fourth dot are hex-encoded bytes containing
  // constraint flags (up to 6 bytes), trailing zero bytes may be omitted
  EXPECT_TRUE(
      ParseHEVCCodecId("hvc1.1.6.L0.0.0.0.0.0.0", &profile, &level_idc));
  EXPECT_TRUE(
      ParseHEVCCodecId("hvc1.1.6.L0.00.00.00.00.00.00", &profile, &level_idc));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L0.12", &profile, &level_idc));
  EXPECT_TRUE(ParseHEVCCodecId("hvc1.1.6.L0.12.34.56", &profile, &level_idc));
  EXPECT_TRUE(
      ParseHEVCCodecId("hvc1.1.6.L0.12.34.56.78.9A.BC", &profile, &level_idc));
  EXPECT_TRUE(
      ParseHEVCCodecId("hvc1.1.6.L0.FF.FF.FF.FF.FF.FF", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L0.FF.FF.FF.FF.FF.FF.0", &profile,
                                &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L0.100", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L0.1FF", &profile, &level_idc));
  EXPECT_FALSE(ParseHEVCCodecId("hvc1.1.6.L0.-1", &profile, &level_idc));
}
#endif

}  // namespace media
