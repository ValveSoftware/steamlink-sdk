// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_VIDEO_CAPTURE_DEVICE_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_VIDEO_CAPTURE_DEVICE_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "media/video/capture/video_capture_device.h"

namespace content {

class ContentVideoCaptureDeviceCore;

// A virtualized VideoCaptureDevice that mirrors the displayed contents of a
// tab (accessed via its associated WebContents instance), producing a stream of
// video frames.
//
// An instance is created by providing a device_id.  The device_id contains the
// routing ID for a RenderViewHost, and from the RenderViewHost instance, a
// reference to its associated WebContents instance is acquired.  From then on,
// WebContentsVideoCaptureDevice will capture from whatever render view is
// currently associated with that WebContents instance.  This allows the
// underlying render view to be swapped out (e.g., due to navigation or
// crashes/reloads), without any interruption in capturing.
class CONTENT_EXPORT WebContentsVideoCaptureDevice
    : public media::VideoCaptureDevice {
 public:
  // Construct from a |device_id| string of the form:
  //   "virtual-media-stream://render_process_id:render_view_id", where
  // |render_process_id| and |render_view_id| are decimal integers.
  // |destroy_cb| is invoked on an outside thread once all outstanding objects
  // are completely destroyed -- this will be some time after the
  // WebContentsVideoCaptureDevice is itself deleted.
  static media::VideoCaptureDevice* Create(const std::string& device_id);

  virtual ~WebContentsVideoCaptureDevice();

  // VideoCaptureDevice implementation.
  virtual void AllocateAndStart(const media::VideoCaptureParams& params,
                                scoped_ptr<Client> client) OVERRIDE;
  virtual void StopAndDeAllocate() OVERRIDE;

 private:
  WebContentsVideoCaptureDevice(int render_process_id, int render_view_id);

  const scoped_ptr<ContentVideoCaptureDeviceCore> core_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsVideoCaptureDevice);
};


}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_VIDEO_CAPTURE_DEVICE_H_
