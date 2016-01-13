// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of a fake VideoCaptureDevice class. Used for testing other
// video capture classes when no real hardware is available.

#ifndef MEDIA_VIDEO_CAPTURE_FAKE_VIDEO_CAPTURE_DEVICE_H_
#define MEDIA_VIDEO_CAPTURE_FAKE_VIDEO_CAPTURE_DEVICE_H_

#include <string>

#include "base/atomicops.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "media/video/capture/video_capture_device.h"

namespace media {

class MEDIA_EXPORT FakeVideoCaptureDevice : public VideoCaptureDevice {
 public:
  static const int kFakeCaptureTimeoutMs = 50;

  FakeVideoCaptureDevice();
  virtual ~FakeVideoCaptureDevice();

  // VideoCaptureDevice implementation.
  virtual void AllocateAndStart(
      const VideoCaptureParams& params,
      scoped_ptr<VideoCaptureDevice::Client> client) OVERRIDE;
  virtual void StopAndDeAllocate() OVERRIDE;

  // Sets the formats to use sequentially when the device is configured as
  // variable capture resolution. Works only before AllocateAndStart() or
  // after StopAndDeallocate().
  void PopulateVariableFormatsRoster(const VideoCaptureFormats& formats);

 private:
  // Called on the |capture_thread_| only.
  void OnAllocateAndStart(const VideoCaptureParams& params,
                          scoped_ptr<Client> client);
  void OnStopAndDeAllocate();
  void OnCaptureTask();
  void Reallocate();

  // |thread_checker_| is used to check that destructor, AllocateAndStart() and
  // StopAndDeAllocate() are called in the correct thread that owns the object.
  base::ThreadChecker thread_checker_;

  base::Thread capture_thread_;
  // The following members are only used on the |capture_thread_|.
  scoped_ptr<VideoCaptureDevice::Client> client_;
  scoped_ptr<uint8[]> fake_frame_;
  int frame_count_;
  VideoCaptureFormat capture_format_;

  // When the device is allowed to change resolution, this vector holds the
  // available ones, used sequentially restarting at the end. These two members
  // are initialised in PopulateFormatRoster() before |capture_thread_| is
  // running and are subsequently read-only in that thread.
  std::vector<VideoCaptureFormat> format_roster_;
  int format_roster_index_;

  DISALLOW_COPY_AND_ASSIGN(FakeVideoCaptureDevice);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_FAKE_VIDEO_CAPTURE_DEVICE_H_
