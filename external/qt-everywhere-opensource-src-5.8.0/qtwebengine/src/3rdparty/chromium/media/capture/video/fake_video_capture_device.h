// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of a fake VideoCaptureDevice class. Used for testing other
// video capture classes when no real hardware is available.

#ifndef MEDIA_CAPTURE_VIDEO_FAKE_VIDEO_CAPTURE_DEVICE_H_
#define MEDIA_CAPTURE_VIDEO_FAKE_VIDEO_CAPTURE_DEVICE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/atomicops.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/capture/video/video_capture_device.h"

namespace media {

class CAPTURE_EXPORT FakeVideoCaptureDevice : public VideoCaptureDevice {
 public:
  enum class BufferOwnership {
    OWN_BUFFERS,
    CLIENT_BUFFERS,
  };

  FakeVideoCaptureDevice(BufferOwnership buffer_ownership,
                         float fake_capture_rate);
  ~FakeVideoCaptureDevice() override;

  // VideoCaptureDevice implementation.
  void AllocateAndStart(const VideoCaptureParams& params,
                        std::unique_ptr<Client> client) override;
  void StopAndDeAllocate() override;
  void GetPhotoCapabilities(
      ScopedResultCallback<GetPhotoCapabilitiesCallback> callback) override;
  void TakePhoto(ScopedResultCallback<TakePhotoCallback> callback) override;

 private:
  void CaptureUsingOwnBuffers(base::TimeTicks expected_execution_time);
  void CaptureUsingClientBuffers(base::TimeTicks expected_execution_time);
  void BeepAndScheduleNextCapture(
      base::TimeTicks expected_execution_time,
      const base::Callback<void(base::TimeTicks)>& next_capture);

  // |thread_checker_| is used to check that all methods are called in the
  // correct thread that owns the object.
  base::ThreadChecker thread_checker_;

  const BufferOwnership buffer_ownership_;
  // Frame rate of the fake video device.
  const float fake_capture_rate_;

  std::unique_ptr<VideoCaptureDevice::Client> client_;
  // |fake_frame_| is used for capturing on Own Buffers.
  std::unique_ptr<uint8_t[]> fake_frame_;
  // Time when the next beep occurs.
  base::TimeDelta beep_time_;
  // Time since the fake video started rendering frames.
  base::TimeDelta elapsed_time_;
  VideoCaptureFormat capture_format_;

  // The system time when we receive the first frame.
  base::TimeTicks first_ref_time_;
  // FakeVideoCaptureDevice post tasks to itself for frame construction and
  // needs to deal with asynchronous StopAndDeallocate().
  base::WeakPtrFactory<FakeVideoCaptureDevice> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeVideoCaptureDevice);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_FAKE_VIDEO_CAPTURE_DEVICE_H_
