// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/fake_video_capture_device_factory.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"

namespace {

// Factory has one device by default; I420. If there are more, the second device
// is of Y16 format while the rest are I420.
media::VideoPixelFormat GetPixelFormat(const std::string& device_id) {
  return (device_id == "/dev/video1") ? media::PIXEL_FORMAT_Y16
                                      : media::PIXEL_FORMAT_I420;
}
}

namespace media {

// Cap the frame rate command line input to reasonable values.
static const float kFakeCaptureMinFrameRate = 5.0f;
static const float kFakeCaptureMaxFrameRate = 60.0f;
// Default rate if none is specified as part of the command line.
static const float kFakeCaptureDefaultFrameRate = 20.0f;
// Cap the device count command line input to reasonable values.
static const int kFakeCaptureMinDeviceCount = 1;
static const int kFakeCaptureMaxDeviceCount = 10;

FakeVideoCaptureDeviceFactory::FakeVideoCaptureDeviceFactory()
    : number_of_devices_(1),
      fake_vcd_ownership_(FakeVideoCaptureDevice::BufferOwnership::OWN_BUFFERS),
      frame_rate_(kFakeCaptureDefaultFrameRate) {}

std::unique_ptr<VideoCaptureDevice> FakeVideoCaptureDeviceFactory::CreateDevice(
    const VideoCaptureDeviceDescriptor& device_descriptor) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!command_line_parsed_) {
    ParseCommandLine();
    command_line_parsed_ = true;
  }

  for (int n = 0; n < number_of_devices_; ++n) {
    std::string possible_id = base::StringPrintf("/dev/video%d", n);
    if (device_descriptor.device_id.compare(possible_id) == 0) {
      return std::unique_ptr<VideoCaptureDevice>(new FakeVideoCaptureDevice(
          fake_vcd_ownership_, frame_rate_, GetPixelFormat(possible_id)));
    }
  }
  return std::unique_ptr<VideoCaptureDevice>();
}

void FakeVideoCaptureDeviceFactory::GetDeviceDescriptors(
    VideoCaptureDeviceDescriptors* device_descriptors) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(device_descriptors->empty());

  if (!command_line_parsed_) {
    ParseCommandLine();
    command_line_parsed_ = true;
  }

  for (int n = 0; n < number_of_devices_; ++n) {
    device_descriptors->emplace_back(base::StringPrintf("fake_device_%d", n),
                                     base::StringPrintf("/dev/video%d", n),
#if defined(OS_LINUX)
                                     VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE
#elif defined(OS_MACOSX)
                                     VideoCaptureApi::MACOSX_AVFOUNDATION
#elif defined(OS_WIN)
                                     VideoCaptureApi::WIN_DIRECT_SHOW
#elif defined(OS_ANDROID)
                                     VideoCaptureApi::ANDROID_API2_LEGACY
#endif
                                     );
  }
}

void FakeVideoCaptureDeviceFactory::GetSupportedFormats(
    const VideoCaptureDeviceDescriptor& device_descriptor,
    VideoCaptureFormats* supported_formats) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const gfx::Size supported_sizes[] = {
      gfx::Size(96, 96), gfx::Size(320, 240), gfx::Size(640, 480),
      gfx::Size(1280, 720), gfx::Size(1920, 1080)};
  supported_formats->clear();
  for (const auto& size : supported_sizes) {
    supported_formats->push_back(VideoCaptureFormat(
        size, frame_rate_, GetPixelFormat(device_descriptor.device_id)));
  }
}

// Optional comma delimited parameters to the command line can specify buffer
// ownership, device count, and the fake video devices FPS.
// Examples: "ownership=client, device-count=2, fps=60" "fps=30"
void FakeVideoCaptureDeviceFactory::ParseCommandLine() {
  const std::string option =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kUseFakeDeviceForMediaStream);
  base::StringTokenizer option_tokenizer(option, ", ");
  option_tokenizer.set_quote_chars("\"");

  while (option_tokenizer.GetNext()) {
    std::vector<std::string> param =
        base::SplitString(option_tokenizer.token(), "=", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);

    if (param.size() != 2u) {
      LOG(WARNING) << "Forget a value '" << option << "'? Use name=value for "
                   << switches::kUseFakeDeviceForMediaStream << ".";
      return;
    }

    if (base::EqualsCaseInsensitiveASCII(param.front(), "ownership") &&
        base::EqualsCaseInsensitiveASCII(param.back(), "client")) {
      fake_vcd_ownership_ =
          FakeVideoCaptureDevice::BufferOwnership::CLIENT_BUFFERS;
    } else if (base::EqualsCaseInsensitiveASCII(param.front(), "fps")) {
      double fps = 0;
      if (base::StringToDouble(param.back(), &fps)) {
        frame_rate_ =
            std::max(kFakeCaptureMinFrameRate, static_cast<float>(fps));
        frame_rate_ = std::min(kFakeCaptureMaxFrameRate, frame_rate_);
      }
    } else if (base::EqualsCaseInsensitiveASCII(param.front(),
                                                "device-count")) {
      unsigned int count = 0;
      if (base::StringToUint(param.back(), &count)) {
        number_of_devices_ = std::min(
            kFakeCaptureMaxDeviceCount,
            std::max(kFakeCaptureMinDeviceCount, static_cast<int>(count)));
      }
    }
  }
}

}  // namespace media
