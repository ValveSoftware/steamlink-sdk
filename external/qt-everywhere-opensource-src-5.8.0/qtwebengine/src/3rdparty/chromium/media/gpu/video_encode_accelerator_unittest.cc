// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <queue>
#include <string>
#include <utility>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/bits.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process_handle.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "media/base/test_data_util.h"
#include "media/base/video_decoder.h"
#include "media/base/video_frame.h"
#include "media/filters/ffmpeg_glue.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/h264_parser.h"
#include "media/filters/ivf_parser.h"
#include "media/gpu/video_accelerator_unittest_helpers.h"
#include "media/video/fake_video_encode_accelerator.h"
#include "media/video/video_encode_accelerator.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#if defined(ARCH_CPU_ARMEL) || (defined(USE_OZONE) && defined(USE_V4L2_CODEC))
#include "media/gpu/v4l2_video_encode_accelerator.h"
#endif
#if defined(ARCH_CPU_X86_FAMILY)
#include "media/gpu/vaapi_video_encode_accelerator.h"
#include "media/gpu/vaapi_wrapper.h"
// Status has been defined as int in Xlib.h.
#undef Status
#endif  // defined(ARCH_CPU_X86_FAMILY)
#elif defined(OS_MACOSX)
#include "media/gpu/vt_video_encode_accelerator_mac.h"
#else
#error The VideoEncodeAcceleratorUnittest is not supported on this platform.
#endif

namespace media {
namespace {

const VideoPixelFormat kInputFormat = PIXEL_FORMAT_I420;

// The absolute differences between original frame and decoded frame usually
// ranges aroud 1 ~ 7. So we pick 10 as an extreme value to detect abnormal
// decoded frames.
const double kDecodeSimilarityThreshold = 10.0;

// Arbitrarily chosen to add some depth to the pipeline.
const unsigned int kNumOutputBuffers = 4;
const unsigned int kNumExtraInputFrames = 4;
// Maximum delay between requesting a keyframe and receiving one, in frames.
// Arbitrarily chosen as a reasonable requirement.
const unsigned int kMaxKeyframeDelay = 4;
// Default initial bitrate.
const uint32_t kDefaultBitrate = 2000000;
// Default ratio of requested_subsequent_bitrate to initial_bitrate
// (see test parameters below) if one is not provided.
const double kDefaultSubsequentBitrateRatio = 2.0;
// Default initial framerate.
const uint32_t kDefaultFramerate = 30;
// Default ratio of requested_subsequent_framerate to initial_framerate
// (see test parameters below) if one is not provided.
const double kDefaultSubsequentFramerateRatio = 0.1;
// Tolerance factor for how encoded bitrate can differ from requested bitrate.
const double kBitrateTolerance = 0.1;
// Minimum required FPS throughput for the basic performance test.
const uint32_t kMinPerfFPS = 30;
// Minimum (arbitrary) number of frames required to enforce bitrate requirements
// over. Streams shorter than this may be too short to realistically require
// an encoder to be able to converge to the requested bitrate over.
// The input stream will be looped as many times as needed in bitrate tests
// to reach at least this number of frames before calculating final bitrate.
const unsigned int kMinFramesForBitrateTests = 300;
// The percentiles to measure for encode latency.
const unsigned int kLoggedLatencyPercentiles[] = {50, 75, 95};

// The syntax of multiple test streams is:
//  test-stream1;test-stream2;test-stream3
// The syntax of each test stream is:
// "in_filename:width:height:profile:out_filename:requested_bitrate
//  :requested_framerate:requested_subsequent_bitrate
//  :requested_subsequent_framerate"
// - |in_filename| must be an I420 (YUV planar) raw stream
//   (see http://www.fourcc.org/yuv.php#IYUV).
// - |width| and |height| are in pixels.
// - |profile| to encode into (values of VideoCodecProfile).
// - |out_filename| filename to save the encoded stream to (optional). The
//   format for H264 is Annex-B byte stream. The format for VP8 is IVF. Output
//   stream is saved for the simple encode test only. H264 raw stream and IVF
//   can be used as input of VDA unittest. H264 raw stream can be played by
//   "mplayer -fps 25 out.h264" and IVF can be played by mplayer directly.
//   Helpful description: http://wiki.multimedia.cx/index.php?title=IVF
// Further parameters are optional (need to provide preceding positional
// parameters if a specific subsequent parameter is required):
// - |requested_bitrate| requested bitrate in bits per second.
// - |requested_framerate| requested initial framerate.
// - |requested_subsequent_bitrate| bitrate to switch to in the middle of the
//                                  stream.
// - |requested_subsequent_framerate| framerate to switch to in the middle
//                                    of the stream.
//   Bitrate is only forced for tests that test bitrate.
const char* g_default_in_filename = "bear_320x192_40frames.yuv";
#if !defined(OS_MACOSX)
const char* g_default_in_parameters = ":320:192:1:out.h264:200000";
#else
const char* g_default_in_parameters = ":320:192:0:out.h264:200000";
#endif

// Enabled by including a --fake_encoder flag to the command line invoking the
// test.
bool g_fake_encoder = false;

// Environment to store test stream data for all test cases.
class VideoEncodeAcceleratorTestEnvironment;
VideoEncodeAcceleratorTestEnvironment* g_env;

// The number of frames to be encoded. This variable is set by the switch
// "--num_frames_to_encode". Ignored if 0.
int g_num_frames_to_encode = 0;

// An aligned STL allocator.
template <typename T, size_t ByteAlignment>
class AlignedAllocator : public std::allocator<T> {
 public:
  typedef size_t size_type;
  typedef T* pointer;

  template <class T1>
  struct rebind {
    typedef AlignedAllocator<T1, ByteAlignment> other;
  };

  AlignedAllocator() {}
  explicit AlignedAllocator(const AlignedAllocator&) {}
  template <class T1>
  explicit AlignedAllocator(const AlignedAllocator<T1, ByteAlignment>&) {}
  ~AlignedAllocator() {}

  pointer allocate(size_type n, const void* = 0) {
    return static_cast<pointer>(base::AlignedAlloc(n, ByteAlignment));
  }

  void deallocate(pointer p, size_type n) {
    base::AlignedFree(static_cast<void*>(p));
  }

  size_type max_size() const {
    return std::numeric_limits<size_t>::max() / sizeof(T);
  }
};

struct TestStream {
  TestStream()
      : num_frames(0),
        aligned_buffer_size(0),
        requested_bitrate(0),
        requested_framerate(0),
        requested_subsequent_bitrate(0),
        requested_subsequent_framerate(0) {}
  ~TestStream() {}

  gfx::Size visible_size;
  gfx::Size coded_size;
  unsigned int num_frames;

  // Original unaligned input file name provided as an argument to the test.
  // And the file must be an I420 (YUV planar) raw stream.
  std::string in_filename;

  // A vector used to prepare aligned input buffers of |in_filename|. This
  // makes sure starting address of YUV planes are 64 bytes-aligned.
  std::vector<char, AlignedAllocator<char, 64>> aligned_in_file_data;

  // Byte size of a frame of |aligned_in_file_data|.
  size_t aligned_buffer_size;

  // Byte size for each aligned plane of a frame.
  std::vector<size_t> aligned_plane_size;

