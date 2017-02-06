// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The bulk of this file is support code; sorry about that.  Here's an overview
// to hopefully help readers of this code:
// - RenderingHelper is charged with interacting with X11/{EGL/GLES2,GLX/GL} or
//   Win/EGL.
// - ClientState is an enum for the state of the decode client used by the test.
// - ClientStateNotification is a barrier abstraction that allows the test code
//   to be written sequentially and wait for the decode client to see certain
//   state transitions.
// - GLRenderingVDAClient is a VideoDecodeAccelerator::Client implementation
// - Finally actual TEST cases are at the bottom of this file, using the above
//   infrastructure.

#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <deque>
#include <map>
#include <memory>
#include <tuple>
#include <utility>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/md5.h"
#include "base/process/process_handle.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringize_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "media/filters/h264_parser.h"
#include "media/gpu/fake_video_decode_accelerator.h"
#include "media/gpu/gpu_video_decode_accelerator_factory_impl.h"
#include "media/gpu/rendering_helper.h"
#include "media/gpu/video_accelerator_unittest_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gl/gl_image.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "media/gpu/dxva_video_decode_accelerator_win.h"
#elif defined(OS_CHROMEOS)
#if defined(USE_V4L2_CODEC)
#include "media/gpu/v4l2_device.h"
#include "media/gpu/v4l2_slice_video_decode_accelerator.h"
#include "media/gpu/v4l2_video_decode_accelerator.h"
#endif
#if defined(ARCH_CPU_X86_FAMILY)
#include "media/gpu/vaapi_video_decode_accelerator.h"
#include "media/gpu/vaapi_wrapper.h"
#endif  // defined(ARCH_CPU_X86_FAMILY)
#else
#error The VideoAccelerator tests are not supported on this platform.
#endif  // OS_WIN

#if defined(USE_OZONE)
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/ozone_gpu_test_helper.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif  // defined(USE_OZONE)

namespace media {

namespace {

// Values optionally filled in from flags; see main() below.
// The syntax of multiple test videos is:
//  test-video1;test-video2;test-video3
// where only the first video is required and other optional videos would be
// decoded by concurrent decoders.
// The syntax of each test-video is:
//  filename:width:height:numframes:numfragments:minFPSwithRender:minFPSnoRender
// where only the first field is required.  Value details:
// - |filename| must be an h264 Annex B (NAL) stream or an IVF VP8/9 stream.
// - |width| and |height| are in pixels.
// - |numframes| is the number of picture frames in the file.
// - |numfragments| NALU (h264) or frame (VP8/9) count in the stream.
// - |minFPSwithRender| and |minFPSnoRender| are minimum frames/second speeds
//   expected to be achieved with and without rendering to the screen, resp.
//   (the latter tests just decode speed).
// - |profile| is the VideoCodecProfile set during Initialization.
// An empty value for a numeric field means "ignore".
const base::FilePath::CharType* g_test_video_data =
    // FILE_PATH_LITERAL("test-25fps.vp8:320:240:250:250:50:175:11");
    FILE_PATH_LITERAL("test-25fps.h264:320:240:250:258:50:175:1");

// The file path of the test output log. This is used to communicate the test
// results to CrOS autotests. We can enable the log and specify the filename by
// the "--output_log" switch.
const base::FilePath::CharType* g_output_log = NULL;

// The value is set by the switch "--rendering_fps".
double g_rendering_fps = 60;

// The value is set by the switch "--rendering_warm_up".
int g_rendering_warm_up = 0;

// The value is set by the switch "--num_play_throughs". The video will play
// the specified number of times. In different test cases, we have different
// values for |num_play_throughs|. This setting will override the value. A
// special value "0" means no override.
int g_num_play_throughs = 0;
// Fake decode
int g_fake_decoder = 0;

// Test buffer import into VDA, providing buffers allocated by us, instead of
// requesting the VDA itself to allocate buffers.
bool g_test_import = false;

// Environment to store rendering thread.
class VideoDecodeAcceleratorTestEnvironment;
VideoDecodeAcceleratorTestEnvironment* g_env;

// Magic constants for differentiating the reasons for NotifyResetDone being
// called.
enum ResetPoint {
  // Reset() just after calling Decode() with a fragment containing config info.
  RESET_AFTER_FIRST_CONFIG_INFO = -4,
  START_OF_STREAM_RESET = -3,
  MID_STREAM_RESET = -2,
  END_OF_STREAM_RESET = -1
};

const int kMaxResetAfterFrameNum = 100;
const int kMaxFramesToDelayReuse = 64;
const base::TimeDelta kReuseDelay = base::TimeDelta::FromSeconds(1);
// Simulate WebRTC and call VDA::Decode 30 times per second.
const int kWebRtcDecodeCallsPerSecond = 30;
// Simulate an adjustment to a larger number of pictures to make sure the
// decoder supports an upwards adjustment.
const int kExtraPictureBuffers = 2;

struct TestVideoFile {
  explicit TestVideoFile(base::FilePath::StringType file_name)
      : file_name(file_name),
        width(-1),
        height(-1),
        num_frames(-1),
        num_fragments(-1),
        min_fps_render(-1),
        min_fps_no_render(-1),
        profile(VIDEO_CODEC_PROFILE_UNKNOWN),
        reset_after_frame_num(END_OF_STREAM_RESET) {}

  base::FilePath::StringType file_name;
  int width;
  int height;
  int num_frames;
  int num_fragments;
  int min_fps_render;
  int min_fps_no_render;
  VideoCodecProfile profile;
  int reset_after_frame_num;
  std::string data_str;
};

const gfx::Size kThumbnailsPageSize(1600, 1200);
const gfx::Size kThumbnailSize(160, 120);
const int kMD5StringLength = 32;

// Read in golden MD5s for the thumbnailed rendering of this video
void ReadGoldenThumbnailMD5s(const TestVideoFile* video_file,
                             std::vector<std::string>* md5_strings) {
  base::FilePath filepath(video_file->file_name);
  filepath = filepath.AddExtension(FILE_PATH_LITERAL(".md5"));
  std::string all_md5s;
  base::ReadFileToString(filepath, &all_md5s);
  *md5_strings = base::SplitString(all_md5s, "\n", base::TRIM_WHITESPACE,
                                   base::SPLIT_WANT_ALL);
  // Check these are legitimate MD5s.
  for (const std::string& md5_string : *md5_strings) {
    // Ignore the empty string added by SplitString
    if (!md5_string.length())
      continue;
    // Ignore comments
    if (md5_string.at(0) == '#')
      continue;

    LOG_ASSERT(static_cast<int>(md5_string.length()) == kMD5StringLength)
        << md5_string;
    bool hex_only = std::count_if(md5_string.begin(), md5_string.end(),
                                  isxdigit) == kMD5StringLength;
    LOG_ASSERT(hex_only) << md5_string;
  }
  LOG_ASSERT(md5_strings->size() >= 1U) << "  MD5 checksum file ("
                                        << filepath.MaybeAsASCII()
                                        << ") missing or empty.";
}

// State of the GLRenderingVDAClient below.  Order matters here as the test
// makes assumptions about it.
enum ClientState {
  CS_CREATED = 0,
  CS_DECODER_SET = 1,
  CS_INITIALIZED = 2,
  CS_FLUSHING = 3,
  CS_FLUSHED = 4,
  CS_RESETTING = 5,
  CS_RESET = 6,
  CS_ERROR = 7,
  CS_DESTROYED = 8,
  CS_MAX,  // Must be last entry.
};

// Initialize the GPU thread for rendering. We only need to setup once
// for all test cases.
class VideoDecodeAcceleratorTestEnvironment : public ::testing::Environment {
 public:
  VideoDecodeAcceleratorTestEnvironment()
      : rendering_thread_("GLRenderingVDAClientThread") {}

  void SetUp() override {
    rendering_thread_.Start();

    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    rendering_thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&RenderingHelper::InitializeOneOff, &done));
    done.Wait();

#if defined(USE_OZONE)
    gpu_helper_.reset(new ui::OzoneGpuTestHelper());
    // Need to initialize after the rendering side since the rendering side
    // initializes the "GPU" parts of Ozone.
    //
    // This also needs to be done in the test environment since this shouldn't
    // be initialized multiple times for the same Ozone platform.
    gpu_helper_->Initialize(base::ThreadTaskRunnerHandle::Get(),
                            GetRenderingTaskRunner());
#endif
  }

