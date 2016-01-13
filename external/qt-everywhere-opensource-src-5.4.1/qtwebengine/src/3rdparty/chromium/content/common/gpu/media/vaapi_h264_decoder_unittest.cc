// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

// This has to be included first.
// See http://code.google.com/p/googletest/issues/detail?id=371
#include "testing/gtest/include/gtest/gtest.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "content/common/gpu/media/vaapi_h264_decoder.h"
#include "media/base/video_decoder_config.h"
#include "third_party/libyuv/include/libyuv.h"

// This program is run like this:
// DISPLAY=:0 ./vaapi_h264_decoder_unittest --input_file input.h264
// [--output_file output.i420] [--md5sum expected_md5_hex]
//
// The input is read from input.h264. The output is written to output.i420 if it
// is given. It also verifies the MD5 sum of the decoded I420 data if the
// expected MD5 sum is given.

namespace content {
namespace {

// These are the command line parameters
base::FilePath g_input_file;
base::FilePath g_output_file;
std::string g_md5sum;

// These default values are used if nothing is specified in the command line.
const base::FilePath::CharType* kDefaultInputFile =
    FILE_PATH_LITERAL("test-25fps.h264");
const char* kDefaultMD5Sum = "3af866863225b956001252ebeccdb71d";

// This class encapsulates the use of VaapiH264Decoder to a simpler interface.
// It reads an H.264 Annex B bytestream from a file and outputs the decoded
// frames (in I420 format) to another file. The output file can be played by
// $ mplayer test_dec.yuv -demuxer rawvideo -rawvideo w=1920:h=1080
//
// To use the class, construct an instance, call Initialize() to specify the
// input and output file paths, then call Run() to decode the whole stream and
// output the frames.
//
// This class must be created, called and destroyed on a single thread, and
// does nothing internally on any other thread.
class VaapiH264DecoderLoop {
 public:
  VaapiH264DecoderLoop();
  ~VaapiH264DecoderLoop();

  // Initialize the decoder. Return true if successful.
  bool Initialize(base::FilePath input_file, base::FilePath output_file);

  // Run the decode loop. The decoded data is written to the file specified by
  // output_file in Initialize().  Return true if all decoding is successful.
  bool Run();

  // Get the MD5 sum of the decoded data.
  std::string GetMD5Sum();

 private:
  // Callback from the decoder when a picture is decoded.
  void OutputPicture(int32 input_id,
                     const scoped_refptr<VASurface>& va_surface);

  // Recycle one surface and put it on available_surfaces_ list.
  void RecycleSurface(VASurfaceID va_surface_id);

  // Give all surfaces in available_surfaces_ to the decoder.
  void RefillSurfaces();

  // Free the current set of surfaces and allocate a new set of
  // surfaces. Returns true when successful.
  bool AllocateNewSurfaces();

  // Use the data in the frame: write to file and update MD5 sum.
  bool ProcessVideoFrame(const scoped_refptr<media::VideoFrame>& frame);

  scoped_ptr<VaapiWrapper> wrapper_;
  scoped_ptr<VaapiH264Decoder> decoder_;
  std::string data_;            // data read from input_file
  base::FilePath output_file_;  // output data is written to this file
  std::vector<VASurfaceID> available_surfaces_;

