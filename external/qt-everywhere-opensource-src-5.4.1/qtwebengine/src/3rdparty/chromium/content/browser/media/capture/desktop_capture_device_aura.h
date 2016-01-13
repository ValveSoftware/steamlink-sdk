// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_CAPTURE_DEVICE_AURA_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_CAPTURE_DEVICE_AURA_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/desktop_media_id.h"
#include "media/video/capture/video_capture_device.h"

namespace aura {
class Window;
}  // namespace aura

namespace content {

class ContentVideoCaptureDeviceCore;

// An implementation of VideoCaptureDevice that mirrors an Aura window.
class CONTENT_EXPORT DesktopCaptureDeviceAura
    : public media::VideoCaptureDevice {
 public:
  // Creates a VideoCaptureDevice for the Aura desktop.
  static media::VideoCaptureDevice* Create(const DesktopMediaID& source);

  virtual ~DesktopCaptureDeviceAura();

  // VideoCaptureDevice implementation.
  virtual void AllocateAndStart(const media::VideoCaptureParams& params,
                                scoped_ptr<Client> client) OVERRIDE;
  virtual void StopAndDeAllocate() OVERRIDE;

 private:
  DesktopCaptureDeviceAura(const DesktopMediaID& source);

  const scoped_ptr<class ContentVideoCaptureDeviceCore> core_;

  DISALLOW_COPY_AND_ASSIGN(DesktopCaptureDeviceAura);
};


}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_CAPTURE_DEVICE_AURA_H_