  void TearDown() override {
#if defined(USE_OZONE)
    gpu_helper_.reset();
#endif
    rendering_thread_.Stop();
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetRenderingTaskRunner() const {
    return rendering_thread_.task_runner();
  }

 private:
  base::Thread rendering_thread_;
#if defined(USE_OZONE)
  std::unique_ptr<ui::OzoneGpuTestHelper> gpu_helper_;
#endif

  DISALLOW_COPY_AND_ASSIGN(VideoDecodeAcceleratorTestEnvironment);
};

// A helper class used to manage the lifetime of a Texture. Can be backed by
// either a buffer allocated by the VDA, or by a preallocated pixmap.
class TextureRef : public base::RefCounted<TextureRef> {
 public:
  static scoped_refptr<TextureRef> Create(
      uint32_t texture_id,
      const base::Closure& no_longer_needed_cb);

  static scoped_refptr<TextureRef> CreatePreallocated(
      uint32_t texture_id,
      const base::Closure& no_longer_needed_cb,
      VideoPixelFormat pixel_format,
      const gfx::Size& size);

  gfx::GpuMemoryBufferHandle ExportGpuMemoryBufferHandle() const;

  int32_t texture_id() const { return texture_id_; }

 private:
  friend class base::RefCounted<TextureRef>;

  TextureRef(uint32_t texture_id, const base::Closure& no_longer_needed_cb)
      : texture_id_(texture_id), no_longer_needed_cb_(no_longer_needed_cb) {}

  ~TextureRef();

