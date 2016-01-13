// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "media/base/decoder_buffer.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/limits.h"
#include "media/base/mock_filters.h"
#include "media/base/test_data_util.h"
#include "media/base/test_helpers.h"
#include "media/base/video_decoder.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_glue.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::InSequence;
using ::testing::IsNull;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

static const VideoFrame::Format kVideoFormat = VideoFrame::YV12;
static const gfx::Size kCodedSize(320, 240);
static const gfx::Rect kVisibleRect(320, 240);
static const gfx::Size kNaturalSize(320, 240);

ACTION_P(ReturnBuffer, buffer) {
  arg0.Run(buffer.get() ? DemuxerStream::kOk : DemuxerStream::kAborted, buffer);
}

class FFmpegVideoDecoderTest : public testing::Test {
 public:
  FFmpegVideoDecoderTest()
      : decoder_(new FFmpegVideoDecoder(message_loop_.message_loop_proxy())),
        decode_cb_(base::Bind(&FFmpegVideoDecoderTest::DecodeDone,
                              base::Unretained(this))) {
    FFmpegGlue::InitializeFFmpeg();

    // Initialize various test buffers.
    frame_buffer_.reset(new uint8[kCodedSize.GetArea()]);
    end_of_stream_buffer_ = DecoderBuffer::CreateEOSBuffer();
    i_frame_buffer_ = ReadTestDataFile("vp8-I-frame-320x240");
    corrupt_i_frame_buffer_ = ReadTestDataFile("vp8-corrupt-I-frame");
  }

  virtual ~FFmpegVideoDecoderTest() {
    Stop();
  }

  void Initialize() {
    InitializeWithConfig(TestVideoConfig::Normal());
  }

  void InitializeWithConfigAndStatus(const VideoDecoderConfig& config,
                                     PipelineStatus status) {
    decoder_->Initialize(config, false, NewExpectedStatusCB(status),
                         base::Bind(&FFmpegVideoDecoderTest::FrameReady,
                                    base::Unretained(this)));
    message_loop_.RunUntilIdle();
  }

  void InitializeWithConfig(const VideoDecoderConfig& config) {
    InitializeWithConfigAndStatus(config, PIPELINE_OK);
  }

  void Reinitialize() {
    InitializeWithConfig(TestVideoConfig::Large());
  }

  void Reset() {
    decoder_->Reset(NewExpectedClosure());
    message_loop_.RunUntilIdle();
  }

  void Stop() {
    decoder_->Stop();
    message_loop_.RunUntilIdle();
  }

  // Sets up expectations and actions to put FFmpegVideoDecoder in an active
  // decoding state.
  void EnterDecodingState() {
    EXPECT_EQ(VideoDecoder::kOk, DecodeSingleFrame(i_frame_buffer_));
    ASSERT_EQ(1U, output_frames_.size());
  }

  // Sets up expectations and actions to put FFmpegVideoDecoder in an end
  // of stream state.
  void EnterEndOfStreamState() {
    EXPECT_EQ(VideoDecoder::kOk, DecodeSingleFrame(end_of_stream_buffer_));
    ASSERT_FALSE(output_frames_.empty());
  }

  typedef std::vector<scoped_refptr<DecoderBuffer> > InputBuffers;
  typedef std::vector<scoped_refptr<VideoFrame> > OutputFrames;

  // Decodes all buffers in |input_buffers| and push all successfully decoded
  // output frames into |output_frames|.
  // Returns the last decode status returned by the decoder.
  VideoDecoder::Status DecodeMultipleFrames(const InputBuffers& input_buffers) {
    for (InputBuffers::const_iterator iter = input_buffers.begin();
         iter != input_buffers.end();
         ++iter) {
      VideoDecoder::Status status = Decode(*iter);
      switch (status) {
        case VideoDecoder::kOk:
          break;
        case VideoDecoder::kAborted:
          NOTREACHED();
        case VideoDecoder::kDecodeError:
        case VideoDecoder::kDecryptError:
          DCHECK(output_frames_.empty());
          return status;
      }
    }
    return VideoDecoder::kOk;
  }

