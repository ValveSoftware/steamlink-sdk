// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MacOSX implementation of generic VideoCaptureDevice, using either QTKit or
// AVFoundation as native capture API. QTKit is available in all OSX versions,
// although namely deprecated in 10.9, and AVFoundation is available in versions
// 10.7 (Lion) and later.

#ifndef MEDIA_VIDEO_CAPTURE_MAC_VIDEO_CAPTURE_DEVICE_MAC_H_
#define MEDIA_VIDEO_CAPTURE_MAC_VIDEO_CAPTURE_DEVICE_MAC_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/video/capture/video_capture_device.h"
#include "media/video/capture/video_capture_types.h"

@protocol PlatformVideoCapturingMac;

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

// Called by VideoCaptureManager to open, close and start, stop Mac video
// capture devices.
class VideoCaptureDeviceMac : public VideoCaptureDevice {
 public:
  explicit VideoCaptureDeviceMac(const Name& device_name);
  virtual ~VideoCaptureDeviceMac();

  // VideoCaptureDevice implementation.
  virtual void AllocateAndStart(
      const VideoCaptureParams& params,
      scoped_ptr<VideoCaptureDevice::Client> client) OVERRIDE;
  virtual void StopAndDeAllocate() OVERRIDE;

  bool Init(VideoCaptureDevice::Name::CaptureApiType capture_api_type);

  // Called to deliver captured video frames.
  void ReceiveFrame(const uint8* video_frame,
                    int video_frame_length,
                    const VideoCaptureFormat& frame_format,
                    int aspect_numerator,
                    int aspect_denominator);

  void ReceiveError(const std::string& reason);

 private:
  void SetErrorState(const std::string& reason);
  void LogMessage(const std::string& message);
  bool UpdateCaptureResolution();

  // Flag indicating the internal state.
  enum InternalState {
    kNotInitialized,
    kIdle,
    kCapturing,
    kError
  };

  Name device_name_;
  scoped_ptr<VideoCaptureDevice::Client> client_;

  VideoCaptureFormat capture_format_;
  // These variables control the two-step configure-start process for QTKit HD:
  // the device is first started with no configuration and the captured frames
  // are inspected to check if the camera really supports HD. AVFoundation does
  // not need this process so |final_resolution_selected_| is false then.
  bool final_resolution_selected_;
  bool tried_to_square_pixels_;

  // Only read and write state_ from inside this loop.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  InternalState state_;

  id<PlatformVideoCapturingMac> capture_device_;

  // Used with Bind and PostTask to ensure that methods aren't called after the
  // VideoCaptureDeviceMac is destroyed.
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<VideoCaptureDeviceMac> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureDeviceMac);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_MAC_VIDEO_CAPTURE_DEVICE_MAC_H_