  std::string out_filename;
  VideoCodecProfile requested_profile;
  unsigned int requested_bitrate;
  unsigned int requested_framerate;
  unsigned int requested_subsequent_bitrate;
  unsigned int requested_subsequent_framerate;
};

inline static size_t Align64Bytes(size_t value) {
  return base::bits::Align(value, 64);
}

// Return the |percentile| from a sorted vector.
static base::TimeDelta Percentile(
    const std::vector<base::TimeDelta>& sorted_values,
    unsigned int percentile) {
  size_t size = sorted_values.size();
  LOG_ASSERT(size > 0UL);
  LOG_ASSERT(percentile <= 100UL);
  // Use Nearest Rank method in http://en.wikipedia.org/wiki/Percentile.
  int index =
      std::max(static_cast<int>(ceil(0.01f * percentile * size)) - 1, 0);
  return sorted_values[index];
}

static bool IsH264(VideoCodecProfile profile) {
  return profile >= H264PROFILE_MIN && profile <= H264PROFILE_MAX;
}

static bool IsVP8(VideoCodecProfile profile) {
  return profile >= VP8PROFILE_MIN && profile <= VP8PROFILE_MAX;
}

// ARM performs CPU cache management with CPU cache line granularity. We thus
// need to ensure our buffers are CPU cache line-aligned (64 byte-aligned).
// Otherwise newer kernels will refuse to accept them, and on older kernels
// we'll be treating ourselves to random corruption.
// Since we are just mapping and passing chunks of the input file directly to
// the VEA as input frames to avoid copying large chunks of raw data on each
// frame and thus affecting performance measurements, we have to prepare a
// temporary file with all planes aligned to 64-byte boundaries beforehand.
static void CreateAlignedInputStreamFile(const gfx::Size& coded_size,
                                         TestStream* test_stream) {
  // Test case may have many encoders and memory should be prepared once.
  if (test_stream->coded_size == coded_size &&
      !test_stream->aligned_in_file_data.empty())
    return;

  // All encoders in multiple encoder test reuse the same test_stream, make
  // sure they requested the same coded_size
  ASSERT_TRUE(test_stream->aligned_in_file_data.empty() ||
              coded_size == test_stream->coded_size);
  test_stream->coded_size = coded_size;

  size_t num_planes = VideoFrame::NumPlanes(kInputFormat);
  std::vector<size_t> padding_sizes(num_planes);
  std::vector<size_t> coded_bpl(num_planes);
  std::vector<size_t> visible_bpl(num_planes);
  std::vector<size_t> visible_plane_rows(num_planes);

  // Calculate padding in bytes to be added after each plane required to keep
  // starting addresses of all planes at a 64 byte boudnary. This padding will
  // be added after each plane when copying to the temporary file.
  // At the same time we also need to take into account coded_size requested by
  // the VEA; each row of visible_bpl bytes in the original file needs to be
  // copied into a row of coded_bpl bytes in the aligned file.
  for (size_t i = 0; i < num_planes; i++) {
    const size_t size =
        VideoFrame::PlaneSize(kInputFormat, i, coded_size).GetArea();
    test_stream->aligned_plane_size.push_back(Align64Bytes(size));
    test_stream->aligned_buffer_size += test_stream->aligned_plane_size.back();

    coded_bpl[i] = VideoFrame::RowBytes(i, kInputFormat, coded_size.width());
    visible_bpl[i] = VideoFrame::RowBytes(i, kInputFormat,
                                          test_stream->visible_size.width());
    visible_plane_rows[i] =
        VideoFrame::Rows(i, kInputFormat, test_stream->visible_size.height());
    const size_t padding_rows =
        VideoFrame::Rows(i, kInputFormat, coded_size.height()) -
        visible_plane_rows[i];
    padding_sizes[i] = padding_rows * coded_bpl[i] + Align64Bytes(size) - size;
  }

  base::FilePath src_file(test_stream->in_filename);
  int64_t src_file_size = 0;
  LOG_ASSERT(base::GetFileSize(src_file, &src_file_size));

  size_t visible_buffer_size =
      VideoFrame::AllocationSize(kInputFormat, test_stream->visible_size);
  LOG_ASSERT(src_file_size % visible_buffer_size == 0U)
      << "Stream byte size is not a product of calculated frame byte size";

  test_stream->num_frames = src_file_size / visible_buffer_size;

  LOG_ASSERT(test_stream->aligned_buffer_size > 0UL);
  test_stream->aligned_in_file_data.resize(test_stream->aligned_buffer_size *
                                           test_stream->num_frames);

  base::File src(src_file, base::File::FLAG_OPEN | base::File::FLAG_READ);
  std::vector<char> src_data(visible_buffer_size);
  off_t src_offset = 0, dest_offset = 0;
  for (size_t frame = 0; frame < test_stream->num_frames; frame++) {
    LOG_ASSERT(src.Read(src_offset, &src_data[0], visible_buffer_size) ==
               static_cast<int>(visible_buffer_size));
    const char* src_ptr = &src_data[0];
    for (size_t i = 0; i < num_planes; i++) {
      // Assert that each plane of frame starts at 64 byte boundary.
      ASSERT_EQ(dest_offset & 63, 0)
          << "Planes of frame should be mapped at a 64 byte boundary";
      for (size_t j = 0; j < visible_plane_rows[i]; j++) {
        memcpy(&test_stream->aligned_in_file_data[dest_offset], src_ptr,
               visible_bpl[i]);
        src_ptr += visible_bpl[i];
        dest_offset += coded_bpl[i];
      }
      dest_offset += padding_sizes[i];
    }
    src_offset += visible_buffer_size;
  }
  src.Close();

  // Assert that memory mapped of file starts at 64 byte boundary. So each
  // plane of frames also start at 64 byte boundary.
  ASSERT_EQ(reinterpret_cast<off_t>(&test_stream->aligned_in_file_data[0]) & 63,
            0)
      << "File should be mapped at a 64 byte boundary";

  LOG_ASSERT(test_stream->num_frames > 0UL);
}

// Parse |data| into its constituent parts, set the various output fields
// accordingly, read in video stream, and store them to |test_streams|.
static void ParseAndReadTestStreamData(const base::FilePath::StringType& data,
                                       ScopedVector<TestStream>* test_streams) {
  // Split the string to individual test stream data.
  std::vector<base::FilePath::StringType> test_streams_data =
      base::SplitString(data, base::FilePath::StringType(1, ';'),
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  LOG_ASSERT(test_streams_data.size() >= 1U) << data;

  // Parse each test stream data and read the input file.
  for (size_t index = 0; index < test_streams_data.size(); ++index) {
    std::vector<base::FilePath::StringType> fields = base::SplitString(
        test_streams_data[index], base::FilePath::StringType(1, ':'),
        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    LOG_ASSERT(fields.size() >= 4U) << data;
    LOG_ASSERT(fields.size() <= 9U) << data;
    TestStream* test_stream = new TestStream();

    test_stream->in_filename = fields[0];
    int width, height;
    bool result = base::StringToInt(fields[1], &width);
    LOG_ASSERT(result);
    result = base::StringToInt(fields[2], &height);
    LOG_ASSERT(result);
    test_stream->visible_size = gfx::Size(width, height);
    LOG_ASSERT(!test_stream->visible_size.IsEmpty());
    int profile;
    result = base::StringToInt(fields[3], &profile);
    LOG_ASSERT(result);
    LOG_ASSERT(profile > VIDEO_CODEC_PROFILE_UNKNOWN);
    LOG_ASSERT(profile <= VIDEO_CODEC_PROFILE_MAX);
    test_stream->requested_profile = static_cast<VideoCodecProfile>(profile);

    if (fields.size() >= 5 && !fields[4].empty())
      test_stream->out_filename = fields[4];

    if (fields.size() >= 6 && !fields[5].empty())
      LOG_ASSERT(
          base::StringToUint(fields[5], &test_stream->requested_bitrate));

    if (fields.size() >= 7 && !fields[6].empty())
      LOG_ASSERT(
          base::StringToUint(fields[6], &test_stream->requested_framerate));

    if (fields.size() >= 8 && !fields[7].empty()) {
      LOG_ASSERT(base::StringToUint(
          fields[7], &test_stream->requested_subsequent_bitrate));
    }

    if (fields.size() >= 9 && !fields[8].empty()) {
      LOG_ASSERT(base::StringToUint(
          fields[8], &test_stream->requested_subsequent_framerate));
    }
    test_streams->push_back(test_stream);
  }
}

// Basic test environment shared across multiple test cases. We only need to
// setup it once for all test cases.
// It helps
// - maintain test stream data and other test settings.
// - clean up temporary aligned files.
// - output log to file.
class VideoEncodeAcceleratorTestEnvironment : public ::testing::Environment {
 public:
  VideoEncodeAcceleratorTestEnvironment(
      std::unique_ptr<base::FilePath::StringType> data,
      const base::FilePath& log_path,
      bool run_at_fps,
      bool needs_encode_latency,
      bool verify_all_output)
      : test_stream_data_(std::move(data)),
        log_path_(log_path),
        run_at_fps_(run_at_fps),
        needs_encode_latency_(needs_encode_latency),
        verify_all_output_(verify_all_output) {}

  virtual void SetUp() {
    if (!log_path_.empty()) {
      log_file_.reset(new base::File(
          log_path_, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE));
      LOG_ASSERT(log_file_->IsValid());
    }
    ParseAndReadTestStreamData(*test_stream_data_, &test_streams_);
  }

  virtual void TearDown() {
    log_file_.reset();
  }

  // Log one entry of machine-readable data to file and LOG(INFO).
  // The log has one data entry per line in the format of "<key>: <value>".
  // Note that Chrome OS video_VEAPerf autotest parses the output key and value
  // pairs. Be sure to keep the autotest in sync.
  void LogToFile(const std::string& key, const std::string& value) {
    std::string s = base::StringPrintf("%s: %s\n", key.c_str(), value.c_str());
    LOG(INFO) << s;
    if (log_file_) {
      log_file_->WriteAtCurrentPos(s.data(), s.length());
    }
  }

  // Feed the encoder with the input buffers at the requested framerate. If
  // false, feed as fast as possible. This is set by the command line switch
  // "--run_at_fps".
  bool run_at_fps() const { return run_at_fps_; }

  // Whether to measure encode latency. This is set by the command line switch
  // "--measure_latency".
  bool needs_encode_latency() const { return needs_encode_latency_; }

  // Verify the encoder output of all testcases. This is set by the command line
  // switch "--verify_all_output".
  bool verify_all_output() const { return verify_all_output_; }

  ScopedVector<TestStream> test_streams_;

 private:
  std::unique_ptr<base::FilePath::StringType> test_stream_data_;
  base::FilePath log_path_;
  std::unique_ptr<base::File> log_file_;
  bool run_at_fps_;
  bool needs_encode_latency_;
  bool verify_all_output_;
};

enum ClientState {
  CS_CREATED,
  CS_ENCODER_SET,
  CS_INITIALIZED,
  CS_ENCODING,
  // Encoding has finished.
  CS_FINISHED,
  // Encoded frame quality has been validated.
  CS_VALIDATED,
  CS_ERROR,
};

// Performs basic, codec-specific sanity checks on the stream buffers passed
// to ProcessStreamBuffer(): whether we've seen keyframes before non-keyframes,
// correct sequences of H.264 NALUs (SPS before PPS and before slices), etc.
// Calls given FrameFoundCallback when a complete frame is found while
// processing.
class StreamValidator {
 public:
  // To be called when a complete frame is found while processing a stream
  // buffer, passing true if the frame is a keyframe. Returns false if we
  // are not interested in more frames and further processing should be aborted.
  typedef base::Callback<bool(bool)> FrameFoundCallback;

  virtual ~StreamValidator() {}

  // Provide a StreamValidator instance for the given |profile|.
  static std::unique_ptr<StreamValidator> Create(
      VideoCodecProfile profile,
      const FrameFoundCallback& frame_cb);

  // Process and verify contents of a bitstream buffer.
  virtual void ProcessStreamBuffer(const uint8_t* stream, size_t size) = 0;

 protected:
  explicit StreamValidator(const FrameFoundCallback& frame_cb)
      : frame_cb_(frame_cb) {}

  FrameFoundCallback frame_cb_;
};

class H264Validator : public StreamValidator {
 public:
  explicit H264Validator(const FrameFoundCallback& frame_cb)
      : StreamValidator(frame_cb),
        seen_sps_(false),
        seen_pps_(false),
        seen_idr_(false) {}

  void ProcessStreamBuffer(const uint8_t* stream, size_t size) override;

 private:
  // Set to true when encoder provides us with the corresponding NALU type.
  bool seen_sps_;
  bool seen_pps_;
  bool seen_idr_;

  H264Parser h264_parser_;
};

void H264Validator::ProcessStreamBuffer(const uint8_t* stream, size_t size) {
  h264_parser_.SetStream(stream, size);

  while (1) {
    H264NALU nalu;
    H264Parser::Result result;

    result = h264_parser_.AdvanceToNextNALU(&nalu);
    if (result == H264Parser::kEOStream)
      break;

    ASSERT_EQ(H264Parser::kOk, result);

    bool keyframe = false;

    switch (nalu.nal_unit_type) {
      case H264NALU::kIDRSlice:
        ASSERT_TRUE(seen_sps_);
        ASSERT_TRUE(seen_pps_);
        seen_idr_ = true;
        keyframe = true;
      // fallthrough
      case H264NALU::kNonIDRSlice: {
        ASSERT_TRUE(seen_idr_);
        if (!frame_cb_.Run(keyframe))
          return;
        break;
      }

      case H264NALU::kSPS: {
        int sps_id;
        ASSERT_EQ(H264Parser::kOk, h264_parser_.ParseSPS(&sps_id));
        seen_sps_ = true;
        break;
      }

      case H264NALU::kPPS: {
        ASSERT_TRUE(seen_sps_);
        int pps_id;
        ASSERT_EQ(H264Parser::kOk, h264_parser_.ParsePPS(&pps_id));
        seen_pps_ = true;
        break;
      }

      default:
        break;
    }
  }
}

class VP8Validator : public StreamValidator {
 public:
  explicit VP8Validator(const FrameFoundCallback& frame_cb)
      : StreamValidator(frame_cb), seen_keyframe_(false) {}

  void ProcessStreamBuffer(const uint8_t* stream, size_t size) override;

 private:
  // Have we already got a keyframe in the stream?
  bool seen_keyframe_;
};

void VP8Validator::ProcessStreamBuffer(const uint8_t* stream, size_t size) {
  bool keyframe = !(stream[0] & 0x01);
  if (keyframe)
    seen_keyframe_ = true;

  EXPECT_TRUE(seen_keyframe_);

  frame_cb_.Run(keyframe);
  // TODO(posciak): We could be getting more frames in the buffer, but there is
  // no simple way to detect this. We'd need to parse the frames and go through
  // partition numbers/sizes. For now assume one frame per buffer.
}

// static
std::unique_ptr<StreamValidator> StreamValidator::Create(
    VideoCodecProfile profile,
    const FrameFoundCallback& frame_cb) {
  std::unique_ptr<StreamValidator> validator;

  if (IsH264(profile)) {
    validator.reset(new H264Validator(frame_cb));
  } else if (IsVP8(profile)) {
    validator.reset(new VP8Validator(frame_cb));
  } else {
    LOG(FATAL) << "Unsupported profile: " << profile;
  }

  return validator;
}

class VideoFrameQualityValidator {
 public:
  VideoFrameQualityValidator(const VideoCodecProfile profile,
                             const base::Closure& flush_complete_cb,
                             const base::Closure& decode_error_cb);
  void Initialize(const gfx::Size& coded_size, const gfx::Rect& visible_size);
  // Save original YUV frame to compare it with the decoded frame later.
  void AddOriginalFrame(scoped_refptr<VideoFrame> frame);
  void AddDecodeBuffer(const scoped_refptr<DecoderBuffer>& buffer);
  // Flush the decoder.
  void Flush();

 private:
  void InitializeCB(bool success);
  void DecodeDone(DecodeStatus status);
  void FlushDone(DecodeStatus status);
  void VerifyOutputFrame(const scoped_refptr<VideoFrame>& output_frame);
  void Decode();

  enum State { UNINITIALIZED, INITIALIZED, DECODING, ERROR };

  const VideoCodecProfile profile_;
  std::unique_ptr<FFmpegVideoDecoder> decoder_;
  VideoDecoder::DecodeCB decode_cb_;
  // Decode callback of an EOS buffer.
  VideoDecoder::DecodeCB eos_decode_cb_;
  // Callback of Flush(). Called after all frames are decoded.
  const base::Closure flush_complete_cb_;
  const base::Closure decode_error_cb_;
  State decoder_state_;
  std::queue<scoped_refptr<VideoFrame>> original_frames_;
  std::queue<scoped_refptr<DecoderBuffer>> decode_buffers_;
};

VideoFrameQualityValidator::VideoFrameQualityValidator(
    const VideoCodecProfile profile,
    const base::Closure& flush_complete_cb,
    const base::Closure& decode_error_cb)
    : profile_(profile),
      decoder_(new FFmpegVideoDecoder()),
      decode_cb_(base::Bind(&VideoFrameQualityValidator::DecodeDone,
                            base::Unretained(this))),
      eos_decode_cb_(base::Bind(&VideoFrameQualityValidator::FlushDone,
                                base::Unretained(this))),
      flush_complete_cb_(flush_complete_cb),
      decode_error_cb_(decode_error_cb),
      decoder_state_(UNINITIALIZED) {
  // Allow decoding of individual NALU. Entire frames are required by default.
  decoder_->set_decode_nalus(true);
}

void VideoFrameQualityValidator::Initialize(const gfx::Size& coded_size,
                                            const gfx::Rect& visible_size) {
  FFmpegGlue::InitializeFFmpeg();

  gfx::Size natural_size(visible_size.size());
  // The default output format of ffmpeg video decoder is YV12.
  VideoDecoderConfig config;
  if (IsVP8(profile_))
    config.Initialize(kCodecVP8, VP8PROFILE_ANY, kInputFormat,
                      COLOR_SPACE_UNSPECIFIED, coded_size, visible_size,
                      natural_size, EmptyExtraData(), Unencrypted());
  else if (IsH264(profile_))
    config.Initialize(kCodecH264, H264PROFILE_MAIN, kInputFormat,
                      COLOR_SPACE_UNSPECIFIED, coded_size, visible_size,
                      natural_size, EmptyExtraData(), Unencrypted());
  else
    LOG_ASSERT(0) << "Invalid profile " << profile_;

  decoder_->Initialize(
      config, false, nullptr,
      base::Bind(&VideoFrameQualityValidator::InitializeCB,
                 base::Unretained(this)),
      base::Bind(&VideoFrameQualityValidator::VerifyOutputFrame,
                 base::Unretained(this)));
}

void VideoFrameQualityValidator::InitializeCB(bool success) {
  if (success) {
    decoder_state_ = INITIALIZED;
    Decode();
  } else {
    decoder_state_ = ERROR;
    if (IsH264(profile_))
      LOG(ERROR) << "Chromium does not support H264 decode. Try Chrome.";
    FAIL() << "Decoder initialization error";
    decode_error_cb_.Run();
  }
}

void VideoFrameQualityValidator::AddOriginalFrame(
    scoped_refptr<VideoFrame> frame) {
  original_frames_.push(frame);
}

void VideoFrameQualityValidator::DecodeDone(DecodeStatus status) {
  if (status == DecodeStatus::OK) {
    decoder_state_ = INITIALIZED;
    Decode();
  } else {
    decoder_state_ = ERROR;
    FAIL() << "Unexpected decode status = " << status << ". Stop decoding.";
    decode_error_cb_.Run();
  }
}

void VideoFrameQualityValidator::FlushDone(DecodeStatus status) {
  flush_complete_cb_.Run();
}

void VideoFrameQualityValidator::Flush() {
  if (decoder_state_ != ERROR) {
    decode_buffers_.push(DecoderBuffer::CreateEOSBuffer());
    Decode();
  }
}

void VideoFrameQualityValidator::AddDecodeBuffer(
    const scoped_refptr<DecoderBuffer>& buffer) {
  if (decoder_state_ != ERROR) {
    decode_buffers_.push(buffer);
    Decode();
  }
}

void VideoFrameQualityValidator::Decode() {
  if (decoder_state_ == INITIALIZED && !decode_buffers_.empty()) {
    scoped_refptr<DecoderBuffer> next_buffer = decode_buffers_.front();
    decode_buffers_.pop();
    decoder_state_ = DECODING;
    if (next_buffer->end_of_stream())
      decoder_->Decode(next_buffer, eos_decode_cb_);
    else
      decoder_->Decode(next_buffer, decode_cb_);
  }
}

void VideoFrameQualityValidator::VerifyOutputFrame(
    const scoped_refptr<VideoFrame>& output_frame) {
  scoped_refptr<VideoFrame> original_frame = original_frames_.front();
  original_frames_.pop();
  gfx::Size visible_size = original_frame->visible_rect().size();

  int planes[] = {VideoFrame::kYPlane, VideoFrame::kUPlane,
                  VideoFrame::kVPlane};
  double difference = 0;
  for (int plane : planes) {
    uint8_t* original_plane = original_frame->data(plane);
    uint8_t* output_plane = output_frame->data(plane);

    size_t rows = VideoFrame::Rows(plane, kInputFormat, visible_size.height());
    size_t columns =
        VideoFrame::Columns(plane, kInputFormat, visible_size.width());
    size_t stride = original_frame->stride(plane);

    for (size_t i = 0; i < rows; i++)
      for (size_t j = 0; j < columns; j++)
        difference += std::abs(original_plane[stride * i + j] -
                               output_plane[stride * i + j]);
  }
  // Divide the difference by the size of frame.
  difference /= VideoFrame::AllocationSize(kInputFormat, visible_size);
  EXPECT_TRUE(difference <= kDecodeSimilarityThreshold)
      << "differrence = " << difference << "  > decode similarity threshold";
}

class VEAClient : public VideoEncodeAccelerator::Client {
 public:
  VEAClient(TestStream* test_stream,
            ClientStateNotification<ClientState>* note,
            bool save_to_file,
            unsigned int keyframe_period,
            bool force_bitrate,
            bool test_perf,
            bool mid_stream_bitrate_switch,
            bool mid_stream_framerate_switch,
            bool verify_output);
  ~VEAClient() override;
  void CreateEncoder();
  void DestroyEncoder();

  // VideoDecodeAccelerator::Client implementation.
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override;
  void BitstreamBufferReady(int32_t bitstream_buffer_id,
                            size_t payload_size,
                            bool key_frame,
                            base::TimeDelta timestamp) override;
  void NotifyError(VideoEncodeAccelerator::Error error) override;

 private:
  bool has_encoder() { return encoder_.get(); }

  // Return the number of encoded frames per second.
  double frames_per_second();

  std::unique_ptr<VideoEncodeAccelerator> CreateFakeVEA();
  std::unique_ptr<VideoEncodeAccelerator> CreateV4L2VEA();
  std::unique_ptr<VideoEncodeAccelerator> CreateVaapiVEA();
  std::unique_ptr<VideoEncodeAccelerator> CreateVTVEA();

  void SetState(ClientState new_state);

  // Set current stream parameters to given |bitrate| at |framerate|.
  void SetStreamParameters(unsigned int bitrate, unsigned int framerate);

  // Called when encoder is done with a VideoFrame.
  void InputNoLongerNeededCallback(int32_t input_id);

  // Feed the encoder with one input frame.
  void FeedEncoderWithOneInput();

  // Provide the encoder with a new output buffer.
  void FeedEncoderWithOutput(base::SharedMemory* shm);

  // Called on finding a complete frame (with |keyframe| set to true for
  // keyframes) in the stream, to perform codec-independent, per-frame checks
  // and accounting. Returns false once we have collected all frames we needed.
  bool HandleEncodedFrame(bool keyframe);

  // Verify the minimum FPS requirement.
  void VerifyMinFPS();

  // Verify that stream bitrate has been close to current_requested_bitrate_,
  // assuming current_framerate_ since the last time VerifyStreamProperties()
  // was called. Fail the test if |force_bitrate_| is true and the bitrate
  // is not within kBitrateTolerance.
  void VerifyStreamProperties();

  // Log the performance data.
  void LogPerf();

  // Write IVF file header to test_stream_->out_filename.
  void WriteIvfFileHeader();

  // Write an IVF frame header to test_stream_->out_filename.
  void WriteIvfFrameHeader(int frame_index, size_t frame_size);

  // Create and return a VideoFrame wrapping the data at |position| bytes in the
  // input stream.
  scoped_refptr<VideoFrame> CreateFrame(off_t position);

  // Prepare and return a frame wrapping the data at |position| bytes in the
  // input stream, ready to be sent to encoder.
  // The input frame id is returned in |input_id|.
  scoped_refptr<VideoFrame> PrepareInputFrame(off_t position,
                                              int32_t* input_id);

  // Update the parameters according to |mid_stream_bitrate_switch| and
  // |mid_stream_framerate_switch|.
  void UpdateTestStreamData(bool mid_stream_bitrate_switch,
                            bool mid_stream_framerate_switch);

  // Callback function of the |input_timer_|.
  void OnInputTimer();

  // Called when the quality validator has decoded all the frames.
  void DecodeCompleted();

  // Called when the quality validator fails to decode a frame.
  void DecodeFailed();

  ClientState state_;
  std::unique_ptr<VideoEncodeAccelerator> encoder_;

  TestStream* test_stream_;

  // Used to notify another thread about the state. VEAClient does not own this.
  ClientStateNotification<ClientState>* note_;

  // Ids assigned to VideoFrames.
  std::set<int32_t> inputs_at_client_;
  int32_t next_input_id_;

  // Encode start time of all encoded frames. The position in the vector is the
  // frame input id.
  std::vector<base::TimeTicks> encode_start_time_;
  // The encode latencies of all encoded frames. We define encode latency as the
  // time delay from input of each VideoFrame (VEA::Encode()) to output of the
  // corresponding BitstreamBuffer (VEA::Client::BitstreamBufferReady()).
  std::vector<base::TimeDelta> encode_latencies_;

  // Ids for output BitstreamBuffers.
  typedef std::map<int32_t, base::SharedMemory*> IdToSHM;
  ScopedVector<base::SharedMemory> output_shms_;
  IdToSHM output_buffers_at_client_;
  int32_t next_output_buffer_id_;

  // Current offset into input stream.
  off_t pos_in_input_stream_;
  gfx::Size input_coded_size_;
  // Requested by encoder.
  unsigned int num_required_input_buffers_;
  size_t output_buffer_size_;

  // Number of frames to encode. This may differ from the number of frames in
  // stream if we need more frames for bitrate tests.
  unsigned int num_frames_to_encode_;

  // Number of encoded frames we've got from the encoder thus far.
  unsigned int num_encoded_frames_;

  // Frames since last bitrate verification.
  unsigned int num_frames_since_last_check_;

  // True if received a keyframe while processing current bitstream buffer.
  bool seen_keyframe_in_this_buffer_;

  // True if we are to save the encoded stream to a file.
  bool save_to_file_;

  // Request a keyframe every keyframe_period_ frames.
  const unsigned int keyframe_period_;

  // Number of keyframes requested by now.
  unsigned int num_keyframes_requested_;

  // Next keyframe expected before next_keyframe_at_ + kMaxKeyframeDelay.
  unsigned int next_keyframe_at_;

  // True if we are asking encoder for a particular bitrate.
  bool force_bitrate_;

  // Current requested bitrate.
  unsigned int current_requested_bitrate_;

  // Current expected framerate.
  unsigned int current_framerate_;

  // Byte size of the encoded stream (for bitrate calculation) since last
  // time we checked bitrate.
  size_t encoded_stream_size_since_last_check_;

  // If true, verify performance at the end of the test.
  bool test_perf_;

  // Check the output frame quality of the encoder.
  bool verify_output_;

  // Used to perform codec-specific sanity checks on the stream.
  std::unique_ptr<StreamValidator> stream_validator_;

  // Used to validate the encoded frame quality.
  std::unique_ptr<VideoFrameQualityValidator> quality_validator_;

  // The time when the first frame is submitted for encode.
  base::TimeTicks first_frame_start_time_;

  // The time when the last encoded frame is ready.
  base::TimeTicks last_frame_ready_time_;

  // All methods of this class should be run on the same thread.
  base::ThreadChecker thread_checker_;

  // Requested bitrate in bits per second.
  unsigned int requested_bitrate_;

  // Requested initial framerate.
  unsigned int requested_framerate_;

  // Bitrate to switch to in the middle of the stream.
  unsigned int requested_subsequent_bitrate_;

  // Framerate to switch to in the middle of the stream.
  unsigned int requested_subsequent_framerate_;

  // The timer used to feed the encoder with the input frames.
  std::unique_ptr<base::RepeatingTimer> input_timer_;
};

VEAClient::VEAClient(TestStream* test_stream,
                     ClientStateNotification<ClientState>* note,
                     bool save_to_file,
                     unsigned int keyframe_period,
                     bool force_bitrate,
                     bool test_perf,
                     bool mid_stream_bitrate_switch,
                     bool mid_stream_framerate_switch,
                     bool verify_output)
    : state_(CS_CREATED),
      test_stream_(test_stream),
      note_(note),
      next_input_id_(0),
      next_output_buffer_id_(0),
      pos_in_input_stream_(0),
      num_required_input_buffers_(0),
      output_buffer_size_(0),
      num_frames_to_encode_(0),
      num_encoded_frames_(0),
      num_frames_since_last_check_(0),
      seen_keyframe_in_this_buffer_(false),
      save_to_file_(save_to_file),
      keyframe_period_(keyframe_period),
      num_keyframes_requested_(0),
      next_keyframe_at_(0),
      force_bitrate_(force_bitrate),
      current_requested_bitrate_(0),
      current_framerate_(0),
      encoded_stream_size_since_last_check_(0),
      test_perf_(test_perf),
      verify_output_(verify_output),
      requested_bitrate_(0),
      requested_framerate_(0),
      requested_subsequent_bitrate_(0),
      requested_subsequent_framerate_(0) {
  if (keyframe_period_)
    LOG_ASSERT(kMaxKeyframeDelay < keyframe_period_);

  // Fake encoder produces an invalid stream, so skip validating it.
  if (!g_fake_encoder) {
    stream_validator_ = StreamValidator::Create(
        test_stream_->requested_profile,
        base::Bind(&VEAClient::HandleEncodedFrame, base::Unretained(this)));
    CHECK(stream_validator_);
  }

  if (save_to_file_) {
    LOG_ASSERT(!test_stream_->out_filename.empty());
    base::FilePath out_filename(test_stream_->out_filename);
    // This creates or truncates out_filename.
    // Without it, AppendToFile() will not work.
    EXPECT_EQ(0, base::WriteFile(out_filename, NULL, 0));
  }

  // Initialize the parameters of the test streams.
  UpdateTestStreamData(mid_stream_bitrate_switch, mid_stream_framerate_switch);

  thread_checker_.DetachFromThread();
}

VEAClient::~VEAClient() {
  LOG_ASSERT(!has_encoder());
}

std::unique_ptr<VideoEncodeAccelerator> VEAClient::CreateFakeVEA() {
  std::unique_ptr<VideoEncodeAccelerator> encoder;
  if (g_fake_encoder) {
    encoder.reset(new FakeVideoEncodeAccelerator(
        scoped_refptr<base::SingleThreadTaskRunner>(
            base::ThreadTaskRunnerHandle::Get())));
  }
  return encoder;
}

std::unique_ptr<VideoEncodeAccelerator> VEAClient::CreateV4L2VEA() {
  std::unique_ptr<VideoEncodeAccelerator> encoder;
#if defined(OS_CHROMEOS) && (defined(ARCH_CPU_ARMEL) || \
                             (defined(USE_OZONE) && defined(USE_V4L2_CODEC)))
  scoped_refptr<V4L2Device> device = V4L2Device::Create(V4L2Device::kEncoder);
  if (device)
    encoder.reset(new V4L2VideoEncodeAccelerator(device));
#endif
  return encoder;
}

std::unique_ptr<VideoEncodeAccelerator> VEAClient::CreateVaapiVEA() {
  std::unique_ptr<VideoEncodeAccelerator> encoder;
#if defined(OS_CHROMEOS) && defined(ARCH_CPU_X86_FAMILY)
  encoder.reset(new VaapiVideoEncodeAccelerator());
#endif
  return encoder;
}

std::unique_ptr<VideoEncodeAccelerator> VEAClient::CreateVTVEA() {
  std::unique_ptr<VideoEncodeAccelerator> encoder;
#if defined(OS_MACOSX)
  encoder.reset(new VTVideoEncodeAccelerator());
#endif
  return encoder;
}

void VEAClient::CreateEncoder() {
  DCHECK(thread_checker_.CalledOnValidThread());
  LOG_ASSERT(!has_encoder());

  std::unique_ptr<VideoEncodeAccelerator> encoders[] = {
      CreateFakeVEA(), CreateV4L2VEA(), CreateVaapiVEA(), CreateVTVEA()};

  DVLOG(1) << "Profile: " << test_stream_->requested_profile
           << ", initial bitrate: " << requested_bitrate_;

  for (size_t i = 0; i < arraysize(encoders); ++i) {
    if (!encoders[i])
      continue;
    encoder_ = std::move(encoders[i]);
    SetState(CS_ENCODER_SET);
    if (encoder_->Initialize(kInputFormat, test_stream_->visible_size,
                             test_stream_->requested_profile,
                             requested_bitrate_, this)) {
      SetStreamParameters(requested_bitrate_, requested_framerate_);
      SetState(CS_INITIALIZED);

      if (verify_output_ && !g_fake_encoder)
        quality_validator_.reset(new VideoFrameQualityValidator(
            test_stream_->requested_profile,
            base::Bind(&VEAClient::DecodeCompleted, base::Unretained(this)),
            base::Bind(&VEAClient::DecodeFailed, base::Unretained(this))));
      return;
    }
  }
  encoder_.reset();
  LOG(ERROR) << "VideoEncodeAccelerator::Initialize() failed";
  SetState(CS_ERROR);
}

void VEAClient::DecodeCompleted() {
  SetState(CS_VALIDATED);
}

void VEAClient::DecodeFailed() {
  SetState(CS_ERROR);
}

void VEAClient::DestroyEncoder() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!has_encoder())
    return;
  // Clear the objects that should be destroyed on the same thread as creation.
  encoder_.reset();
  input_timer_.reset();
  quality_validator_.reset();
}

void VEAClient::UpdateTestStreamData(bool mid_stream_bitrate_switch,
                                     bool mid_stream_framerate_switch) {
  // Use defaults for bitrate/framerate if they are not provided.
  if (test_stream_->requested_bitrate == 0)
    requested_bitrate_ = kDefaultBitrate;
  else
    requested_bitrate_ = test_stream_->requested_bitrate;

  if (test_stream_->requested_framerate == 0)
    requested_framerate_ = kDefaultFramerate;
  else
    requested_framerate_ = test_stream_->requested_framerate;

  // If bitrate/framerate switch is requested, use the subsequent values if
  // provided, or, if not, calculate them from their initial values using
  // the default ratios.
  // Otherwise, if a switch is not requested, keep the initial values.
  if (mid_stream_bitrate_switch) {
    if (test_stream_->requested_subsequent_bitrate == 0)
      requested_subsequent_bitrate_ =
          requested_bitrate_ * kDefaultSubsequentBitrateRatio;
    else
      requested_subsequent_bitrate_ =
          test_stream_->requested_subsequent_bitrate;
  } else {
    requested_subsequent_bitrate_ = requested_bitrate_;
  }
  if (requested_subsequent_bitrate_ == 0)
    requested_subsequent_bitrate_ = 1;

  if (mid_stream_framerate_switch) {
    if (test_stream_->requested_subsequent_framerate == 0)
      requested_subsequent_framerate_ =
          requested_framerate_ * kDefaultSubsequentFramerateRatio;
    else
      requested_subsequent_framerate_ =
          test_stream_->requested_subsequent_framerate;
  } else {
    requested_subsequent_framerate_ = requested_framerate_;
  }
  if (requested_subsequent_framerate_ == 0)
    requested_subsequent_framerate_ = 1;
}

double VEAClient::frames_per_second() {
  LOG_ASSERT(num_encoded_frames_ != 0UL);
  base::TimeDelta duration = last_frame_ready_time_ - first_frame_start_time_;
  return num_encoded_frames_ / duration.InSecondsF();
}

void VEAClient::RequireBitstreamBuffers(unsigned int input_count,
                                        const gfx::Size& input_coded_size,
                                        size_t output_size) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ASSERT_EQ(state_, CS_INITIALIZED);
  SetState(CS_ENCODING);

