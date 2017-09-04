// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "media/capture/capture_export.h"
#include "media/capture/video/video_capture_device.h"

namespace media {

class CAPTURE_EXPORT VideoCaptureJpegDecoder {
 public:
  // Enumeration of decoder status. The enumeration is published for clients to
  // decide the behavior according to STATUS.
  enum STATUS {
    INIT_PENDING,  // Default value while waiting initialization finished.
    INIT_PASSED,   // Initialization succeed.
    FAILED,        // JPEG decode is not supported, initialization failed, or
                   // decode error.
  };

  using DecodeDoneCB = base::Callback<void(
      std::unique_ptr<media::VideoCaptureDevice::Client::Buffer>,
      scoped_refptr<media::VideoFrame>)>;

  virtual ~VideoCaptureJpegDecoder() {}

  // Creates and intializes decoder asynchronously.
  virtual void Initialize() = 0;

  // Returns initialization status.
  virtual STATUS GetStatus() const = 0;

  // Decodes a JPEG picture.
  virtual void DecodeCapturedData(
      const uint8_t* data,
      size_t in_buffer_size,
      const media::VideoCaptureFormat& frame_format,
      base::TimeTicks reference_time,
      base::TimeDelta timestamp,
      std::unique_ptr<media::VideoCaptureDevice::Client::Buffer>
          out_buffer) = 0;
};

}  // namespace media
