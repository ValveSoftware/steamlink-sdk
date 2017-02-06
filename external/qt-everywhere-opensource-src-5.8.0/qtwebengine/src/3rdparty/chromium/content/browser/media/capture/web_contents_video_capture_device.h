// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_VIDEO_CAPTURE_DEVICE_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_VIDEO_CAPTURE_DEVICE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "media/capture/content/screen_capture_device_core.h"
#include "media/capture/video/video_capture_device.h"

namespace content {

// A virtualized VideoCaptureDevice that mirrors the displayed contents of a
// WebContents (i.e., the composition of an entire render frame tree), producing
// a stream of video frames.
//
// An instance is created by providing a device_id.  The device_id contains
// information necessary for finding a WebContents instance.  From then on,
// WebContentsVideoCaptureDevice will capture from the RenderWidgetHost that
// encompasses the currently active RenderFrameHost tree for that that
// WebContents instance.  As the RenderFrameHost tree mutates (e.g., due to page
// navigations, or crashes/reloads), capture will continue without interruption.
class CONTENT_EXPORT WebContentsVideoCaptureDevice
    : public media::VideoCaptureDevice {
 public:
  // Create a WebContentsVideoCaptureDevice instance from the given
  // |device_id|.  Returns NULL if |device_id| is invalid.
  static media::VideoCaptureDevice* Create(const std::string& device_id);

  ~WebContentsVideoCaptureDevice() override;

  // VideoCaptureDevice implementation.
  void AllocateAndStart(const media::VideoCaptureParams& params,
                        std::unique_ptr<Client> client) override;
  void RequestRefreshFrame() override;
  void StopAndDeAllocate() override;

 private:
  WebContentsVideoCaptureDevice(
      int render_process_id,
      int main_render_frame_id,
      bool enable_auto_throttling);

  const std::unique_ptr<media::ScreenCaptureDeviceCore> core_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsVideoCaptureDevice);
};


}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_VIDEO_CAPTURE_DEVICE_H_