  if (quality_validator_)
    quality_validator_->Initialize(input_coded_size,
                                   gfx::Rect(test_stream_->visible_size));

  CreateAlignedInputStreamFile(input_coded_size, test_stream_);

  num_frames_to_encode_ = test_stream_->num_frames;
  if (g_num_frames_to_encode > 0)
    num_frames_to_encode_ = g_num_frames_to_encode;

  // We may need to loop over the stream more than once if more frames than
  // provided is required for bitrate tests.
  if (force_bitrate_ && num_frames_to_encode_ < kMinFramesForBitrateTests) {
    DVLOG(1) << "Stream too short for bitrate test ("
             << test_stream_->num_frames << " frames), will loop it to reach "
             << kMinFramesForBitrateTests << " frames";
    num_frames_to_encode_ = kMinFramesForBitrateTests;
  }
  if (save_to_file_ && IsVP8(test_stream_->requested_profile))
    WriteIvfFileHeader();

  input_coded_size_ = input_coded_size;
  num_required_input_buffers_ = input_count;
  ASSERT_GT(num_required_input_buffers_, 0UL);

  output_buffer_size_ = output_size;
  ASSERT_GT(output_buffer_size_, 0UL);

  for (unsigned int i = 0; i < kNumOutputBuffers; ++i) {
    base::SharedMemory* shm = new base::SharedMemory();
    LOG_ASSERT(shm->CreateAndMapAnonymous(output_buffer_size_));
    output_shms_.push_back(shm);
    FeedEncoderWithOutput(shm);
  }