  uint32_t texture_id_;
  base::Closure no_longer_needed_cb_;
#if defined(USE_OZONE)
  scoped_refptr<ui::NativePixmap> pixmap_;
#endif
};

TextureRef::~TextureRef() {
  base::ResetAndReturn(&no_longer_needed_cb_).Run();
}

// static
scoped_refptr<TextureRef> TextureRef::Create(
    uint32_t texture_id,
    const base::Closure& no_longer_needed_cb) {
  return make_scoped_refptr(new TextureRef(texture_id, no_longer_needed_cb));
}

#if defined(USE_OZONE)
gfx::BufferFormat VideoPixelFormatToGfxBufferFormat(
    VideoPixelFormat pixel_format) {
  switch (pixel_format) {
    case VideoPixelFormat::PIXEL_FORMAT_ARGB:
      return gfx::BufferFormat::BGRA_8888;
    case VideoPixelFormat::PIXEL_FORMAT_XRGB:
      return gfx::BufferFormat::BGRX_8888;
    case VideoPixelFormat::PIXEL_FORMAT_NV12:
      return gfx::BufferFormat::YUV_420_BIPLANAR;
    default:
      LOG_ASSERT(false) << "Unknown VideoPixelFormat";
      return gfx::BufferFormat::BGRX_8888;
  }
}
#endif

// static
scoped_refptr<TextureRef> TextureRef::CreatePreallocated(
    uint32_t texture_id,
    const base::Closure& no_longer_needed_cb,
    VideoPixelFormat pixel_format,
    const gfx::Size& size) {
  scoped_refptr<TextureRef> texture_ref;
#if defined(USE_OZONE)
  texture_ref = TextureRef::Create(texture_id, no_longer_needed_cb);
  LOG_ASSERT(texture_ref);

  ui::OzonePlatform* platform = ui::OzonePlatform::GetInstance();
  ui::SurfaceFactoryOzone* factory = platform->GetSurfaceFactoryOzone();
  gfx::BufferFormat buffer_format =
      VideoPixelFormatToGfxBufferFormat(pixel_format);
  texture_ref->pixmap_ =
      factory->CreateNativePixmap(gfx::kNullAcceleratedWidget, size,
                                  buffer_format, gfx::BufferUsage::SCANOUT);
  LOG_ASSERT(texture_ref->pixmap_);
#endif

  return texture_ref;
}

gfx::GpuMemoryBufferHandle TextureRef::ExportGpuMemoryBufferHandle() const {
  gfx::GpuMemoryBufferHandle handle;
#if defined(USE_OZONE)
  CHECK(pixmap_);
  int duped_fd = HANDLE_EINTR(dup(pixmap_->GetDmaBufFd(0)));
  LOG_ASSERT(duped_fd != -1) << "Failed duplicating dmabuf fd";
  handle.type = gfx::OZONE_NATIVE_PIXMAP;
  handle.native_pixmap_handle.fds.emplace_back(
      base::FileDescriptor(duped_fd, true));
  handle.native_pixmap_handle.strides_and_offsets.emplace_back(
      pixmap_->GetDmaBufPitch(0), pixmap_->GetDmaBufOffset(0));
#endif
  return handle;
}

// Client that can accept callbacks from a VideoDecodeAccelerator and is used by
// the TESTs below.
class GLRenderingVDAClient
    : public VideoDecodeAccelerator::Client,
      public base::SupportsWeakPtr<GLRenderingVDAClient> {
 public:
  // |window_id| the window_id of the client, which is used to identify the
  // rendering area in the |rendering_helper|.
  // Doesn't take ownership of |rendering_helper| or |note|, which must outlive
  // |*this|.
  // |num_play_throughs| indicates how many times to play through the video.
  // |reset_after_frame_num| can be a frame number >=0 indicating a mid-stream
  // Reset() should be done after that frame number is delivered, or
  // END_OF_STREAM_RESET to indicate no mid-stream Reset().
  // |delete_decoder_state| indicates when the underlying decoder should be
  // Destroy()'d and deleted and can take values: N<0: delete after -N Decode()
  // calls have been made, N>=0 means interpret as ClientState.
  // Both |reset_after_frame_num| & |delete_decoder_state| apply only to the
  // last play-through (governed by |num_play_throughs|).
  // |suppress_rendering| indicates GL rendering is supressed or not.
  // After |delay_reuse_after_frame_num| frame has been delivered, the client
  // will start delaying the call to ReusePictureBuffer() for kReuseDelay.
  // |decode_calls_per_second| is the number of VDA::Decode calls per second.
  // If |decode_calls_per_second| > 0, |num_in_flight_decodes| must be 1.
  GLRenderingVDAClient(size_t window_id,
                       RenderingHelper* rendering_helper,
                       ClientStateNotification<ClientState>* note,
                       const std::string& encoded_data,
                       int num_in_flight_decodes,
                       int num_play_throughs,
                       int reset_after_frame_num,
                       int delete_decoder_state,
                       int frame_width,
                       int frame_height,
                       VideoCodecProfile profile,
                       int fake_decoder,
                       bool suppress_rendering,
                       int delay_reuse_after_frame_num,
                       int decode_calls_per_second,
                       bool render_as_thumbnails);
  ~GLRenderingVDAClient() override;
  void CreateAndStartDecoder();

  // VideoDecodeAccelerator::Client implementation.
  // The heart of the Client.
  void ProvidePictureBuffers(uint32_t requested_num_of_buffers,
                             VideoPixelFormat format,
                             uint32_t textures_per_buffer,
                             const gfx::Size& dimensions,
                             uint32_t texture_target) override;
  void DismissPictureBuffer(int32_t picture_buffer_id) override;
  void PictureReady(const Picture& picture) override;
  // Simple state changes.
  void NotifyEndOfBitstreamBuffer(int32_t bitstream_buffer_id) override;
  void NotifyFlushDone() override;
  void NotifyResetDone() override;
  void NotifyError(VideoDecodeAccelerator::Error error) override;

  void OutputFrameDeliveryTimes(base::File* output);

  // Simple getters for inspecting the state of the Client.
  int num_done_bitstream_buffers() { return num_done_bitstream_buffers_; }
  int num_skipped_fragments() { return num_skipped_fragments_; }
  int num_queued_fragments() { return num_queued_fragments_; }
  int num_decoded_frames() { return num_decoded_frames_; }
  double frames_per_second();
  // Return the median of the decode time of all decoded frames.
  base::TimeDelta decode_time_median();
  bool decoder_deleted() { return !decoder_.get(); }

 private:
  typedef std::map<int32_t, scoped_refptr<TextureRef>> TextureRefMap;

  void SetState(ClientState new_state);
  void FinishInitialization();
  void ReturnPicture(int32_t picture_buffer_id);

  // Delete the associated decoder helper.
  void DeleteDecoder();

  // Compute & return the first encoded bytes (including a start frame) to send
  // to the decoder, starting at |start_pos| and returning one fragment. Skips
  // to the first decodable position.
  std::string GetBytesForFirstFragment(size_t start_pos, size_t* end_pos);
  // Compute & return the encoded bytes of next fragment to send to the decoder
  // (based on |start_pos|).
  std::string GetBytesForNextFragment(size_t start_pos, size_t* end_pos);
  // Helpers for GetBytesForNextFragment above.
  void GetBytesForNextNALU(size_t start_pos, size_t* end_pos);  // For h.264.
  std::string GetBytesForNextFrame(size_t start_pos,
                                   size_t* end_pos);  // For VP8/9.

  // Request decode of the next fragment in the encoded data.
  void DecodeNextFragment();

  size_t window_id_;
  RenderingHelper* rendering_helper_;
  gfx::Size frame_size_;
  std::string encoded_data_;
  const int num_in_flight_decodes_;
  int outstanding_decodes_;
  size_t encoded_data_next_pos_to_decode_;
  int next_bitstream_buffer_id_;
  ClientStateNotification<ClientState>* note_;
  std::unique_ptr<VideoDecodeAccelerator> decoder_;
  base::WeakPtr<VideoDecodeAccelerator> weak_vda_;
  std::unique_ptr<base::WeakPtrFactory<VideoDecodeAccelerator>>
      weak_vda_ptr_factory_;
  std::unique_ptr<GpuVideoDecodeAcceleratorFactoryImpl> vda_factory_;
  int remaining_play_throughs_;
  int reset_after_frame_num_;
  int delete_decoder_state_;
  ClientState state_;
  int num_skipped_fragments_;
  int num_queued_fragments_;
  int num_decoded_frames_;
  int num_done_bitstream_buffers_;
  base::TimeTicks initialize_done_ticks_;
  VideoCodecProfile profile_;
  int fake_decoder_;
  GLenum texture_target_;
  VideoPixelFormat pixel_format_;
  bool suppress_rendering_;
  std::vector<base::TimeTicks> frame_delivery_times_;
  int delay_reuse_after_frame_num_;
  // A map from bitstream buffer id to the decode start time of the buffer.
  std::map<int, base::TimeTicks> decode_start_time_;
  // The decode time of all decoded frames.
  std::vector<base::TimeDelta> decode_time_;
  // The number of VDA::Decode calls per second. This is to simulate webrtc.
  int decode_calls_per_second_;
  bool render_as_thumbnails_;

  // A map of the textures that are currently active for the decoder, i.e.,
  // have been created via AssignPictureBuffers() and not dismissed via
  // DismissPictureBuffer(). The keys in the map are the IDs of the
  // corresponding picture buffers, and the values are TextureRefs to the
  // textures.
  TextureRefMap active_textures_;

  // A map of the textures that are still pending in the renderer.
  // We check this to ensure all frames are rendered before entering the
  // CS_RESET_State.
  TextureRefMap pending_textures_;

  int32_t next_picture_buffer_id_;

  base::WeakPtr<GLRenderingVDAClient> weak_this_;
  base::WeakPtrFactory<GLRenderingVDAClient> weak_this_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(GLRenderingVDAClient);
};

static bool DoNothingReturnTrue() {
  return true;
}

static bool DummyBindImage(uint32_t client_texture_id,
                           uint32_t texture_target,
                           const scoped_refptr<gl::GLImage>& image,
                           bool can_bind_to_sampler) {
  return true;
}

GLRenderingVDAClient::GLRenderingVDAClient(
    size_t window_id,
    RenderingHelper* rendering_helper,
    ClientStateNotification<ClientState>* note,
    const std::string& encoded_data,
    int num_in_flight_decodes,
    int num_play_throughs,
    int reset_after_frame_num,
    int delete_decoder_state,
    int frame_width,
    int frame_height,
    VideoCodecProfile profile,
    int fake_decoder,
    bool suppress_rendering,
    int delay_reuse_after_frame_num,
    int decode_calls_per_second,
    bool render_as_thumbnails)
    : window_id_(window_id),
      rendering_helper_(rendering_helper),
      frame_size_(frame_width, frame_height),
      encoded_data_(encoded_data),
      num_in_flight_decodes_(num_in_flight_decodes),
      outstanding_decodes_(0),
      encoded_data_next_pos_to_decode_(0),
      next_bitstream_buffer_id_(0),
      note_(note),
      remaining_play_throughs_(num_play_throughs),
      reset_after_frame_num_(reset_after_frame_num),
      delete_decoder_state_(delete_decoder_state),
      state_(CS_CREATED),
      num_skipped_fragments_(0),
      num_queued_fragments_(0),
      num_decoded_frames_(0),
      num_done_bitstream_buffers_(0),
      fake_decoder_(fake_decoder),
      texture_target_(0),
      pixel_format_(PIXEL_FORMAT_UNKNOWN),
      suppress_rendering_(suppress_rendering),
      delay_reuse_after_frame_num_(delay_reuse_after_frame_num),
      decode_calls_per_second_(decode_calls_per_second),
      render_as_thumbnails_(render_as_thumbnails),
      next_picture_buffer_id_(1),
      weak_this_factory_(this) {
  LOG_ASSERT(num_in_flight_decodes > 0);
  LOG_ASSERT(num_play_throughs > 0);
  // |num_in_flight_decodes_| is unsupported if |decode_calls_per_second_| > 0.
  if (decode_calls_per_second_ > 0)
    LOG_ASSERT(1 == num_in_flight_decodes_);

  // Default to H264 baseline if no profile provided.
  profile_ =
      (profile != VIDEO_CODEC_PROFILE_UNKNOWN ? profile : H264PROFILE_BASELINE);

  weak_this_ = weak_this_factory_.GetWeakPtr();
}

GLRenderingVDAClient::~GLRenderingVDAClient() {
  DeleteDecoder();  // Clean up in case of expected error.
  LOG_ASSERT(decoder_deleted());
  SetState(CS_DESTROYED);
}

void GLRenderingVDAClient::CreateAndStartDecoder() {
  LOG_ASSERT(decoder_deleted());
  LOG_ASSERT(!decoder_.get());

  if (fake_decoder_) {
    decoder_.reset(new FakeVideoDecodeAccelerator(
        frame_size_, base::Bind(&DoNothingReturnTrue)));
    LOG_ASSERT(decoder_->Initialize(profile_, this));
  } else {
    if (!vda_factory_) {
      vda_factory_ = GpuVideoDecodeAcceleratorFactoryImpl::Create(
          base::Bind(&RenderingHelper::GetGLContext,
                     base::Unretained(rendering_helper_)),
          base::Bind(&DoNothingReturnTrue), base::Bind(&DummyBindImage));
      LOG_ASSERT(vda_factory_);
    }

    VideoDecodeAccelerator::Config config(profile_);
    if (g_test_import) {
      config.output_mode = VideoDecodeAccelerator::Config::OutputMode::IMPORT;
    }
    gpu::GpuDriverBugWorkarounds workarounds;
    gpu::GpuPreferences gpu_preferences;
    decoder_ =
        vda_factory_->CreateVDA(this, config, workarounds, gpu_preferences);
  }

  LOG_ASSERT(decoder_) << "Failed creating a VDA";

  decoder_->TryToSetupDecodeOnSeparateThread(
      weak_this_, base::ThreadTaskRunnerHandle::Get());

  weak_vda_ptr_factory_.reset(
      new base::WeakPtrFactory<VideoDecodeAccelerator>(decoder_.get()));
  weak_vda_ = weak_vda_ptr_factory_->GetWeakPtr();

  SetState(CS_DECODER_SET);
  FinishInitialization();
}

void GLRenderingVDAClient::ProvidePictureBuffers(
    uint32_t requested_num_of_buffers,
    VideoPixelFormat pixel_format,
    uint32_t textures_per_buffer,
    const gfx::Size& dimensions,
    uint32_t texture_target) {
  if (decoder_deleted())
    return;
  LOG_ASSERT(textures_per_buffer == 1u);
  std::vector<PictureBuffer> buffers;

  requested_num_of_buffers += kExtraPictureBuffers;
  if (pixel_format == PIXEL_FORMAT_UNKNOWN)
    pixel_format = PIXEL_FORMAT_ARGB;

  LOG_ASSERT((pixel_format_ == PIXEL_FORMAT_UNKNOWN) ||
             (pixel_format_ == pixel_format));
  pixel_format_ = pixel_format;

  texture_target_ = texture_target;
  for (uint32_t i = 0; i < requested_num_of_buffers; ++i) {
    uint32_t texture_id;
    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    rendering_helper_->CreateTexture(texture_target_, &texture_id, dimensions,
                                     &done);
    done.Wait();

    scoped_refptr<TextureRef> texture_ref;
    base::Closure delete_texture_cb =
        base::Bind(&RenderingHelper::DeleteTexture,
                   base::Unretained(rendering_helper_), texture_id);

    if (g_test_import) {
      texture_ref = TextureRef::CreatePreallocated(
          texture_id, delete_texture_cb, pixel_format, dimensions);
    } else {
      texture_ref = TextureRef::Create(texture_id, delete_texture_cb);
    }

    LOG_ASSERT(texture_ref);

    int32_t picture_buffer_id = next_picture_buffer_id_++;
    LOG_ASSERT(
        active_textures_.insert(std::make_pair(picture_buffer_id, texture_ref))
            .second);

    PictureBuffer::TextureIds ids;
    ids.push_back(texture_id);
    buffers.push_back(PictureBuffer(picture_buffer_id, dimensions, ids));
  }
  decoder_->AssignPictureBuffers(buffers);

  if (g_test_import) {
    for (const auto& buffer : buffers) {
      TextureRefMap::iterator texture_it = active_textures_.find(buffer.id());
      ASSERT_NE(active_textures_.end(), texture_it);

      const gfx::GpuMemoryBufferHandle& handle =
          texture_it->second->ExportGpuMemoryBufferHandle();
      LOG_ASSERT(!handle.is_null()) << "Failed producing GMB handle";
      decoder_->ImportBufferForPicture(buffer.id(), handle);
    }
  }
}

void GLRenderingVDAClient::DismissPictureBuffer(int32_t picture_buffer_id) {
  LOG_ASSERT(1U == active_textures_.erase(picture_buffer_id));
}

void GLRenderingVDAClient::PictureReady(const Picture& picture) {
  // We shouldn't be getting pictures delivered after Reset has completed.
  LOG_ASSERT(state_ < CS_RESET);

  if (decoder_deleted())
    return;

  base::TimeTicks now = base::TimeTicks::Now();

  frame_delivery_times_.push_back(now);

  // Save the decode time of this picture.
  std::map<int, base::TimeTicks>::iterator it =
      decode_start_time_.find(picture.bitstream_buffer_id());
  ASSERT_NE(decode_start_time_.end(), it);
  decode_time_.push_back(now - it->second);
  decode_start_time_.erase(it);

  LOG_ASSERT(picture.bitstream_buffer_id() <= next_bitstream_buffer_id_);
  ++num_decoded_frames_;

  // Mid-stream reset applies only to the last play-through per constructor
  // comment.
  if (remaining_play_throughs_ == 1 &&
      reset_after_frame_num_ == num_decoded_frames_) {
    reset_after_frame_num_ = MID_STREAM_RESET;
    decoder_->Reset();
    // Re-start decoding from the beginning of the stream to avoid needing to
    // know how to find I-frames and so on in this test.
    encoded_data_next_pos_to_decode_ = 0;
  }

  TextureRefMap::iterator texture_it =
      active_textures_.find(picture.picture_buffer_id());
  ASSERT_NE(active_textures_.end(), texture_it);

  scoped_refptr<VideoFrameTexture> video_frame = new VideoFrameTexture(
      texture_target_, texture_it->second->texture_id(),
      base::Bind(&GLRenderingVDAClient::ReturnPicture, AsWeakPtr(),
                 picture.picture_buffer_id()));
  ASSERT_TRUE(pending_textures_.insert(*texture_it).second);

  if (render_as_thumbnails_) {
    rendering_helper_->RenderThumbnail(video_frame->texture_target(),
                                       video_frame->texture_id());
  } else if (!suppress_rendering_) {
    rendering_helper_->QueueVideoFrame(window_id_, video_frame);
  }
}

void GLRenderingVDAClient::ReturnPicture(int32_t picture_buffer_id) {
  if (decoder_deleted())
    return;
  LOG_ASSERT(1U == pending_textures_.erase(picture_buffer_id));

  if (pending_textures_.empty() && state_ == CS_RESETTING) {
    SetState(CS_RESET);
    DeleteDecoder();
    return;
  }

  if (num_decoded_frames_ > delay_reuse_after_frame_num_) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&VideoDecodeAccelerator::ReusePictureBuffer,
                              weak_vda_, picture_buffer_id),
        kReuseDelay);
  } else {
    decoder_->ReusePictureBuffer(picture_buffer_id);
  }
}

