// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <memory>

#include "base/command_line.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/test_data_util.h"
#include "media/filters/h264_parser.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class H264SPSTest : public ::testing::Test {
 public:
  // An exact clone of an SPS from Big Buck Bunny 480p.
  std::unique_ptr<H264SPS> MakeSPS_BBB480p() {
    std::unique_ptr<H264SPS> sps = base::MakeUnique<H264SPS>();
    sps->profile_idc = 100;
    sps->level_idc = 30;
    sps->chroma_format_idc = 1;
    sps->log2_max_pic_order_cnt_lsb_minus4 = 2;
    sps->max_num_ref_frames = 5;
    sps->pic_width_in_mbs_minus1 = 52;
    sps->pic_height_in_map_units_minus1 = 29;
    sps->frame_mbs_only_flag = true;
    sps->direct_8x8_inference_flag = true;
    sps->vui_parameters_present_flag = true;
    sps->timing_info_present_flag = true;
    sps->num_units_in_tick = 1;
    sps->time_scale = 48;
    sps->fixed_frame_rate_flag = true;
    sps->bitstream_restriction_flag = true;
    // These next three fields are not part of our SPS struct yet.
    // sps->motion_vectors_over_pic_boundaries_flag = true;
    // sps->log2_max_mv_length_horizontal = 10;
    // sps->log2_max_mv_length_vertical = 10;
    sps->max_num_reorder_frames = 2;
    sps->max_dec_frame_buffering = 5;

    // Computed field, matches |chroma_format_idc| in this case.
    // TODO(sandersd): Extract that computation from the parsing step.
    sps->chroma_array_type = 1;

    return sps;
  }
};

TEST_F(H264SPSTest, GetCodedSize) {
  std::unique_ptr<H264SPS> sps = MakeSPS_BBB480p();
  EXPECT_EQ(gfx::Size(848, 480), sps->GetCodedSize());

  // Overflow.
  sps->pic_width_in_mbs_minus1 = std::numeric_limits<int>::max();
  EXPECT_EQ(base::nullopt, sps->GetCodedSize());
}

TEST_F(H264SPSTest, GetVisibleRect) {
  std::unique_ptr<H264SPS> sps = MakeSPS_BBB480p();
  EXPECT_EQ(gfx::Rect(0, 0, 848, 480), sps->GetVisibleRect());

  // Add some cropping.
  sps->frame_cropping_flag = true;
  sps->frame_crop_left_offset = 1;
  sps->frame_crop_right_offset = 2;
  sps->frame_crop_top_offset = 3;
  sps->frame_crop_bottom_offset = 4;
  EXPECT_EQ(gfx::Rect(2, 6, 848 - 6, 480 - 14), sps->GetVisibleRect());

  // Not quite invalid.
  sps->frame_crop_left_offset = 422;
  sps->frame_crop_right_offset = 1;
  sps->frame_crop_top_offset = 0;
  sps->frame_crop_bottom_offset = 0;
  EXPECT_EQ(gfx::Rect(844, 0, 2, 480), sps->GetVisibleRect());

  // Invalid crop.
  sps->frame_crop_left_offset = 423;
  sps->frame_crop_right_offset = 1;
  sps->frame_crop_top_offset = 0;
  sps->frame_crop_bottom_offset = 0;
  EXPECT_EQ(base::nullopt, sps->GetVisibleRect());

  // Overflow.
  sps->frame_crop_left_offset = std::numeric_limits<int>::max() / 2 + 1;
  sps->frame_crop_right_offset = 0;
  sps->frame_crop_top_offset = 0;
  sps->frame_crop_bottom_offset = 0;
  EXPECT_EQ(base::nullopt, sps->GetVisibleRect());
}

TEST(H264ParserTest, StreamFileParsing) {
  base::FilePath file_path = GetTestDataFilePath("test-25fps.h264");
  // Number of NALUs in the test stream to be parsed.
  int num_nalus = 759;

  base::MemoryMappedFile stream;
  ASSERT_TRUE(stream.Initialize(file_path))
      << "Couldn't open stream file: " << file_path.MaybeAsASCII();

  H264Parser parser;
  parser.SetStream(stream.data(), stream.length());

  // Parse until the end of stream/unsupported stream/error in stream is found.
  int num_parsed_nalus = 0;
  while (true) {
    media::H264SliceHeader shdr;
    media::H264SEIMessage sei_msg;
    H264NALU nalu;
    H264Parser::Result res = parser.AdvanceToNextNALU(&nalu);
    if (res == H264Parser::kEOStream) {
      DVLOG(1) << "Number of successfully parsed NALUs before EOS: "
               << num_parsed_nalus;
      ASSERT_EQ(num_nalus, num_parsed_nalus);
      return;
    }
    ASSERT_EQ(res, H264Parser::kOk);

    ++num_parsed_nalus;

    int id;
    switch (nalu.nal_unit_type) {
      case H264NALU::kIDRSlice:
      case H264NALU::kNonIDRSlice:
        ASSERT_EQ(parser.ParseSliceHeader(nalu, &shdr), H264Parser::kOk);
        break;

      case H264NALU::kSPS:
        ASSERT_EQ(parser.ParseSPS(&id), H264Parser::kOk);
        break;

      case H264NALU::kPPS:
        ASSERT_EQ(parser.ParsePPS(&id), H264Parser::kOk);
        break;

      case H264NALU::kSEIMessage:
        ASSERT_EQ(parser.ParseSEI(&sei_msg), H264Parser::kOk);
        break;

      default:
        // Skip unsupported NALU.
        DVLOG(4) << "Skipping unsupported NALU";
        break;
    }
  }
}

}  // namespace media