  if (g_env->run_at_fps()) {
    input_timer_.reset(new base::RepeatingTimer());
    input_timer_->Start(
        FROM_HERE, base::TimeDelta::FromSeconds(1) / current_framerate_,
        base::Bind(&VEAClient::OnInputTimer, base::Unretained(this)));
  } else {
    while (inputs_at_client_.size() <
           num_required_input_buffers_ + kNumExtraInputFrames)
      FeedEncoderWithOneInput();
  }
}

void VEAClient::BitstreamBufferReady(int32_t bitstream_buffer_id,
                                     size_t payload_size,
                                     bool key_frame,
                                     base::TimeDelta timestamp) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ASSERT_LE(payload_size, output_buffer_size_);

  IdToSHM::iterator it = output_buffers_at_client_.find(bitstream_buffer_id);
  ASSERT_NE(it, output_buffers_at_client_.end());
  base::SharedMemory* shm = it->second;
  output_buffers_at_client_.erase(it);

  if (state_ == CS_FINISHED || state_ == CS_VALIDATED)
    return;

  encoded_stream_size_since_last_check_ += payload_size;

  const uint8_t* stream_ptr = static_cast<const uint8_t*>(shm->memory());
  if (payload_size > 0) {
    if (stream_validator_) {
      stream_validator_->ProcessStreamBuffer(stream_ptr, payload_size);
    } else {
      HandleEncodedFrame(key_frame);
    }

    if (quality_validator_) {
      scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CopyFrom(
          reinterpret_cast<const uint8_t*>(shm->memory()),
          static_cast<int>(payload_size)));
      quality_validator_->AddDecodeBuffer(buffer);
      // Insert EOS buffer to flush the decoder.
      if (num_encoded_frames_ == num_frames_to_encode_)
        quality_validator_->Flush();
    }

    if (save_to_file_) {
      if (IsVP8(test_stream_->requested_profile))
        WriteIvfFrameHeader(num_encoded_frames_ - 1, payload_size);

      EXPECT_TRUE(base::AppendToFile(
          base::FilePath::FromUTF8Unsafe(test_stream_->out_filename),
          static_cast<char*>(shm->memory()),
          base::checked_cast<int>(payload_size)));
    }
  }

  EXPECT_EQ(key_frame, seen_keyframe_in_this_buffer_);
  seen_keyframe_in_this_buffer_ = false;

  FeedEncoderWithOutput(shm);
}