void GLRenderingVDAClient::NotifyEndOfBitstreamBuffer(
    int32_t bitstream_buffer_id) {
  // TODO(fischman): this test currently relies on this notification to make
  // forward progress during a Reset().  But the VDA::Reset() API doesn't
  // guarantee this, so stop relying on it (and remove the notifications from
  // VaapiVideoDecodeAccelerator::FinishReset()).
  ++num_done_bitstream_buffers_;
  --outstanding_decodes_;

  // Flush decoder after all BitstreamBuffers are processed.
  if (encoded_data_next_pos_to_decode_ == encoded_data_.size()) {
    // TODO(owenlin): We should not have to check the number of
    // |outstanding_decodes_|. |decoder_| should be able to accept Flush()
    // before it's done with outstanding decodes. (crbug.com/528183)
    if (outstanding_decodes_ == 0) {
      decoder_->Flush();
      SetState(CS_FLUSHING);
    }
  } else if (decode_calls_per_second_ == 0) {
    DecodeNextFragment();
  }
}

void GLRenderingVDAClient::NotifyFlushDone() {
  if (decoder_deleted())
    return;

  SetState(CS_FLUSHED);
  --remaining_play_throughs_;
  DCHECK_GE(remaining_play_throughs_, 0);
  if (decoder_deleted())
    return;
  decoder_->Reset();
  SetState(CS_RESETTING);
}

void GLRenderingVDAClient::NotifyResetDone() {
  if (decoder_deleted())
    return;

  if (reset_after_frame_num_ == MID_STREAM_RESET) {
    reset_after_frame_num_ = END_OF_STREAM_RESET;
    DecodeNextFragment();
    return;
  } else if (reset_after_frame_num_ == START_OF_STREAM_RESET) {
    reset_after_frame_num_ = END_OF_STREAM_RESET;
    for (int i = 0; i < num_in_flight_decodes_; ++i)
      DecodeNextFragment();
    return;
  }

  if (remaining_play_throughs_) {
    encoded_data_next_pos_to_decode_ = 0;
    FinishInitialization();
    return;
  }

  rendering_helper_->Flush(window_id_);

  if (pending_textures_.empty()) {
    SetState(CS_RESET);
    DeleteDecoder();
  }
}

void GLRenderingVDAClient::NotifyError(VideoDecodeAccelerator::Error error) {
  SetState(CS_ERROR);
}

void GLRenderingVDAClient::OutputFrameDeliveryTimes(base::File* output) {
  std::string s = base::StringPrintf("frame count: %" PRIuS "\n",
                                     frame_delivery_times_.size());
  output->WriteAtCurrentPos(s.data(), s.length());
  base::TimeTicks t0 = initialize_done_ticks_;
  for (size_t i = 0; i < frame_delivery_times_.size(); ++i) {
    s = base::StringPrintf("frame %04" PRIuS ": %" PRId64 " us\n", i,
                           (frame_delivery_times_[i] - t0).InMicroseconds());
    t0 = frame_delivery_times_[i];
    output->WriteAtCurrentPos(s.data(), s.length());
  }
}

static bool LookingAtNAL(const std::string& encoded, size_t pos) {
  return encoded[pos] == 0 && encoded[pos + 1] == 0 && encoded[pos + 2] == 0 &&
         encoded[pos + 3] == 1;
}

void GLRenderingVDAClient::SetState(ClientState new_state) {
  note_->Notify(new_state);
  state_ = new_state;
  if (!remaining_play_throughs_ && new_state == delete_decoder_state_) {
    LOG_ASSERT(!decoder_deleted());
    DeleteDecoder();
  }
}

void GLRenderingVDAClient::FinishInitialization() {
  SetState(CS_INITIALIZED);
  initialize_done_ticks_ = base::TimeTicks::Now();

  if (reset_after_frame_num_ == START_OF_STREAM_RESET) {
    reset_after_frame_num_ = MID_STREAM_RESET;
    decoder_->Reset();
    return;
  }

  for (int i = 0; i < num_in_flight_decodes_; ++i)
    DecodeNextFragment();
  DCHECK_EQ(outstanding_decodes_, num_in_flight_decodes_);
}

void GLRenderingVDAClient::DeleteDecoder() {
  if (decoder_deleted())
    return;
  weak_vda_ptr_factory_->InvalidateWeakPtrs();
  decoder_.reset();
  STLClearObject(&encoded_data_);
  active_textures_.clear();

  // Cascade through the rest of the states to simplify test code below.
  for (int i = state_ + 1; i < CS_MAX; ++i)
    SetState(static_cast<ClientState>(i));
}