  // Decodes the single compressed frame in |buffer| and writes the
  // uncompressed output to |video_frame|. This method works with single
  // and multithreaded decoders. End of stream buffers are used to trigger
  // the frame to be returned in the multithreaded decoder case.
  VideoDecoder::Status DecodeSingleFrame(
      const scoped_refptr<DecoderBuffer>& buffer) {
    InputBuffers input_buffers;
    input_buffers.push_back(buffer);
    input_buffers.push_back(end_of_stream_buffer_);

    return DecodeMultipleFrames(input_buffers);
  }

  // Decodes |i_frame_buffer_| and then decodes the data contained in
  // the file named |test_file_name|. This function expects both buffers
  // to decode to frames that are the same size.
  void DecodeIFrameThenTestFile(const std::string& test_file_name,
                                int expected_width,
                                int expected_height) {
    Initialize();
    scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile(test_file_name);

    InputBuffers input_buffers;
    input_buffers.push_back(i_frame_buffer_);
    input_buffers.push_back(buffer);
    input_buffers.push_back(end_of_stream_buffer_);

    VideoDecoder::Status status =
        DecodeMultipleFrames(input_buffers);

    EXPECT_EQ(VideoDecoder::kOk, status);
    ASSERT_EQ(2U, output_frames_.size());

    gfx::Size original_size = kVisibleRect.size();
    EXPECT_EQ(original_size.width(),
              output_frames_[0]->visible_rect().size().width());
    EXPECT_EQ(original_size.height(),
              output_frames_[0]->visible_rect().size().height());
    EXPECT_EQ(expected_width,
              output_frames_[1]->visible_rect().size().width());
    EXPECT_EQ(expected_height,
              output_frames_[1]->visible_rect().size().height());
  }

  VideoDecoder::Status Decode(const scoped_refptr<DecoderBuffer>& buffer) {
    VideoDecoder::Status status;
    EXPECT_CALL(*this, DecodeDone(_)).WillOnce(SaveArg<0>(&status));

    decoder_->Decode(buffer, decode_cb_);

    message_loop_.RunUntilIdle();

    return status;
  }

  void FrameReady(const scoped_refptr<VideoFrame>& frame) {
    DCHECK(!frame->end_of_stream());
    output_frames_.push_back(frame);
  }

  MOCK_METHOD1(DecodeDone, void(VideoDecoder::Status));

  base::MessageLoop message_loop_;
  scoped_ptr<FFmpegVideoDecoder> decoder_;

  VideoDecoder::DecodeCB decode_cb_;

  // Various buffers for testing.
  scoped_ptr<uint8_t[]> frame_buffer_;
  scoped_refptr<DecoderBuffer> end_of_stream_buffer_;
  scoped_refptr<DecoderBuffer> i_frame_buffer_;
  scoped_refptr<DecoderBuffer> corrupt_i_frame_buffer_;

  OutputFrames output_frames_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FFmpegVideoDecoderTest);
};

TEST_F(FFmpegVideoDecoderTest, Initialize_Normal) {
  Initialize();
}

TEST_F(FFmpegVideoDecoderTest, Initialize_UnsupportedDecoder) {
  // Test avcodec_find_decoder() returning NULL.
  InitializeWithConfigAndStatus(TestVideoConfig::Invalid(),
                                DECODER_ERROR_NOT_SUPPORTED);
}

TEST_F(FFmpegVideoDecoderTest, Initialize_UnsupportedPixelFormat) {
  // Ensure decoder handles unsupported pixel formats without crashing.
  VideoDecoderConfig config(kCodecVP8, VIDEO_CODEC_PROFILE_UNKNOWN,
                            VideoFrame::UNKNOWN,
                            kCodedSize, kVisibleRect, kNaturalSize,
                            NULL, 0, false);
  InitializeWithConfigAndStatus(config, DECODER_ERROR_NOT_SUPPORTED);
}

TEST_F(FFmpegVideoDecoderTest, Initialize_OpenDecoderFails) {
  // Specify Theora w/o extra data so that avcodec_open2() fails.
  VideoDecoderConfig config(kCodecTheora, VIDEO_CODEC_PROFILE_UNKNOWN,
                            kVideoFormat,
                            kCodedSize, kVisibleRect, kNaturalSize,
                            NULL, 0, false);
  InitializeWithConfigAndStatus(config, DECODER_ERROR_NOT_SUPPORTED);
}