void VEAClient::NotifyError(VideoEncodeAccelerator::Error error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  SetState(CS_ERROR);
}

void VEAClient::SetState(ClientState new_state) {
  DVLOG(4) << "Changing state " << state_ << "->" << new_state;
  note_->Notify(new_state);
  state_ = new_state;
}

void VEAClient::SetStreamParameters(unsigned int bitrate,
                                    unsigned int framerate) {
  current_requested_bitrate_ = bitrate;
  current_framerate_ = framerate;
  LOG_ASSERT(current_requested_bitrate_ > 0UL);
  LOG_ASSERT(current_framerate_ > 0UL);
  encoder_->RequestEncodingParametersChange(current_requested_bitrate_,
                                            current_framerate_);
  DVLOG(1) << "Switched parameters to " << current_requested_bitrate_
           << " bps @ " << current_framerate_ << " FPS";
}

void VEAClient::InputNoLongerNeededCallback(int32_t input_id) {
  std::set<int32_t>::iterator it = inputs_at_client_.find(input_id);
  ASSERT_NE(it, inputs_at_client_.end());
  inputs_at_client_.erase(it);
  if (!g_env->run_at_fps())
    FeedEncoderWithOneInput();
}

scoped_refptr<VideoFrame> VEAClient::CreateFrame(off_t position) {
  uint8_t* frame_data_y =
      reinterpret_cast<uint8_t*>(&test_stream_->aligned_in_file_data[0]) +
      position;
  uint8_t* frame_data_u = frame_data_y + test_stream_->aligned_plane_size[0];
  uint8_t* frame_data_v = frame_data_u + test_stream_->aligned_plane_size[1];
  CHECK_GT(current_framerate_, 0U);

  scoped_refptr<VideoFrame> video_frame = VideoFrame::WrapExternalYuvData(
      kInputFormat, input_coded_size_, gfx::Rect(test_stream_->visible_size),
      test_stream_->visible_size, input_coded_size_.width(),
      input_coded_size_.width() / 2, input_coded_size_.width() / 2,
      frame_data_y, frame_data_u, frame_data_v,
      base::TimeDelta().FromMilliseconds(next_input_id_ *
                                         base::Time::kMillisecondsPerSecond /
                                         current_framerate_));
  EXPECT_NE(nullptr, video_frame.get());
  return video_frame;
}

