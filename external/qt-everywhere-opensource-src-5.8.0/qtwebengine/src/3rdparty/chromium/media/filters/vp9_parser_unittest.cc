// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "media/base/test_data_util.h"
#include "media/filters/ivf_parser.h"
#include "media/filters/vp9_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class Vp9ParserTest : public ::testing::Test {
 protected:
  void SetUp() override {
    base::FilePath file_path = GetTestDataFilePath("test-25fps.vp9");

    stream_.reset(new base::MemoryMappedFile());
    ASSERT_TRUE(stream_->Initialize(file_path)) << "Couldn't open stream file: "
                                                << file_path.MaybeAsASCII();

    IvfFileHeader ivf_file_header;
    ASSERT_TRUE(ivf_parser_.Initialize(stream_->data(), stream_->length(),
                                       &ivf_file_header));
    ASSERT_EQ(ivf_file_header.fourcc, 0x30395056u);  // VP90
  }

  void TearDown() override { stream_.reset(); }

  bool ParseNextFrame(struct Vp9FrameHeader* frame_hdr);

  const Vp9Segmentation& GetSegmentation() const {
    return vp9_parser_.GetSegmentation();
  }

  const Vp9LoopFilter& GetLoopFilter() const {
    return vp9_parser_.GetLoopFilter();
  }

  IvfParser ivf_parser_;
  std::unique_ptr<base::MemoryMappedFile> stream_;

  Vp9Parser vp9_parser_;
};

bool Vp9ParserTest::ParseNextFrame(Vp9FrameHeader* fhdr) {
  while (1) {
    Vp9Parser::Result res = vp9_parser_.ParseNextFrame(fhdr);
    if (res == Vp9Parser::kEOStream) {
      IvfFrameHeader ivf_frame_header;
      const uint8_t* ivf_payload;

      if (!ivf_parser_.ParseNextFrame(&ivf_frame_header, &ivf_payload))
        return false;

      vp9_parser_.SetStream(ivf_payload, ivf_frame_header.frame_size);
      continue;
    }

    return res == Vp9Parser::kOk;
  }
}

TEST_F(Vp9ParserTest, StreamFileParsing) {
  // Number of frames in the test stream to be parsed.
  const int num_frames = 250;
  int num_parsed_frames = 0;

  while (num_parsed_frames < num_frames) {
    Vp9FrameHeader fhdr;
    if (!ParseNextFrame(&fhdr))
      break;

    ++num_parsed_frames;
  }

  DVLOG(1) << "Number of successfully parsed frames before EOS: "
           << num_parsed_frames;

  EXPECT_EQ(num_frames, num_parsed_frames);
}

TEST_F(Vp9ParserTest, VerifyFirstFrame) {
  Vp9FrameHeader fhdr;

  ASSERT_TRUE(ParseNextFrame(&fhdr));

  EXPECT_EQ(0, fhdr.profile);
  EXPECT_FALSE(fhdr.show_existing_frame);
  EXPECT_EQ(Vp9FrameHeader::KEYFRAME, fhdr.frame_type);
  EXPECT_TRUE(fhdr.show_frame);
  EXPECT_FALSE(fhdr.error_resilient_mode);

  EXPECT_EQ(8, fhdr.bit_depth);
  EXPECT_EQ(Vp9ColorSpace::UNKNOWN, fhdr.color_space);
  EXPECT_FALSE(fhdr.yuv_range);
  EXPECT_EQ(1, fhdr.subsampling_x);
  EXPECT_EQ(1, fhdr.subsampling_y);

  EXPECT_EQ(320u, fhdr.width);
  EXPECT_EQ(240u, fhdr.height);
  EXPECT_EQ(320u, fhdr.display_width);
  EXPECT_EQ(240u, fhdr.display_height);

  EXPECT_TRUE(fhdr.refresh_frame_context);
  EXPECT_TRUE(fhdr.frame_parallel_decoding_mode);
  EXPECT_EQ(0, fhdr.frame_context_idx);

  const Vp9LoopFilter& lf = GetLoopFilter();
  EXPECT_EQ(9, lf.filter_level);
  EXPECT_EQ(0, lf.sharpness_level);
  EXPECT_TRUE(lf.mode_ref_delta_enabled);
  EXPECT_TRUE(lf.mode_ref_delta_update);
  EXPECT_TRUE(lf.update_ref_deltas[0]);
  EXPECT_EQ(1, lf.ref_deltas[0]);
  EXPECT_EQ(-1, lf.ref_deltas[2]);
  EXPECT_EQ(-1, lf.ref_deltas[3]);

  const Vp9QuantizationParams& qp = fhdr.quant_params;
  EXPECT_EQ(65, qp.base_qindex);
  EXPECT_FALSE(qp.y_dc_delta);
  EXPECT_FALSE(qp.uv_dc_delta);
  EXPECT_FALSE(qp.uv_ac_delta);
  EXPECT_FALSE(qp.IsLossless());

  const Vp9Segmentation& seg = GetSegmentation();
  EXPECT_FALSE(seg.enabled);

  EXPECT_EQ(0, fhdr.log2_tile_cols);
  EXPECT_EQ(0, fhdr.log2_tile_rows);

  EXPECT_EQ(120u, fhdr.first_partition_size);
  EXPECT_EQ(18u, fhdr.uncompressed_header_size);
}

TEST_F(Vp9ParserTest, VerifyInterFrame) {
  Vp9FrameHeader fhdr;

  // To verify the second frame.
  for (int i = 0; i < 2; i++)
    ASSERT_TRUE(ParseNextFrame(&fhdr));

  EXPECT_EQ(Vp9FrameHeader::INTERFRAME, fhdr.frame_type);
  EXPECT_FALSE(fhdr.show_frame);
  EXPECT_FALSE(fhdr.intra_only);
  EXPECT_FALSE(fhdr.reset_context);
  EXPECT_TRUE(fhdr.RefreshFlag(2));
  EXPECT_EQ(0, fhdr.frame_refs[0]);
  EXPECT_EQ(1, fhdr.frame_refs[1]);
  EXPECT_EQ(2, fhdr.frame_refs[2]);
  EXPECT_TRUE(fhdr.allow_high_precision_mv);
  EXPECT_EQ(Vp9InterpFilter::EIGHTTAP, fhdr.interp_filter);

  EXPECT_EQ(48u, fhdr.first_partition_size);
  EXPECT_EQ(11u, fhdr.uncompressed_header_size);
}

}  // namespace media