TEST_F(FFmpegVideoDecoderTest, Initialize_AspectRatioNumeratorZero) {
  gfx::Size natural_size = GetNaturalSize(kVisibleRect.size(), 0, 1);
  VideoDecoderConfig config(kCodecVP8, VP8PROFILE_MAIN,
                            kVideoFormat,
                            kCodedSize, kVisibleRect, natural_size,
                            NULL, 0, false);
  InitializeWithConfigAndStatus(config, DECODER_ERROR_NOT_SUPPORTED);
}

TEST_F(FFmpegVideoDecoderTest, Initialize_AspectRatioDenominatorZero) {
  gfx::Size natural_size = GetNaturalSize(kVisibleRect.size(), 1, 0);
  VideoDecoderConfig config(kCodecVP8, VP8PROFILE_MAIN,
                            kVideoFormat,
                            kCodedSize, kVisibleRect, natural_size,
                            NULL, 0, false);
  InitializeWithConfigAndStatus(config, DECODER_ERROR_NOT_SUPPORTED);
}

TEST_F(FFmpegVideoDecoderTest, Initialize_AspectRatioNumeratorNegative) {
  gfx::Size natural_size = GetNaturalSize(kVisibleRect.size(), -1, 1);
  VideoDecoderConfig config(kCodecVP8, VP8PROFILE_MAIN,
                            kVideoFormat,
                            kCodedSize, kVisibleRect, natural_size,
                            NULL, 0, false);
  InitializeWithConfigAndStatus(config, DECODER_ERROR_NOT_SUPPORTED);
}

TEST_F(FFmpegVideoDecoderTest, Initialize_AspectRatioDenominatorNegative) {
  gfx::Size natural_size = GetNaturalSize(kVisibleRect.size(), 1, -1);
  VideoDecoderConfig config(kCodecVP8, VP8PROFILE_MAIN,
                            kVideoFormat,
                            kCodedSize, kVisibleRect, natural_size,
                            NULL, 0, false);
  InitializeWithConfigAndStatus(config, DECODER_ERROR_NOT_SUPPORTED);
}

TEST_F(FFmpegVideoDecoderTest, Initialize_AspectRatioNumeratorTooLarge) {
  int width = kVisibleRect.size().width();
  int num = ceil(static_cast<double>(limits::kMaxDimension + 1) / width);
  gfx::Size natural_size = GetNaturalSize(kVisibleRect.size(), num, 1);
  VideoDecoderConfig config(kCodecVP8, VP8PROFILE_MAIN,
                            kVideoFormat,
                            kCodedSize, kVisibleRect, natural_size,
                            NULL, 0, false);
  InitializeWithConfigAndStatus(config, DECODER_ERROR_NOT_SUPPORTED);
}

TEST_F(FFmpegVideoDecoderTest, Initialize_AspectRatioDenominatorTooLarge) {
  int den = kVisibleRect.size().width() + 1;
  gfx::Size natural_size = GetNaturalSize(kVisibleRect.size(), 1, den);
  VideoDecoderConfig config(kCodecVP8, VP8PROFILE_MAIN,
                            kVideoFormat,
                            kCodedSize, kVisibleRect, natural_size,
                            NULL, 0, false);
  InitializeWithConfigAndStatus(config, DECODER_ERROR_NOT_SUPPORTED);
}

TEST_F(FFmpegVideoDecoderTest, Reinitialize_Normal) {
  Initialize();
  Reinitialize();
}

TEST_F(FFmpegVideoDecoderTest, Reinitialize_Failure) {
  Initialize();
  InitializeWithConfigAndStatus(TestVideoConfig::Invalid(),
                                DECODER_ERROR_NOT_SUPPORTED);
}

TEST_F(FFmpegVideoDecoderTest, Reinitialize_AfterDecodeFrame) {
  Initialize();
  EnterDecodingState();
  Reinitialize();
}

TEST_F(FFmpegVideoDecoderTest, Reinitialize_AfterReset) {
  Initialize();
  EnterDecodingState();
  Reset();
  Reinitialize();
}

TEST_F(FFmpegVideoDecoderTest, DecodeFrame_Normal) {
  Initialize();

  // Simulate decoding a single frame.
  EXPECT_EQ(VideoDecoder::kOk, DecodeSingleFrame(i_frame_buffer_));
  ASSERT_EQ(1U, output_frames_.size());
}

