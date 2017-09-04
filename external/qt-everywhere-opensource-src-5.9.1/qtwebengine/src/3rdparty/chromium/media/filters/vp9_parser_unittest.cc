// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For each sample vp9 test video, $filename, there is a file of golden value
// of frame entropy, named $filename.context. These values are dumped from
// libvpx.
//
// The syntax of these context dump is described as follows.  For every
// frame, there are corresponding data in context file,
// 1. [initial] [current] [should_update=0], or
// 2. [initial] [current] [should_update=1] [update]
// The first two are expected frame entropy, fhdr->initial_frame_context and
// fhdr->frame_context.
// If |should_update| is true, it follows by the frame context to update.
#include <stdint.h>
#include <string.h>

#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "media/base/test_data_util.h"
#include "media/filters/ivf_parser.h"
#include "media/filters/vp9_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class Vp9ParserTest : public ::testing::Test {
 protected:
  void TearDown() override {
    stream_.reset();
    vp9_parser_.reset();
    context_file_.Close();
  }

  void Initialize(const std::string& filename, bool parsing_compressed_header) {
    base::FilePath file_path = GetTestDataFilePath(filename);

    stream_.reset(new base::MemoryMappedFile());
    ASSERT_TRUE(stream_->Initialize(file_path)) << "Couldn't open stream file: "
                                                << file_path.MaybeAsASCII();

    IvfFileHeader ivf_file_header;
    ASSERT_TRUE(ivf_parser_.Initialize(stream_->data(), stream_->length(),
                                       &ivf_file_header));
    ASSERT_EQ(ivf_file_header.fourcc, 0x30395056u);  // VP90

    vp9_parser_.reset(new Vp9Parser(parsing_compressed_header));

    if (parsing_compressed_header) {
      base::FilePath context_path = GetTestDataFilePath(filename + ".context");
      context_file_.Initialize(context_path,
                               base::File::FLAG_OPEN | base::File::FLAG_READ);
      ASSERT_TRUE(context_file_.IsValid());
    }
  }

  bool ReadShouldContextUpdate() {
    char should_update;
    int read_num = context_file_.ReadAtCurrentPos(&should_update, 1);
    CHECK_EQ(1, read_num);
    return should_update != 0;
  }

  void ReadContext(Vp9FrameContext* frame_context) {
    ASSERT_EQ(
        static_cast<int>(sizeof(*frame_context)),
        context_file_.ReadAtCurrentPos(reinterpret_cast<char*>(frame_context),
                                       sizeof(*frame_context)));
  }

  Vp9Parser::Result ParseNextFrame(struct Vp9FrameHeader* frame_hdr);

  const Vp9SegmentationParams& GetSegmentation() const {
    return vp9_parser_->context().segmentation();
  }

  const Vp9LoopFilterParams& GetLoopFilter() const {
    return vp9_parser_->context().loop_filter();
  }

  Vp9Parser::ContextRefreshCallback GetContextRefreshCb(
      const Vp9FrameHeader& frame_hdr) const {
    return vp9_parser_->GetContextRefreshCb(frame_hdr.frame_context_idx);
  }

  IvfParser ivf_parser_;
  std::unique_ptr<base::MemoryMappedFile> stream_;

  std::unique_ptr<Vp9Parser> vp9_parser_;
  base::File context_file_;
};

Vp9Parser::Result Vp9ParserTest::ParseNextFrame(Vp9FrameHeader* fhdr) {
  while (1) {
    Vp9Parser::Result res = vp9_parser_->ParseNextFrame(fhdr);
    if (res == Vp9Parser::kEOStream) {
      IvfFrameHeader ivf_frame_header;
      const uint8_t* ivf_payload;

      if (!ivf_parser_.ParseNextFrame(&ivf_frame_header, &ivf_payload))
        return Vp9Parser::kEOStream;

      vp9_parser_->SetStream(ivf_payload, ivf_frame_header.frame_size);
      continue;
    }

    return res;
  }
}