std::string GLRenderingVDAClient::GetBytesForFirstFragment(size_t start_pos,
                                                           size_t* end_pos) {
  if (profile_ < H264PROFILE_MAX) {
    *end_pos = start_pos;
    while (*end_pos + 4 < encoded_data_.size()) {
      if ((encoded_data_[*end_pos + 4] & 0x1f) == 0x7)  // SPS start frame
        return GetBytesForNextFragment(*end_pos, end_pos);
      GetBytesForNextNALU(*end_pos, end_pos);
      num_skipped_fragments_++;
    }
    *end_pos = start_pos;
    return std::string();
  }
  DCHECK_LE(profile_, VP9PROFILE_MAX);
  return GetBytesForNextFragment(start_pos, end_pos);
}

std::string GLRenderingVDAClient::GetBytesForNextFragment(size_t start_pos,
                                                          size_t* end_pos) {
  if (profile_ < H264PROFILE_MAX) {
    *end_pos = start_pos;
    GetBytesForNextNALU(*end_pos, end_pos);
    if (start_pos != *end_pos) {
      num_queued_fragments_++;
    }
    return encoded_data_.substr(start_pos, *end_pos - start_pos);
  }
  DCHECK_LE(profile_, VP9PROFILE_MAX);
  return GetBytesForNextFrame(start_pos, end_pos);
}

void GLRenderingVDAClient::GetBytesForNextNALU(size_t start_pos,
                                               size_t* end_pos) {
  *end_pos = start_pos;
  if (*end_pos + 4 > encoded_data_.size())
    return;
  LOG_ASSERT(LookingAtNAL(encoded_data_, start_pos));
  *end_pos += 4;
  while (*end_pos + 4 <= encoded_data_.size() &&
         !LookingAtNAL(encoded_data_, *end_pos)) {
    ++*end_pos;
  }
  if (*end_pos + 3 >= encoded_data_.size())
    *end_pos = encoded_data_.size();
}

std::string GLRenderingVDAClient::GetBytesForNextFrame(size_t start_pos,
                                                       size_t* end_pos) {
  // Helpful description: http://wiki.multimedia.cx/index.php?title=IVF
  std::string bytes;
  if (start_pos == 0)
    start_pos = 32;  // Skip IVF header.
  *end_pos = start_pos;
  uint32_t frame_size = *reinterpret_cast<uint32_t*>(&encoded_data_[*end_pos]);
  *end_pos += 12;  // Skip frame header.
  bytes.append(encoded_data_.substr(*end_pos, frame_size));
  *end_pos += frame_size;
  num_queued_fragments_++;
  return bytes;
}

static bool FragmentHasConfigInfo(const uint8_t* data,
                                  size_t size,
                                  VideoCodecProfile profile) {
  if (profile >= H264PROFILE_MIN && profile <= H264PROFILE_MAX) {
    H264Parser parser;
    parser.SetStream(data, size);
    H264NALU nalu;
    H264Parser::Result result = parser.AdvanceToNextNALU(&nalu);
    if (result != H264Parser::kOk) {
      // Let the VDA figure out there's something wrong with the stream.
      return false;
    }

    return nalu.nal_unit_type == H264NALU::kSPS;
  } else if (profile >= VP8PROFILE_MIN && profile <= VP9PROFILE_MAX) {
    return (size > 0 && !(data[0] & 0x01));
  }
  // Shouldn't happen at this point.
  LOG(FATAL) << "Invalid profile: " << profile;
  return false;
}

void GLRenderingVDAClient::DecodeNextFragment() {
  if (decoder_deleted())
    return;
  if (encoded_data_next_pos_to_decode_ == encoded_data_.size())
    return;
  size_t end_pos;
  std::string next_fragment_bytes;
  if (encoded_data_next_pos_to_decode_ == 0) {
    next_fragment_bytes = GetBytesForFirstFragment(0, &end_pos);
  } else {
    next_fragment_bytes =
        GetBytesForNextFragment(encoded_data_next_pos_to_decode_, &end_pos);
  }
  size_t next_fragment_size = next_fragment_bytes.size();

  // Call Reset() just after Decode() if the fragment contains config info.
  // This tests how the VDA behaves when it gets a reset request before it has
  // a chance to ProvidePictureBuffers().
  bool reset_here = false;
  if (reset_after_frame_num_ == RESET_AFTER_FIRST_CONFIG_INFO) {
    reset_here = FragmentHasConfigInfo(
        reinterpret_cast<const uint8_t*>(next_fragment_bytes.data()),
        next_fragment_size, profile_);
    if (reset_here)
      reset_after_frame_num_ = END_OF_STREAM_RESET;
  }

  // Populate the shared memory buffer w/ the fragment, duplicate its handle,
  // and hand it off to the decoder.
  base::SharedMemory shm;
  LOG_ASSERT(shm.CreateAndMapAnonymous(next_fragment_size));
  memcpy(shm.memory(), next_fragment_bytes.data(), next_fragment_size);
  base::SharedMemoryHandle dup_handle;
  bool result =
      shm.ShareToProcess(base::GetCurrentProcessHandle(), &dup_handle);
  LOG_ASSERT(result);
  BitstreamBuffer bitstream_buffer(next_bitstream_buffer_id_, dup_handle,
                                   next_fragment_size);
  decode_start_time_[next_bitstream_buffer_id_] = base::TimeTicks::Now();
  // Mask against 30 bits, to avoid (undefined) wraparound on signed integer.
  next_bitstream_buffer_id_ = (next_bitstream_buffer_id_ + 1) & 0x3FFFFFFF;
  decoder_->Decode(bitstream_buffer);
  ++outstanding_decodes_;
  if (!remaining_play_throughs_ &&
      -delete_decoder_state_ == next_bitstream_buffer_id_) {
    DeleteDecoder();
  }

  if (reset_here) {
    reset_after_frame_num_ = MID_STREAM_RESET;
    decoder_->Reset();
    // Restart from the beginning to re-Decode() the SPS we just sent.
    encoded_data_next_pos_to_decode_ = 0;
  } else {
    encoded_data_next_pos_to_decode_ = end_pos;
  }

  if (decode_calls_per_second_ > 0) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&GLRenderingVDAClient::DecodeNextFragment, AsWeakPtr()),
        base::TimeDelta::FromSeconds(1) / decode_calls_per_second_);
  }
}

double GLRenderingVDAClient::frames_per_second() {
  base::TimeDelta delta = frame_delivery_times_.back() - initialize_done_ticks_;
  return num_decoded_frames_ / delta.InSecondsF();
}

base::TimeDelta GLRenderingVDAClient::decode_time_median() {
  if (decode_time_.size() == 0)
    return base::TimeDelta();
  std::sort(decode_time_.begin(), decode_time_.end());
  int index = decode_time_.size() / 2;
  if (decode_time_.size() % 2 != 0)
    return decode_time_[index];

  return (decode_time_[index] + decode_time_[index - 1]) / 2;
}

class VideoDecodeAcceleratorTest : public ::testing::Test {
 protected:
  VideoDecodeAcceleratorTest();
  void SetUp() override;
  void TearDown() override;

  // Parse |data| into its constituent parts, set the various output fields
  // accordingly, and read in video stream. CHECK-fails on unexpected or
  // missing required data. Unspecified optional fields are set to -1.
  void ParseAndReadTestVideoData(base::FilePath::StringType data,
                                 std::vector<TestVideoFile*>* test_video_files);

  // Update the parameters of |test_video_files| according to
  // |num_concurrent_decoders| and |reset_point|. Ex: the expected number of
  // frames should be adjusted if decoder is reset in the middle of the stream.
  void UpdateTestVideoFileParams(size_t num_concurrent_decoders,
                                 int reset_point,
                                 std::vector<TestVideoFile*>* test_video_files);

  void InitializeRenderingHelper(const RenderingHelperParams& helper_params);
  void CreateAndStartDecoder(GLRenderingVDAClient* client,
                             ClientStateNotification<ClientState>* note);
  void WaitUntilDecodeFinish(ClientStateNotification<ClientState>* note);
  void WaitUntilIdle();
  void OutputLogFile(const base::FilePath::CharType* log_path,
                     const std::string& content);

  std::vector<TestVideoFile*> test_video_files_;
  RenderingHelper rendering_helper_;

 private:
  // Required for Thread to work.  Not used otherwise.
  base::ShadowingAtExitManager at_exit_manager_;

  DISALLOW_COPY_AND_ASSIGN(VideoDecodeAcceleratorTest);
};