scoped_refptr<VideoFrame> VEAClient::PrepareInputFrame(off_t position,
                                                       int32_t* input_id) {
  CHECK_LE(position + test_stream_->aligned_buffer_size,
           test_stream_->aligned_in_file_data.size());

  scoped_refptr<VideoFrame> frame = CreateFrame(position);
  EXPECT_TRUE(frame);
  frame->AddDestructionObserver(
      BindToCurrentLoop(base::Bind(&VEAClient::InputNoLongerNeededCallback,
                                   base::Unretained(this), next_input_id_)));

  LOG_ASSERT(inputs_at_client_.insert(next_input_id_).second);

  *input_id = next_input_id_++;
  return frame;
}

void VEAClient::OnInputTimer() {
  if (!has_encoder() || state_ != CS_ENCODING)
    input_timer_.reset();
  else if (inputs_at_client_.size() <
           num_required_input_buffers_ + kNumExtraInputFrames)
    FeedEncoderWithOneInput();
  else
    DVLOG(1) << "Dropping input frame";
}

void VEAClient::FeedEncoderWithOneInput() {
  if (!has_encoder() || state_ != CS_ENCODING)
    return;

  size_t bytes_left =
      test_stream_->aligned_in_file_data.size() - pos_in_input_stream_;
  if (bytes_left < test_stream_->aligned_buffer_size) {
    DCHECK_EQ(bytes_left, 0UL);
    // Rewind if at the end of stream and we are still encoding.
    // This is to flush the encoder with additional frames from the beginning
    // of the stream, or if the stream is shorter that the number of frames
    // we require for bitrate tests.
    pos_in_input_stream_ = 0;
  }

  if (quality_validator_)
    quality_validator_->AddOriginalFrame(CreateFrame(pos_in_input_stream_));

  int32_t input_id;
  scoped_refptr<VideoFrame> video_frame =
      PrepareInputFrame(pos_in_input_stream_, &input_id);
  pos_in_input_stream_ += test_stream_->aligned_buffer_size;

  bool force_keyframe = false;
  if (keyframe_period_ && input_id % keyframe_period_ == 0) {
    force_keyframe = true;
    ++num_keyframes_requested_;
  }

  if (input_id == 0) {
    first_frame_start_time_ = base::TimeTicks::Now();
  }

  if (g_env->needs_encode_latency()) {
    LOG_ASSERT(input_id == static_cast<int32_t>(encode_start_time_.size()));
    encode_start_time_.push_back(base::TimeTicks::Now());
  }
  encoder_->Encode(video_frame, force_keyframe);
}