TEST_F(Vp9ParserTest, StreamFileParsingWithoutCompressedHeader) {
  Initialize("test-25fps.vp9", false);

  // Number of frames in the test stream to be parsed.
  const int num_expected_frames = 269;
  int num_parsed_frames = 0;

  // Allow to parse twice as many frames in order to detect any extra frames
  // parsed.
  while (num_parsed_frames < num_expected_frames * 2) {
    Vp9FrameHeader fhdr;
    if (ParseNextFrame(&fhdr) != Vp9Parser::kOk)
      break;

    ++num_parsed_frames;
  }

  DVLOG(1) << "Number of successfully parsed frames before EOS: "
           << num_parsed_frames;

  EXPECT_EQ(num_expected_frames, num_parsed_frames);
}

TEST_F(Vp9ParserTest, StreamFileParsingWithCompressedHeader) {
  Initialize("test-25fps.vp9", true);

  // Number of frames in the test stream to be parsed.
  const int num_expected_frames = 269;
  int num_parsed_frames = 0;

  // Allow to parse twice as many frames in order to detect any extra frames
  // parsed.
  while (num_parsed_frames < num_expected_frames * 2) {
    Vp9FrameHeader fhdr;
    if (ParseNextFrame(&fhdr) != Vp9Parser::kOk)
      break;

    Vp9FrameContext frame_context;
    ReadContext(&frame_context);
    EXPECT_TRUE(memcmp(&frame_context, &fhdr.initial_frame_context,
                       sizeof(frame_context)) == 0);
    ReadContext(&frame_context);
    EXPECT_TRUE(memcmp(&frame_context, &fhdr.frame_context,
                       sizeof(frame_context)) == 0);

    // test-25fps.vp9 doesn't need frame update from driver.
    auto context_refresh_cb = GetContextRefreshCb(fhdr);
    EXPECT_TRUE(context_refresh_cb.is_null());
    ASSERT_FALSE(ReadShouldContextUpdate());

    ++num_parsed_frames;
  }

  DVLOG(1) << "Number of successfully parsed frames before EOS: "
           << num_parsed_frames;

  EXPECT_EQ(num_expected_frames, num_parsed_frames);
}

TEST_F(Vp9ParserTest, StreamFileParsingWithContextUpdate) {
  Initialize("bear-vp9.ivf", true);

  // Number of frames in the test stream to be parsed.
  const int num_expected_frames = 82;
  int num_parsed_frames = 0;

  // Allow to parse twice as many frames in order to detect any extra frames
  // parsed.
  while (num_parsed_frames < num_expected_frames * 2) {
    Vp9FrameHeader fhdr;
    if (ParseNextFrame(&fhdr) != Vp9Parser::kOk)
      break;

    Vp9FrameContext frame_context;
    ReadContext(&frame_context);
    EXPECT_TRUE(memcmp(&frame_context, &fhdr.initial_frame_context,
                       sizeof(frame_context)) == 0);
    ReadContext(&frame_context);
    EXPECT_TRUE(memcmp(&frame_context, &fhdr.frame_context,
                       sizeof(frame_context)) == 0);

    bool should_update = ReadShouldContextUpdate();
    auto context_refresh_cb = GetContextRefreshCb(fhdr);
    if (context_refresh_cb.is_null()) {
      EXPECT_FALSE(should_update);
    } else {
      EXPECT_TRUE(should_update);
      ReadContext(&frame_context);
      context_refresh_cb.Run(frame_context);
    }

    ++num_parsed_frames;
  }

  DVLOG(1) << "Number of successfully parsed frames before EOS: "
           << num_parsed_frames;

  EXPECT_EQ(num_expected_frames, num_parsed_frames);
}

TEST_F(Vp9ParserTest, AwaitingContextUpdate) {
  Initialize("bear-vp9.ivf", true);

  Vp9FrameHeader fhdr;
  ASSERT_EQ(Vp9Parser::kOk, ParseNextFrame(&fhdr));

  Vp9FrameContext frame_context;
  ReadContext(&frame_context);
  ReadContext(&frame_context);
  bool should_update = ReadShouldContextUpdate();
  ASSERT_TRUE(should_update);
  ReadContext(&frame_context);

  // Not update yet. Should return kAwaitingRefresh.
  EXPECT_EQ(Vp9Parser::kAwaitingRefresh, ParseNextFrame(&fhdr));
  EXPECT_EQ(Vp9Parser::kAwaitingRefresh, ParseNextFrame(&fhdr));

  // After update, parse should be ok.
  auto context_refresh_cb = GetContextRefreshCb(fhdr);
  EXPECT_FALSE(context_refresh_cb.is_null());
  context_refresh_cb.Run(frame_context);
  EXPECT_EQ(Vp9Parser::kOk, ParseNextFrame(&fhdr));

  // Make sure it parsed the 2nd frame.
  EXPECT_EQ(9u, fhdr.header_size_in_bytes);
}