VideoDecodeAcceleratorTest::VideoDecodeAcceleratorTest() {}

void VideoDecodeAcceleratorTest::SetUp() {
  ParseAndReadTestVideoData(g_test_video_data, &test_video_files_);
}

void VideoDecodeAcceleratorTest::TearDown() {
  g_env->GetRenderingTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&STLDeleteElements<std::vector<TestVideoFile*>>,
                            &test_video_files_));

  base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  g_env->GetRenderingTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&RenderingHelper::UnInitialize,
                            base::Unretained(&rendering_helper_), &done));
  done.Wait();

  rendering_helper_.TearDown();
}

void VideoDecodeAcceleratorTest::ParseAndReadTestVideoData(
    base::FilePath::StringType data,
    std::vector<TestVideoFile*>* test_video_files) {
  std::vector<base::FilePath::StringType> entries =
      base::SplitString(data, base::FilePath::StringType(1, ';'),
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  LOG_ASSERT(entries.size() >= 1U) << data;
  for (size_t index = 0; index < entries.size(); ++index) {
    std::vector<base::FilePath::StringType> fields =
        base::SplitString(entries[index], base::FilePath::StringType(1, ':'),
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    LOG_ASSERT(fields.size() >= 1U) << entries[index];
    LOG_ASSERT(fields.size() <= 8U) << entries[index];
    TestVideoFile* video_file = new TestVideoFile(fields[0]);
    if (!fields[1].empty())
      LOG_ASSERT(base::StringToInt(fields[1], &video_file->width));
    if (!fields[2].empty())
      LOG_ASSERT(base::StringToInt(fields[2], &video_file->height));
    if (!fields[3].empty())
      LOG_ASSERT(base::StringToInt(fields[3], &video_file->num_frames));
    if (!fields[4].empty())
      LOG_ASSERT(base::StringToInt(fields[4], &video_file->num_fragments));
    if (!fields[5].empty())
      LOG_ASSERT(base::StringToInt(fields[5], &video_file->min_fps_render));
    if (!fields[6].empty())
      LOG_ASSERT(base::StringToInt(fields[6], &video_file->min_fps_no_render));
    int profile = -1;
    if (!fields[7].empty())
      LOG_ASSERT(base::StringToInt(fields[7], &profile));
    video_file->profile = static_cast<VideoCodecProfile>(profile);

    // Read in the video data.
    base::FilePath filepath(video_file->file_name);
    LOG_ASSERT(base::ReadFileToString(filepath, &video_file->data_str))
        << "test_video_file: " << filepath.MaybeAsASCII();

    test_video_files->push_back(video_file);
  }
}

void VideoDecodeAcceleratorTest::UpdateTestVideoFileParams(
    size_t num_concurrent_decoders,
    int reset_point,
    std::vector<TestVideoFile*>* test_video_files) {
  for (size_t i = 0; i < test_video_files->size(); i++) {
    TestVideoFile* video_file = (*test_video_files)[i];
    if (reset_point == MID_STREAM_RESET) {
      // Reset should not go beyond the last frame;
      // reset in the middle of the stream for short videos.
      video_file->reset_after_frame_num = kMaxResetAfterFrameNum;
      if (video_file->num_frames <= video_file->reset_after_frame_num)
        video_file->reset_after_frame_num = video_file->num_frames / 2;

      video_file->num_frames += video_file->reset_after_frame_num;
    } else {
      video_file->reset_after_frame_num = reset_point;
    }

    if (video_file->min_fps_render != -1)
      video_file->min_fps_render /= num_concurrent_decoders;
    if (video_file->min_fps_no_render != -1)
      video_file->min_fps_no_render /= num_concurrent_decoders;
  }
}

void VideoDecodeAcceleratorTest::InitializeRenderingHelper(
    const RenderingHelperParams& helper_params) {
  rendering_helper_.Setup();

  base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  g_env->GetRenderingTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&RenderingHelper::Initialize,
                 base::Unretained(&rendering_helper_), helper_params, &done));
  done.Wait();
}

void VideoDecodeAcceleratorTest::CreateAndStartDecoder(
    GLRenderingVDAClient* client,
    ClientStateNotification<ClientState>* note) {
  g_env->GetRenderingTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&GLRenderingVDAClient::CreateAndStartDecoder,
                            base::Unretained(client)));
  ASSERT_EQ(note->Wait(), CS_DECODER_SET);
}

void VideoDecodeAcceleratorTest::WaitUntilDecodeFinish(
    ClientStateNotification<ClientState>* note) {
  for (int i = 0; i < CS_MAX; i++) {
    if (note->Wait() == CS_DESTROYED)
      break;
  }
}

void VideoDecodeAcceleratorTest::WaitUntilIdle() {
  base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  g_env->GetRenderingTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&base::WaitableEvent::Signal, base::Unretained(&done)));
  done.Wait();
}