  // These members (x_display_, num_outputted_pictures_, num_surfaces_)
  // need to be initialized and possibly freed manually.
  Display* x_display_;
  int num_outputted_pictures_;  // number of pictures already outputted
  size_t num_surfaces_;  // number of surfaces in the current set of surfaces
  base::MD5Context md5_context_;
};

VaapiH264DecoderLoop::VaapiH264DecoderLoop()
    : x_display_(NULL), num_outputted_pictures_(0), num_surfaces_(0) {
  base::MD5Init(&md5_context_);
}

VaapiH264DecoderLoop::~VaapiH264DecoderLoop() {
  // We need to destruct decoder and wrapper first because:
  // (1) The decoder has a reference to the wrapper.
  // (2) The wrapper has a reference to x_display_.
  decoder_.reset();
  wrapper_.reset();

  if (x_display_) {
    XCloseDisplay(x_display_);
  }
}

void LogOnError(VaapiH264Decoder::VAVDAH264DecoderFailure error) {
  LOG(FATAL) << "Oh noes! Decoder failed: " << error;
}

bool VaapiH264DecoderLoop::Initialize(base::FilePath input_file,
                                      base::FilePath output_file) {
  x_display_ = XOpenDisplay(NULL);
  if (!x_display_) {
    LOG(ERROR) << "Can't open X display";
    return false;
  }

  media::VideoCodecProfile profile = media::H264PROFILE_BASELINE;
  base::Closure report_error_cb =
      base::Bind(&LogOnError, VaapiH264Decoder::VAAPI_ERROR);
  wrapper_ = VaapiWrapper::Create(profile, x_display_, report_error_cb);
  if (!wrapper_.get()) {
    LOG(ERROR) << "Can't create vaapi wrapper";
    return false;
  }

  decoder_.reset(new VaapiH264Decoder(
      wrapper_.get(),
      base::Bind(&VaapiH264DecoderLoop::OutputPicture, base::Unretained(this)),
      base::Bind(&LogOnError)));

  if (!base::ReadFileToString(input_file, &data_)) {
    LOG(ERROR) << "failed to read input data from " << input_file.value();
    return false;
  }

  const int input_id = 0;  // We don't use input_id in this class.
  decoder_->SetStream(
      reinterpret_cast<const uint8*>(data_.c_str()), data_.size(), input_id);

  // This creates or truncates output_file.
  // Without it, AppendToFile() will not work.
  if (!output_file.empty()) {
    if (base::WriteFile(output_file, NULL, 0) != 0) {
      return false;
    }
    output_file_ = output_file;
  }

  return true;
}

bool VaapiH264DecoderLoop::Run() {
  while (1) {
    switch (decoder_->Decode()) {
      case VaapiH264Decoder::kDecodeError:
        LOG(ERROR) << "Decode Error";
        return false;
      case VaapiH264Decoder::kAllocateNewSurfaces:
        VLOG(1) << "Allocate new surfaces";
        if (!AllocateNewSurfaces()) {
          LOG(ERROR) << "Failed to allocate new surfaces";
          return false;
        }
        break;
      case VaapiH264Decoder::kRanOutOfStreamData: {
        bool rc = decoder_->Flush();
        VLOG(1) << "Flush returns " << rc;
        return rc;
      }
      case VaapiH264Decoder::kRanOutOfSurfaces:
        VLOG(1) << "Ran out of surfaces";
        RefillSurfaces();
        break;
    }
  }
}

std::string VaapiH264DecoderLoop::GetMD5Sum() {
  base::MD5Digest digest;
  base::MD5Final(&digest, &md5_context_);
  return MD5DigestToBase16(digest);
}

scoped_refptr<media::VideoFrame> CopyNV12ToI420(VAImage* image, void* mem) {
  int width = image->width;
  int height = image->height;

  DVLOG(1) << "CopyNV12ToI420 width=" << width << ", height=" << height;

  const gfx::Size coded_size(width, height);
  const gfx::Rect visible_rect(width, height);
  const gfx::Size natural_size(width, height);

  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::CreateFrame(media::VideoFrame::I420,
                                     coded_size,
                                     visible_rect,
                                     natural_size,
                                     base::TimeDelta());

  uint8_t* mem_byte_ptr = static_cast<uint8_t*>(mem);
  uint8_t* src_y = mem_byte_ptr + image->offsets[0];
  uint8_t* src_uv = mem_byte_ptr + image->offsets[1];
  int src_stride_y = image->pitches[0];
  int src_stride_uv = image->pitches[1];

  uint8_t* dst_y = frame->data(media::VideoFrame::kYPlane);
  uint8_t* dst_u = frame->data(media::VideoFrame::kUPlane);
  uint8_t* dst_v = frame->data(media::VideoFrame::kVPlane);
  int dst_stride_y = frame->stride(media::VideoFrame::kYPlane);
  int dst_stride_u = frame->stride(media::VideoFrame::kUPlane);
  int dst_stride_v = frame->stride(media::VideoFrame::kVPlane);

  int rc = libyuv::NV12ToI420(src_y,
                              src_stride_y,
                              src_uv,
                              src_stride_uv,
                              dst_y,
                              dst_stride_y,
                              dst_u,
                              dst_stride_u,
                              dst_v,
                              dst_stride_v,
                              width,
                              height);
  CHECK_EQ(0, rc);
  return frame;
}

bool VaapiH264DecoderLoop::ProcessVideoFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
  frame->HashFrameForTesting(&md5_context_);

  if (output_file_.empty())
    return true;