TEST_F(Vp9ParserTest, VerifyFirstFrame) {
  Initialize("test-25fps.vp9", false);
  Vp9FrameHeader fhdr;

  ASSERT_EQ(Vp9Parser::kOk, ParseNextFrame(&fhdr));

  EXPECT_EQ(0, fhdr.profile);
  EXPECT_FALSE(fhdr.show_existing_frame);
  EXPECT_EQ(Vp9FrameHeader::KEYFRAME, fhdr.frame_type);
  EXPECT_TRUE(fhdr.show_frame);
  EXPECT_FALSE(fhdr.error_resilient_mode);

  EXPECT_EQ(8, fhdr.bit_depth);
  EXPECT_EQ(Vp9ColorSpace::UNKNOWN, fhdr.color_space);
  EXPECT_FALSE(fhdr.color_range);
  EXPECT_EQ(1, fhdr.subsampling_x);
  EXPECT_EQ(1, fhdr.subsampling_y);

  EXPECT_EQ(320u, fhdr.frame_width);
  EXPECT_EQ(240u, fhdr.frame_height);
  EXPECT_EQ(320u, fhdr.render_width);
  EXPECT_EQ(240u, fhdr.render_height);

  EXPECT_TRUE(fhdr.refresh_frame_context);
  EXPECT_TRUE(fhdr.frame_parallel_decoding_mode);
  EXPECT_EQ(0, fhdr.frame_context_idx_to_save_probs);

  const Vp9LoopFilterParams& lf = GetLoopFilter();
  EXPECT_EQ(9, lf.level);
  EXPECT_EQ(0, lf.sharpness);
  EXPECT_TRUE(lf.delta_enabled);
  EXPECT_TRUE(lf.delta_update);
  EXPECT_TRUE(lf.update_ref_deltas[0]);
  EXPECT_EQ(1, lf.ref_deltas[0]);
  EXPECT_EQ(-1, lf.ref_deltas[2]);
  EXPECT_EQ(-1, lf.ref_deltas[3]);

  const Vp9QuantizationParams& qp = fhdr.quant_params;
  EXPECT_EQ(65, qp.base_q_idx);
  EXPECT_FALSE(qp.delta_q_y_dc);
  EXPECT_FALSE(qp.delta_q_uv_dc);
  EXPECT_FALSE(qp.delta_q_uv_ac);
  EXPECT_FALSE(qp.IsLossless());

  const Vp9SegmentationParams& seg = GetSegmentation();
  EXPECT_FALSE(seg.enabled);

  EXPECT_EQ(0, fhdr.tile_cols_log2);
  EXPECT_EQ(0, fhdr.tile_rows_log2);

  EXPECT_EQ(120u, fhdr.header_size_in_bytes);
  EXPECT_EQ(18u, fhdr.uncompressed_header_size);
}

TEST_F(Vp9ParserTest, VerifyInterFrame) {
  Initialize("test-25fps.vp9", false);
  Vp9FrameHeader fhdr;

  // To verify the second frame.
  for (int i = 0; i < 2; i++)
    ASSERT_EQ(Vp9Parser::kOk, ParseNextFrame(&fhdr));

  EXPECT_EQ(Vp9FrameHeader::INTERFRAME, fhdr.frame_type);
  EXPECT_FALSE(fhdr.show_frame);
  EXPECT_FALSE(fhdr.intra_only);
  EXPECT_FALSE(fhdr.reset_frame_context);
  EXPECT_TRUE(fhdr.RefreshFlag(2));
  EXPECT_EQ(0, fhdr.ref_frame_idx[0]);
  EXPECT_EQ(1, fhdr.ref_frame_idx[1]);
  EXPECT_EQ(2, fhdr.ref_frame_idx[2]);
  EXPECT_TRUE(fhdr.allow_high_precision_mv);
  EXPECT_EQ(Vp9InterpolationFilter::EIGHTTAP, fhdr.interpolation_filter);

  EXPECT_EQ(48u, fhdr.header_size_in_bytes);
  EXPECT_EQ(11u, fhdr.uncompressed_header_size);
}

}  // namespace media