void VideoDecodeAcceleratorTest::OutputLogFile(
    const base::FilePath::CharType* log_path,
    const std::string& content) {
  base::File file(base::FilePath(log_path),
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  file.WriteAtCurrentPos(content.data(), content.length());
}

// Test parameters:
// - Number of concurrent decoders. The value takes effect when there is only
//   one input stream; otherwise, one decoder per input stream will be
//   instantiated.
// - Number of concurrent in-flight Decode() calls per decoder.
// - Number of play-throughs.
// - reset_after_frame_num: see GLRenderingVDAClient ctor.
// - delete_decoder_phase: see GLRenderingVDAClient ctor.
// - whether to test slow rendering by delaying ReusePictureBuffer().
// - whether the video frames are rendered as thumbnails.
class VideoDecodeAcceleratorParamTest
    : public VideoDecodeAcceleratorTest,
      public ::testing::WithParamInterface<
          std::tuple<int, int, int, ResetPoint, ClientState, bool, bool>> {};

// Wait for |note| to report a state and if it's not |expected_state| then
// assert |client| has deleted its decoder.
static void AssertWaitForStateOrDeleted(
    ClientStateNotification<ClientState>* note,
    GLRenderingVDAClient* client,
    ClientState expected_state) {
  ClientState state = note->Wait();
  if (state == expected_state)
    return;
  ASSERT_TRUE(client->decoder_deleted())
      << "Decoder not deleted but Wait() returned " << state
      << ", instead of " << expected_state;
}

// We assert a minimal number of concurrent decoders we expect to succeed.
// Different platforms can support more concurrent decoders, so we don't assert
// failure above this.
enum { kMinSupportedNumConcurrentDecoders = 3 };

// Test the most straightforward case possible: data is decoded from a single
// chunk and rendered to the screen.
TEST_P(VideoDecodeAcceleratorParamTest, TestSimpleDecode) {
  size_t num_concurrent_decoders = std::get<0>(GetParam());
  const size_t num_in_flight_decodes = std::get<1>(GetParam());
  int num_play_throughs = std::get<2>(GetParam());
  const int reset_point = std::get<3>(GetParam());
  const int delete_decoder_state = std::get<4>(GetParam());
  bool test_reuse_delay = std::get<5>(GetParam());
  const bool render_as_thumbnails = std::get<6>(GetParam());

  if (test_video_files_.size() > 1)
    num_concurrent_decoders = test_video_files_.size();

  if (g_num_play_throughs > 0)
    num_play_throughs = g_num_play_throughs;

  UpdateTestVideoFileParams(num_concurrent_decoders, reset_point,
                            &test_video_files_);

  // Suppress GL rendering for all tests when the "--rendering_fps" is 0.
  const bool suppress_rendering = g_rendering_fps == 0;

  std::vector<ClientStateNotification<ClientState>*> notes(
      num_concurrent_decoders, NULL);
  std::vector<GLRenderingVDAClient*> clients(num_concurrent_decoders, NULL);

  RenderingHelperParams helper_params;
  helper_params.rendering_fps = g_rendering_fps;
  helper_params.warm_up_iterations = g_rendering_warm_up;
  helper_params.render_as_thumbnails = render_as_thumbnails;
  if (render_as_thumbnails) {
    // Only one decoder is supported with thumbnail rendering
    LOG_ASSERT(num_concurrent_decoders == 1U);
    helper_params.thumbnails_page_size = kThumbnailsPageSize;
    helper_params.thumbnail_size = kThumbnailSize;
  }

  // First kick off all the decoders.
  for (size_t index = 0; index < num_concurrent_decoders; ++index) {
    TestVideoFile* video_file =
        test_video_files_[index % test_video_files_.size()];
    ClientStateNotification<ClientState>* note =
        new ClientStateNotification<ClientState>();
    notes[index] = note;

    int delay_after_frame_num = std::numeric_limits<int>::max();
    if (test_reuse_delay &&
        kMaxFramesToDelayReuse * 2 < video_file->num_frames) {
      delay_after_frame_num = video_file->num_frames - kMaxFramesToDelayReuse;
    }

    GLRenderingVDAClient* client =
        new GLRenderingVDAClient(index,
                                 &rendering_helper_,
                                 note,
                                 video_file->data_str,
                                 num_in_flight_decodes,
                                 num_play_throughs,
                                 video_file->reset_after_frame_num,
                                 delete_decoder_state,
                                 video_file->width,
                                 video_file->height,
                                 video_file->profile,
                                 g_fake_decoder,
                                 suppress_rendering,
                                 delay_after_frame_num,
                                 0,
                                 render_as_thumbnails);

    clients[index] = client;
    helper_params.window_sizes.push_back(
        render_as_thumbnails
            ? kThumbnailsPageSize
            : gfx::Size(video_file->width, video_file->height));
  }

  InitializeRenderingHelper(helper_params);

  for (size_t index = 0; index < num_concurrent_decoders; ++index) {
    CreateAndStartDecoder(clients[index], notes[index]);
  }

  // Then wait for all the decodes to finish.
  // Only check performance & correctness later if we play through only once.
  bool skip_performance_and_correctness_checks = num_play_throughs > 1;
  for (size_t i = 0; i < num_concurrent_decoders; ++i) {
    ClientStateNotification<ClientState>* note = notes[i];
    ClientState state = note->Wait();
    if (state != CS_INITIALIZED) {
      skip_performance_and_correctness_checks = true;
      // We expect initialization to fail only when more than the supported
      // number of decoders is instantiated.  Assert here that something else
      // didn't trigger failure.
      ASSERT_GT(num_concurrent_decoders,
                static_cast<size_t>(kMinSupportedNumConcurrentDecoders));
      continue;
    }
    ASSERT_EQ(state, CS_INITIALIZED);
    for (int n = 0; n < num_play_throughs; ++n) {
      // For play-throughs other than the first, we expect initialization to
      // succeed unconditionally.
      if (n > 0) {
        ASSERT_NO_FATAL_FAILURE(
            AssertWaitForStateOrDeleted(note, clients[i], CS_INITIALIZED));
      }
      // InitializeDone kicks off decoding inside the client, so we just need to
      // wait for Flush.
      ASSERT_NO_FATAL_FAILURE(
          AssertWaitForStateOrDeleted(note, clients[i], CS_FLUSHING));
      ASSERT_NO_FATAL_FAILURE(
          AssertWaitForStateOrDeleted(note, clients[i], CS_FLUSHED));
      // FlushDone requests Reset().
      ASSERT_NO_FATAL_FAILURE(
          AssertWaitForStateOrDeleted(note, clients[i], CS_RESETTING));
    }
    ASSERT_NO_FATAL_FAILURE(
        AssertWaitForStateOrDeleted(note, clients[i], CS_RESET));
    // ResetDone requests Destroy().
    ASSERT_NO_FATAL_FAILURE(
        AssertWaitForStateOrDeleted(note, clients[i], CS_DESTROYED));
  }
  // Finally assert that decoding went as expected.
  for (size_t i = 0;
       i < num_concurrent_decoders && !skip_performance_and_correctness_checks;
       ++i) {
    // We can only make performance/correctness assertions if the decoder was
    // allowed to finish.
    if (delete_decoder_state < CS_FLUSHED)
      continue;
    GLRenderingVDAClient* client = clients[i];
    TestVideoFile* video_file = test_video_files_[i % test_video_files_.size()];
    if (video_file->num_frames > 0) {
      // Expect the decoded frames may be more than the video frames as frames
      // could still be returned until resetting done.
      if (video_file->reset_after_frame_num > 0)
        EXPECT_GE(client->num_decoded_frames(), video_file->num_frames);
      else
        EXPECT_EQ(client->num_decoded_frames(), video_file->num_frames);
    }
    if (reset_point == END_OF_STREAM_RESET) {
      EXPECT_EQ(video_file->num_fragments, client->num_skipped_fragments() +
                                               client->num_queued_fragments());
      EXPECT_EQ(client->num_done_bitstream_buffers(),
                client->num_queued_fragments());
    }
    LOG(INFO) << "Decoder " << i << " fps: " << client->frames_per_second();
    if (!render_as_thumbnails) {
      int min_fps = suppress_rendering ? video_file->min_fps_no_render
                                       : video_file->min_fps_render;
      if (min_fps > 0 && !test_reuse_delay)
        EXPECT_GT(client->frames_per_second(), min_fps);
    }
  }

  if (render_as_thumbnails) {
    std::vector<unsigned char> rgb;
    bool alpha_solid;
    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    g_env->GetRenderingTaskRunner()->PostTask(
        FROM_HERE, base::Bind(&RenderingHelper::GetThumbnailsAsRGB,
                              base::Unretained(&rendering_helper_), &rgb,
                              &alpha_solid, &done));
    done.Wait();

    std::vector<std::string> golden_md5s;
    std::string md5_string = base::MD5String(
        base::StringPiece(reinterpret_cast<char*>(&rgb[0]), rgb.size()));
    ReadGoldenThumbnailMD5s(test_video_files_[0], &golden_md5s);
    std::vector<std::string>::iterator match =
        find(golden_md5s.begin(), golden_md5s.end(), md5_string);
    if (match == golden_md5s.end()) {
      // Convert raw RGB into PNG for export.
      std::vector<unsigned char> png;
      gfx::PNGCodec::Encode(&rgb[0],
                            gfx::PNGCodec::FORMAT_RGB,
                            kThumbnailsPageSize,
                            kThumbnailsPageSize.width() * 3,
                            true,
                            std::vector<gfx::PNGCodec::Comment>(),
                            &png);

      LOG(ERROR) << "Unknown thumbnails MD5: " << md5_string;

      base::FilePath filepath(test_video_files_[0]->file_name);
      filepath = filepath.AddExtension(FILE_PATH_LITERAL(".bad_thumbnails"));
      filepath = filepath.AddExtension(FILE_PATH_LITERAL(".png"));
      int num_bytes = base::WriteFile(
          filepath, reinterpret_cast<char*>(&png[0]), png.size());
      ASSERT_EQ(num_bytes, static_cast<int>(png.size()));
    }
    ASSERT_NE(match, golden_md5s.end());
    EXPECT_EQ(alpha_solid, true) << "RGBA frame had incorrect alpha";
  }

  // Output the frame delivery time to file
  // We can only make performance/correctness assertions if the decoder was
  // allowed to finish.
  if (g_output_log != NULL && delete_decoder_state >= CS_FLUSHED) {
    base::File output_file(
        base::FilePath(g_output_log),
        base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    for (size_t i = 0; i < num_concurrent_decoders; ++i) {
      clients[i]->OutputFrameDeliveryTimes(&output_file);
    }
  }

  g_env->GetRenderingTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&STLDeleteElements<std::vector<GLRenderingVDAClient*>>,
                 &clients));
  g_env->GetRenderingTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&STLDeleteElements<
                     std::vector<ClientStateNotification<ClientState>*>>,
                 &notes));
  WaitUntilIdle();
};

// Test that replay after EOS works fine.
INSTANTIATE_TEST_CASE_P(
    ReplayAfterEOS,
    VideoDecodeAcceleratorParamTest,
    ::testing::Values(
        std::make_tuple(1, 1, 4, END_OF_STREAM_RESET, CS_RESET, false, false)));

// Test that Reset() before the first Decode() works fine.
INSTANTIATE_TEST_CASE_P(ResetBeforeDecode,
                        VideoDecodeAcceleratorParamTest,
                        ::testing::Values(std::make_tuple(1,
                                                          1,
                                                          1,
                                                          START_OF_STREAM_RESET,
                                                          CS_RESET,
                                                          false,
                                                          false)));

// Test Reset() immediately after Decode() containing config info.
INSTANTIATE_TEST_CASE_P(
    ResetAfterFirstConfigInfo,
    VideoDecodeAcceleratorParamTest,
    ::testing::Values(std::make_tuple(1,
                                      1,
                                      1,
                                      RESET_AFTER_FIRST_CONFIG_INFO,
                                      CS_RESET,
                                      false,
                                      false)));