  for (size_t i = 0; i < media::VideoFrame::NumPlanes(frame->format()); i++) {
    int to_write = media::VideoFrame::PlaneAllocationSize(
        frame->format(), i, frame->coded_size());
    const char* buf = reinterpret_cast<const char*>(frame->data(i));
    int written = base::AppendToFile(output_file_, buf, to_write);
    if (written != to_write)
      return false;
  }
  return true;
}

void VaapiH264DecoderLoop::OutputPicture(
    int32 input_id,
    const scoped_refptr<VASurface>& va_surface) {
  VLOG(1) << "OutputPicture: picture " << num_outputted_pictures_++;

  VAImage image;
  void* mem;

  if (!wrapper_->GetVaImageForTesting(va_surface->id(), &image, &mem)) {
    LOG(ERROR) << "Cannot get VAImage.";
    return;
  }

  if (image.format.fourcc != VA_FOURCC_NV12) {
    LOG(ERROR) << "Unexpected image format: " << image.format.fourcc;
    wrapper_->ReturnVaImageForTesting(&image);
    return;
  }

  // Convert NV12 to I420 format.
  scoped_refptr<media::VideoFrame> frame = CopyNV12ToI420(&image, mem);

  if (frame.get()) {
    if (!ProcessVideoFrame(frame)) {
      LOG(ERROR) << "Write to file failed";
    }
  } else {
    LOG(ERROR) << "Cannot convert image to I420.";
  }

  wrapper_->ReturnVaImageForTesting(&image);
}

void VaapiH264DecoderLoop::RecycleSurface(VASurfaceID va_surface_id) {
  available_surfaces_.push_back(va_surface_id);
}

void VaapiH264DecoderLoop::RefillSurfaces() {
  for (size_t i = 0; i < available_surfaces_.size(); i++) {
    VASurface::ReleaseCB release_cb = base::Bind(
        &VaapiH264DecoderLoop::RecycleSurface, base::Unretained(this));
    scoped_refptr<VASurface> surface(
        new VASurface(available_surfaces_[i], release_cb));
    decoder_->ReuseSurface(surface);
  }
  available_surfaces_.clear();
}

bool VaapiH264DecoderLoop::AllocateNewSurfaces() {
  CHECK_EQ(num_surfaces_, available_surfaces_.size())
      << "not all surfaces are returned";

  available_surfaces_.clear();
  wrapper_->DestroySurfaces();

  gfx::Size size = decoder_->GetPicSize();
  num_surfaces_ = decoder_->GetRequiredNumOfPictures();
  return wrapper_->CreateSurfaces(size, num_surfaces_, &available_surfaces_);
}

TEST(VaapiH264DecoderTest, TestDecode) {
  base::FilePath input_file = g_input_file;
  base::FilePath output_file = g_output_file;
  std::string md5sum = g_md5sum;

  // If nothing specified, use the default file in the source tree.
  if (input_file.empty() && output_file.empty() && md5sum.empty()) {
    input_file = base::FilePath(kDefaultInputFile);
    md5sum = kDefaultMD5Sum;
  } else {
    ASSERT_FALSE(input_file.empty()) << "Need to specify --input_file";
  }

  VLOG(1) << "Input File: " << input_file.value();
  VLOG(1) << "Output File: " << output_file.value();
  VLOG(1) << "Expected MD5 sum: " << md5sum;

  content::VaapiH264DecoderLoop loop;
  ASSERT_TRUE(loop.Initialize(input_file, output_file))
      << "initialize decoder loop failed";
  ASSERT_TRUE(loop.Run()) << "run decoder loop failed";

  if (!md5sum.empty()) {
    std::string actual = loop.GetMD5Sum();
    VLOG(1) << "Actual MD5 sum: " << actual;
    EXPECT_EQ(md5sum, actual);
  }
}

}  // namespace
}  // namespace content

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);  // Removes gtest-specific args.
  CommandLine::Init(argc, argv);

  // Needed to enable DVLOG through --vmodule.
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  CHECK(logging::InitLogging(settings));

  // Process command line.
  CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  CHECK(cmd_line);

  CommandLine::SwitchMap switches = cmd_line->GetSwitches();
  for (CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end();
       ++it) {
    if (it->first == "input_file") {
      content::g_input_file = base::FilePath(it->second);
      continue;
    }
    if (it->first == "output_file") {
      content::g_output_file = base::FilePath(it->second);
      continue;
    }
    if (it->first == "md5sum") {
      content::g_md5sum = it->second;
      continue;
    }
    if (it->first == "v" || it->first == "vmodule")
      continue;
    LOG(FATAL) << "Unexpected switch: " << it->first << ":" << it->second;
  }

  return RUN_ALL_TESTS();
}