void VEAClient::FeedEncoderWithOutput(base::SharedMemory* shm) {
  if (!has_encoder())
    return;

  if (state_ != CS_ENCODING)
    return;

  base::SharedMemoryHandle dup_handle;
  LOG_ASSERT(shm->ShareToProcess(base::GetCurrentProcessHandle(), &dup_handle));

  BitstreamBuffer bitstream_buffer(next_output_buffer_id_++, dup_handle,
                                   output_buffer_size_);
  LOG_ASSERT(output_buffers_at_client_
                 .insert(std::make_pair(bitstream_buffer.id(), shm))
                 .second);
  encoder_->UseOutputBitstreamBuffer(bitstream_buffer);
}

bool VEAClient::HandleEncodedFrame(bool keyframe) {
  // This would be a bug in the test, which should not ignore false
  // return value from this method.
  LOG_ASSERT(num_encoded_frames_ <= num_frames_to_encode_);

  last_frame_ready_time_ = base::TimeTicks::Now();

  if (g_env->needs_encode_latency()) {
    LOG_ASSERT(num_encoded_frames_ < encode_start_time_.size());
    base::TimeTicks start_time = encode_start_time_[num_encoded_frames_];
    LOG_ASSERT(!start_time.is_null());
    encode_latencies_.push_back(last_frame_ready_time_ - start_time);
  }

  ++num_encoded_frames_;
  ++num_frames_since_last_check_;

  // Because the keyframe behavior requirements are loose, we give
  // the encoder more freedom here. It could either deliver a keyframe
  // immediately after we requested it, which could be for a frame number
  // before the one we requested it for (if the keyframe request
  // is asynchronous, i.e. not bound to any concrete frame, and because
  // the pipeline can be deeper than one frame), at that frame, or after.
  // So the only constraints we put here is that we get a keyframe not
  // earlier than we requested one (in time), and not later than
  // kMaxKeyframeDelay frames after the frame, for which we requested
  // it, comes back encoded.
  if (keyframe) {
    if (num_keyframes_requested_ > 0) {
      --num_keyframes_requested_;
      next_keyframe_at_ += keyframe_period_;
    }
    seen_keyframe_in_this_buffer_ = true;
  }

  if (num_keyframes_requested_ > 0)
    EXPECT_LE(num_encoded_frames_, next_keyframe_at_ + kMaxKeyframeDelay);

  if (num_encoded_frames_ == num_frames_to_encode_ / 2) {
    VerifyStreamProperties();
    if (requested_subsequent_bitrate_ != current_requested_bitrate_ ||
        requested_subsequent_framerate_ != current_framerate_) {
      SetStreamParameters(requested_subsequent_bitrate_,
                          requested_subsequent_framerate_);
      if (g_env->run_at_fps() && input_timer_)
        input_timer_->Start(
            FROM_HERE, base::TimeDelta::FromSeconds(1) / current_framerate_,
            base::Bind(&VEAClient::OnInputTimer, base::Unretained(this)));
    }
  } else if (num_encoded_frames_ == num_frames_to_encode_) {
    LogPerf();
    VerifyMinFPS();
    VerifyStreamProperties();
    SetState(CS_FINISHED);
    if (!quality_validator_)
      SetState(CS_VALIDATED);
    return false;
  }

  return true;
}

void VEAClient::LogPerf() {
  g_env->LogToFile("Measured encoder FPS",
                   base::StringPrintf("%.3f", frames_per_second()));

  // Log encode latencies.
  if (g_env->needs_encode_latency()) {
    std::sort(encode_latencies_.begin(), encode_latencies_.end());
    for (const auto& percentile : kLoggedLatencyPercentiles) {
      base::TimeDelta latency = Percentile(encode_latencies_, percentile);
      g_env->LogToFile(
          base::StringPrintf("Encode latency for the %dth percentile",
                             percentile),
          base::StringPrintf("%" PRId64 " us", latency.InMicroseconds()));
    }
  }
}

void VEAClient::VerifyMinFPS() {
  if (test_perf_)
    EXPECT_GE(frames_per_second(), kMinPerfFPS);
}

void VEAClient::VerifyStreamProperties() {
  LOG_ASSERT(num_frames_since_last_check_ > 0UL);
  LOG_ASSERT(encoded_stream_size_since_last_check_ > 0UL);
  unsigned int bitrate = encoded_stream_size_since_last_check_ * 8 *
                         current_framerate_ / num_frames_since_last_check_;
  DVLOG(1) << "Current chunk's bitrate: " << bitrate
           << " (expected: " << current_requested_bitrate_
           << " @ " << current_framerate_ << " FPS,"
           << " num frames in chunk: " << num_frames_since_last_check_;

  num_frames_since_last_check_ = 0;
  encoded_stream_size_since_last_check_ = 0;

  if (force_bitrate_) {
    EXPECT_NEAR(bitrate, current_requested_bitrate_,
                kBitrateTolerance * current_requested_bitrate_);
  }

  // All requested keyframes should've been provided. Allow the last requested
  // frame to remain undelivered if we haven't reached the maximum frame number
  // by which it should have arrived.
  if (num_encoded_frames_ < next_keyframe_at_ + kMaxKeyframeDelay)
    EXPECT_LE(num_keyframes_requested_, 1UL);
  else
    EXPECT_EQ(num_keyframes_requested_, 0UL);
}

void VEAClient::WriteIvfFileHeader() {
  IvfFileHeader header = {};

  memcpy(header.signature, kIvfHeaderSignature, sizeof(header.signature));
  header.version = 0;
  header.header_size = sizeof(header);
  header.fourcc = 0x30385056;  // VP80
  header.width =
      base::checked_cast<uint16_t>(test_stream_->visible_size.width());
  header.height =
      base::checked_cast<uint16_t>(test_stream_->visible_size.height());
  header.timebase_denum = requested_framerate_;
  header.timebase_num = 1;
  header.num_frames = num_frames_to_encode_;
  header.ByteSwap();

  EXPECT_TRUE(base::AppendToFile(
      base::FilePath::FromUTF8Unsafe(test_stream_->out_filename),
      reinterpret_cast<char*>(&header), sizeof(header)));
}

void VEAClient::WriteIvfFrameHeader(int frame_index, size_t frame_size) {
  IvfFrameHeader header = {};

  header.frame_size = frame_size;
  header.timestamp = frame_index;
  header.ByteSwap();
  EXPECT_TRUE(base::AppendToFile(
      base::FilePath::FromUTF8Unsafe(test_stream_->out_filename),
      reinterpret_cast<char*>(&header), sizeof(header)));
}

// Test parameters:
// - Number of concurrent encoders. The value takes effect when there is only
//   one input stream; otherwise, one encoder per input stream will be
//   instantiated.
// - If true, save output to file (provided an output filename was supplied).
// - Force a keyframe every n frames.
// - Force bitrate; the actual required value is provided as a property
//   of the input stream, because it depends on stream type/resolution/etc.
// - If true, measure performance.
// - If true, switch bitrate mid-stream.
// - If true, switch framerate mid-stream.
// - If true, verify the output frames of encoder.
class VideoEncodeAcceleratorTest
    : public ::testing::TestWithParam<
          std::tuple<int, bool, int, bool, bool, bool, bool, bool>> {};