// Test that Reset() mid-stream works fine and doesn't affect decoding even when
// Decode() calls are made during the reset.
INSTANTIATE_TEST_CASE_P(
    MidStreamReset,
    VideoDecodeAcceleratorParamTest,
    ::testing::Values(
        std::make_tuple(1, 1, 1, MID_STREAM_RESET, CS_RESET, false, false)));

INSTANTIATE_TEST_CASE_P(
    SlowRendering,
    VideoDecodeAcceleratorParamTest,
    ::testing::Values(
        std::make_tuple(1, 1, 1, END_OF_STREAM_RESET, CS_RESET, true, false)));

// Test that Destroy() mid-stream works fine (primarily this is testing that no
// crashes occur).
INSTANTIATE_TEST_CASE_P(
    TearDownTiming,
    VideoDecodeAcceleratorParamTest,
    ::testing::Values(
        std::make_tuple(1,
                        1,
                        1,
                        END_OF_STREAM_RESET,
                        CS_DECODER_SET,
                        false,
                        false),
        std::make_tuple(1,
                        1,
                        1,
                        END_OF_STREAM_RESET,
                        CS_INITIALIZED,
                        false,
                        false),
        std::make_tuple(1,
                        1,
                        1,
                        END_OF_STREAM_RESET,
                        CS_FLUSHING,
                        false,
                        false),
        std::make_tuple(1, 1, 1, END_OF_STREAM_RESET, CS_FLUSHED, false, false),
        std::make_tuple(1,
                        1,
                        1,
                        END_OF_STREAM_RESET,
                        CS_RESETTING,
                        false,
                        false),
        std::make_tuple(1, 1, 1, END_OF_STREAM_RESET, CS_RESET, false, false),
        std::make_tuple(1,
                        1,
                        1,
                        END_OF_STREAM_RESET,
                        static_cast<ClientState>(-1),
                        false,
                        false),
        std::make_tuple(1,
                        1,
                        1,
                        END_OF_STREAM_RESET,
                        static_cast<ClientState>(-10),
                        false,
                        false),
        std::make_tuple(1,
                        1,
                        1,
                        END_OF_STREAM_RESET,
                        static_cast<ClientState>(-100),
                        false,
                        false)));

// Test that decoding various variation works with multiple in-flight decodes.
INSTANTIATE_TEST_CASE_P(
    DecodeVariations,
    VideoDecodeAcceleratorParamTest,
    ::testing::Values(
        std::make_tuple(1, 1, 1, END_OF_STREAM_RESET, CS_RESET, false, false),
        std::make_tuple(1, 10, 1, END_OF_STREAM_RESET, CS_RESET, false, false),
        // Tests queuing.
        std::make_tuple(1,
                        15,
                        1,
                        END_OF_STREAM_RESET,
                        CS_RESET,
                        false,
                        false)));

// Find out how many concurrent decoders can go before we exhaust system
// resources.
INSTANTIATE_TEST_CASE_P(
    ResourceExhaustion,
    VideoDecodeAcceleratorParamTest,
    ::testing::Values(
        // +0 hack below to promote enum to int.
        std::make_tuple(kMinSupportedNumConcurrentDecoders + 0,
                        1,
                        1,
                        END_OF_STREAM_RESET,
                        CS_RESET,
                        false,
                        false),
        std::make_tuple(kMinSupportedNumConcurrentDecoders + 1,
                        1,
                        1,
                        END_OF_STREAM_RESET,
                        CS_RESET,
                        false,
                        false)));

// Thumbnailing test
INSTANTIATE_TEST_CASE_P(
    Thumbnail,
    VideoDecodeAcceleratorParamTest,
    ::testing::Values(
        std::make_tuple(1, 1, 1, END_OF_STREAM_RESET, CS_RESET, false, true)));

// Measure the median of the decode time when VDA::Decode is called 30 times per
// second.
TEST_F(VideoDecodeAcceleratorTest, TestDecodeTimeMedian) {
  RenderingHelperParams helper_params;

  // Disable rendering by setting the rendering_fps = 0.
  helper_params.rendering_fps = 0;
  helper_params.warm_up_iterations = 0;
  helper_params.render_as_thumbnails = false;

  ClientStateNotification<ClientState>* note =
      new ClientStateNotification<ClientState>();
  GLRenderingVDAClient* client =
    new GLRenderingVDAClient(0,
                             &rendering_helper_,
                             note,
                             test_video_files_[0]->data_str,
                             1,
                             1,
                             test_video_files_[0]->reset_after_frame_num,
                             CS_RESET,
                             test_video_files_[0]->width,
                             test_video_files_[0]->height,
                             test_video_files_[0]->profile,
                             g_fake_decoder,
                             true,
                             std::numeric_limits<int>::max(),
                             kWebRtcDecodeCallsPerSecond,
                             false /* render_as_thumbnail */);
  helper_params.window_sizes.push_back(
      gfx::Size(test_video_files_[0]->width, test_video_files_[0]->height));
  InitializeRenderingHelper(helper_params);
  CreateAndStartDecoder(client, note);
  WaitUntilDecodeFinish(note);

  base::TimeDelta decode_time_median = client->decode_time_median();
  std::string output_string =
      base::StringPrintf("Decode time median: %" PRId64 " us",
                         decode_time_median.InMicroseconds());
  LOG(INFO) << output_string;

  if (g_output_log != NULL)
    OutputLogFile(g_output_log, output_string);

  g_env->GetRenderingTaskRunner()->DeleteSoon(FROM_HERE, client);
  g_env->GetRenderingTaskRunner()->DeleteSoon(FROM_HERE, note);
  WaitUntilIdle();
};

// TODO(fischman, vrk): add more tests!  In particular:
// - Test life-cycle: Seek/Stop/Pause/Play for a single decoder.
// - Test alternate configurations
// - Test failure conditions.
// - Test frame size changes mid-stream

}  // namespace
}  // namespace media

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);  // Removes gtest-specific args.
  base::CommandLine::Init(argc, argv);

  // Needed to enable DVLOG through --vmodule.
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  LOG_ASSERT(logging::InitLogging(settings));

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  DCHECK(cmd_line);

  base::CommandLine::SwitchMap switches = cmd_line->GetSwitches();
  for (base::CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    if (it->first == "test_video_data") {
      media::g_test_video_data = it->second.c_str();
      continue;
    }
    // The output log for VDA performance test.
    if (it->first == "output_log") {
      media::g_output_log = it->second.c_str();
      continue;
    }
    if (it->first == "rendering_fps") {
      // On Windows, CommandLine::StringType is wstring. We need to convert
      // it to std::string first
      std::string input(it->second.begin(), it->second.end());
      LOG_ASSERT(base::StringToDouble(input, &media::g_rendering_fps));
      continue;
    }
    if (it->first == "rendering_warm_up") {
      std::string input(it->second.begin(), it->second.end());
      LOG_ASSERT(base::StringToInt(input, &media::g_rendering_warm_up));
      continue;
    }
    // TODO(owenlin): Remove this flag once it is not used in autotest.
    if (it->first == "disable_rendering") {
      media::g_rendering_fps = 0;
      continue;
    }

    if (it->first == "num_play_throughs") {
      std::string input(it->second.begin(), it->second.end());
      LOG_ASSERT(base::StringToInt(input, &media::g_num_play_throughs));
      continue;
    }
    if (it->first == "fake_decoder") {
      media::g_fake_decoder = 1;
      continue;
    }
    if (it->first == "v" || it->first == "vmodule")
      continue;
    if (it->first == "ozone-platform" || it->first == "ozone-use-surfaceless")
      continue;
    if (it->first == "test_import") {
      media::g_test_import = true;
      continue;
    }
    LOG(FATAL) << "Unexpected switch: " << it->first << ":" << it->second;
  }

  base::ShadowingAtExitManager at_exit_manager;
#if defined(OS_WIN) || defined(USE_OZONE)
  // For windows the decoding thread initializes the media foundation decoder
  // which uses COM. We need the thread to be a UI thread.
  // On Ozone, the backend initializes the event system using a UI
  // thread.
  base::MessageLoopForUI main_loop;
#else
  base::MessageLoop main_loop;
#endif  // OS_WIN || USE_OZONE

#if defined(USE_OZONE)
  ui::OzonePlatform::InitializeForUI();
#endif

#if defined(OS_CHROMEOS) && defined(ARCH_CPU_X86_FAMILY)
  media::VaapiWrapper::PreSandboxInitialization();
#endif

  media::g_env =
      reinterpret_cast<media::VideoDecodeAcceleratorTestEnvironment*>(
          testing::AddGlobalTestEnvironment(
              new media::VideoDecodeAcceleratorTestEnvironment()));

  return RUN_ALL_TESTS();
}