// Verify current behavior for 0 byte frames. FFmpeg simply ignores
// the 0 byte frames.
TEST_F(FFmpegVideoDecoderTest, DecodeFrame_0ByteFrame) {
  Initialize();

  scoped_refptr<DecoderBuffer> zero_byte_buffer = new DecoderBuffer(0);

  InputBuffers input_buffers;
  input_buffers.push_back(i_frame_buffer_);
  input_buffers.push_back(zero_byte_buffer);
  input_buffers.push_back(i_frame_buffer_);
  input_buffers.push_back(end_of_stream_buffer_);

  VideoDecoder::Status status = DecodeMultipleFrames(input_buffers);

  EXPECT_EQ(VideoDecoder::kOk, status);
  ASSERT_EQ(2U, output_frames_.size());
}

TEST_F(FFmpegVideoDecoderTest, DecodeFrame_DecodeError) {
  Initialize();

  // The error is only raised on the second decode attempt, so we expect at
  // least one successful decode but we don't expect valid frame to be decoded.
  // During the second decode attempt an error is raised.
  EXPECT_EQ(VideoDecoder::kOk, Decode(corrupt_i_frame_buffer_));
  EXPECT_TRUE(output_frames_.empty());
  EXPECT_EQ(VideoDecoder::kDecodeError, Decode(i_frame_buffer_));
  EXPECT_TRUE(output_frames_.empty());

  // After a decode error occurred, all following decodes will return
  // kDecodeError.
  EXPECT_EQ(VideoDecoder::kDecodeError, Decode(i_frame_buffer_));
  EXPECT_TRUE(output_frames_.empty());
}

// Multi-threaded decoders have different behavior than single-threaded
// decoders at the end of the stream. Multithreaded decoders hide errors
// that happen on the last |codec_context_->thread_count| frames to avoid
// prematurely signalling EOS. This test just exposes that behavior so we can
// detect if it changes.
TEST_F(FFmpegVideoDecoderTest, DecodeFrame_DecodeErrorAtEndOfStream) {
  Initialize();

  EXPECT_EQ(VideoDecoder::kOk, DecodeSingleFrame(corrupt_i_frame_buffer_));
}

// Decode |i_frame_buffer_| and then a frame with a larger width and verify
// the output size was adjusted.
TEST_F(FFmpegVideoDecoderTest, DecodeFrame_LargerWidth) {
  DecodeIFrameThenTestFile("vp8-I-frame-640x240", 640, 240);
}

// Decode |i_frame_buffer_| and then a frame with a smaller width and verify
// the output size was adjusted.
TEST_F(FFmpegVideoDecoderTest, DecodeFrame_SmallerWidth) {
  DecodeIFrameThenTestFile("vp8-I-frame-160x240", 160, 240);
}

// Decode |i_frame_buffer_| and then a frame with a larger height and verify
// the output size was adjusted.
TEST_F(FFmpegVideoDecoderTest, DecodeFrame_LargerHeight) {
  DecodeIFrameThenTestFile("vp8-I-frame-320x480", 320, 480);
}

// Decode |i_frame_buffer_| and then a frame with a smaller height and verify
// the output size was adjusted.
TEST_F(FFmpegVideoDecoderTest, DecodeFrame_SmallerHeight) {
  DecodeIFrameThenTestFile("vp8-I-frame-320x120", 320, 120);
}

// Test resetting when decoder has initialized but not decoded.
TEST_F(FFmpegVideoDecoderTest, Reset_Initialized) {
  Initialize();
  Reset();
}

// Test resetting when decoder has decoded single frame.
TEST_F(FFmpegVideoDecoderTest, Reset_Decoding) {
  Initialize();
  EnterDecodingState();
  Reset();
}

// Test resetting when decoder has hit end of stream.
TEST_F(FFmpegVideoDecoderTest, Reset_EndOfStream) {
  Initialize();
  EnterDecodingState();
  EnterEndOfStreamState();
  Reset();
}

// Test stopping when decoder has initialized but not decoded.
TEST_F(FFmpegVideoDecoderTest, Stop_Initialized) {
  Initialize();
  Stop();
}

// Test stopping when decoder has decoded single frame.
TEST_F(FFmpegVideoDecoderTest, Stop_Decoding) {
  Initialize();
  EnterDecodingState();
  Stop();
}

// Test stopping when decoder has hit end of stream.
TEST_F(FFmpegVideoDecoderTest, Stop_EndOfStream) {
  Initialize();
  EnterDecodingState();
  EnterEndOfStreamState();
  Stop();
}

}  // namespace media