TEST_P(VideoEncodeAcceleratorTest, TestSimpleEncode) {
  size_t num_concurrent_encoders = std::get<0>(GetParam());
  const bool save_to_file = std::get<1>(GetParam());
  const unsigned int keyframe_period = std::get<2>(GetParam());
  const bool force_bitrate = std::get<3>(GetParam());
  const bool test_perf = std::get<4>(GetParam());
  const bool mid_stream_bitrate_switch = std::get<5>(GetParam());
  const bool mid_stream_framerate_switch = std::get<6>(GetParam());
  const bool verify_output =
      std::get<7>(GetParam()) || g_env->verify_all_output();

  ScopedVector<ClientStateNotification<ClientState>> notes;
  ScopedVector<VEAClient> clients;
  base::Thread encoder_thread("EncoderThread");
  ASSERT_TRUE(encoder_thread.Start());

  if (g_env->test_streams_.size() > 1)
    num_concurrent_encoders = g_env->test_streams_.size();

  // Create all encoders.
  for (size_t i = 0; i < num_concurrent_encoders; i++) {
    size_t test_stream_index = i % g_env->test_streams_.size();
    // Disregard save_to_file if we didn't get an output filename.
    bool encoder_save_to_file =
        (save_to_file &&
         !g_env->test_streams_[test_stream_index]->out_filename.empty());

    notes.push_back(new ClientStateNotification<ClientState>());
    clients.push_back(new VEAClient(
        g_env->test_streams_[test_stream_index], notes.back(),
        encoder_save_to_file, keyframe_period, force_bitrate, test_perf,
        mid_stream_bitrate_switch, mid_stream_framerate_switch, verify_output));

    encoder_thread.task_runner()->PostTask(
        FROM_HERE, base::Bind(&VEAClient::CreateEncoder,
                              base::Unretained(clients.back())));
  }

  // All encoders must pass through states in this order.
  enum ClientState state_transitions[] = {
      CS_ENCODER_SET, CS_INITIALIZED, CS_ENCODING, CS_FINISHED, CS_VALIDATED};

  // Wait for all encoders to go through all states and finish.
  // Do this by waiting for all encoders to advance to state n before checking
  // state n+1, to verify that they are able to operate concurrently.
  // It also simulates the real-world usage better, as the main thread, on which
  // encoders are created/destroyed, is a single GPU Process ChildThread.
  // Moreover, we can't have proper multithreading on X11, so this could cause
  // hard to debug issues there, if there were multiple "ChildThreads".
  for (size_t state_no = 0; state_no < arraysize(state_transitions);
       ++state_no) {
    for (size_t i = 0; i < num_concurrent_encoders; i++)
      ASSERT_EQ(notes[i]->Wait(), state_transitions[state_no]);
  }

  for (size_t i = 0; i < num_concurrent_encoders; ++i) {
    encoder_thread.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&VEAClient::DestroyEncoder, base::Unretained(clients[i])));
  }

  // This ensures all tasks have finished.
  encoder_thread.Stop();
}

#if !defined(OS_MACOSX)
INSTANTIATE_TEST_CASE_P(
    SimpleEncode,
    VideoEncodeAcceleratorTest,
    ::testing::Values(
        std::make_tuple(1, true, 0, false, false, false, false, false),
        std::make_tuple(1, true, 0, false, false, false, false, true)));

INSTANTIATE_TEST_CASE_P(
    EncoderPerf,
    VideoEncodeAcceleratorTest,
    ::testing::Values(
        std::make_tuple(1, false, 0, false, true, false, false, false)));

INSTANTIATE_TEST_CASE_P(
    ForceKeyframes,
    VideoEncodeAcceleratorTest,
    ::testing::Values(
        std::make_tuple(1, false, 10, false, false, false, false, false)));

INSTANTIATE_TEST_CASE_P(
    ForceBitrate,
    VideoEncodeAcceleratorTest,
    ::testing::Values(
        std::make_tuple(1, false, 0, true, false, false, false, false)));

INSTANTIATE_TEST_CASE_P(
    MidStreamParamSwitchBitrate,
    VideoEncodeAcceleratorTest,
    ::testing::Values(
        std::make_tuple(1, false, 0, true, false, true, false, false)));

INSTANTIATE_TEST_CASE_P(
    MidStreamParamSwitchFPS,
    VideoEncodeAcceleratorTest,
    ::testing::Values(
        std::make_tuple(1, false, 0, true, false, false, true, false)));

INSTANTIATE_TEST_CASE_P(
    MultipleEncoders,
    VideoEncodeAcceleratorTest,
    ::testing::Values(
        std::make_tuple(3, false, 0, false, false, false, false, false),
        std::make_tuple(3, false, 0, true, false, false, true, false),
        std::make_tuple(3, false, 0, true, false, true, false, false)));
#else
INSTANTIATE_TEST_CASE_P(
    SimpleEncode,
    VideoEncodeAcceleratorTest,
    ::testing::Values(
        std::make_tuple(1, true, 0, false, false, false, false, false),
        std::make_tuple(1, true, 0, false, false, false, false, true)));

INSTANTIATE_TEST_CASE_P(
    EncoderPerf,
    VideoEncodeAcceleratorTest,
    ::testing::Values(
        std::make_tuple(1, false, 0, false, true, false, false, false)));

INSTANTIATE_TEST_CASE_P(
    MultipleEncoders,
    VideoEncodeAcceleratorTest,
    ::testing::Values(
        std::make_tuple(3, false, 0, false, false, false, false, false)));
#endif

// TODO(posciak): more tests:
// - async FeedEncoderWithOutput
// - out-of-order return of outputs to encoder
// - multiple encoders + decoders
// - mid-stream encoder_->Destroy()

}  // namespace
}  // namespace media

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);  // Removes gtest-specific args.
  base::CommandLine::Init(argc, argv);

  base::ShadowingAtExitManager at_exit_manager;
  base::MessageLoop main_loop;

  std::unique_ptr<base::FilePath::StringType> test_stream_data(
      new base::FilePath::StringType(
          media::GetTestDataFilePath(media::g_default_in_filename).value() +
          media::g_default_in_parameters));

  // Needed to enable DVLOG through --vmodule.
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  LOG_ASSERT(logging::InitLogging(settings));

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  DCHECK(cmd_line);

  bool run_at_fps = false;
  bool needs_encode_latency = false;
  bool verify_all_output = false;
  base::FilePath log_path;

  base::CommandLine::SwitchMap switches = cmd_line->GetSwitches();
  for (base::CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    if (it->first == "test_stream_data") {
      test_stream_data->assign(it->second.c_str());
      continue;
    }
    // Output machine-readable logs with fixed formats to a file.
    if (it->first == "output_log") {
      log_path = base::FilePath(
          base::FilePath::StringType(it->second.begin(), it->second.end()));
      continue;
    }
    if (it->first == "num_frames_to_encode") {
      std::string input(it->second.begin(), it->second.end());
      LOG_ASSERT(base::StringToInt(input, &media::g_num_frames_to_encode));
      continue;
    }
    if (it->first == "measure_latency") {
      needs_encode_latency = true;
      continue;
    }
    if (it->first == "fake_encoder") {
      media::g_fake_encoder = true;
      continue;
    }
    if (it->first == "run_at_fps") {
      run_at_fps = true;
      continue;
    }
    if (it->first == "verify_all_output") {
      verify_all_output = true;
      continue;
    }
    if (it->first == "v" || it->first == "vmodule")
      continue;
    if (it->first == "ozone-platform" || it->first == "ozone-use-surfaceless")
      continue;
    LOG(FATAL) << "Unexpected switch: " << it->first << ":" << it->second;
  }

  if (needs_encode_latency && !run_at_fps) {
    // Encode latency can only be measured with --run_at_fps. Otherwise, we get
    // skewed results since it may queue too many frames at once with the same
    // encode start time.
    LOG(FATAL) << "--measure_latency requires --run_at_fps enabled to work.";
  }

#if defined(OS_CHROMEOS) && defined(ARCH_CPU_X86_FAMILY)
  media::VaapiWrapper::PreSandboxInitialization();
#endif

  media::g_env =
      reinterpret_cast<media::VideoEncodeAcceleratorTestEnvironment*>(
          testing::AddGlobalTestEnvironment(
              new media::VideoEncodeAcceleratorTestEnvironment(
                  std::move(test_stream_data), log_path, run_at_fps,
                  needs_encode_latency, verify_all_output)));

  return RUN_ALL_TESTS();
}
