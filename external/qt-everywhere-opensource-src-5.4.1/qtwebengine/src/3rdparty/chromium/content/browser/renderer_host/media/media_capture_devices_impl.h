// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_CAPTURE_DEVICES_H
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_CAPTURE_DEVICES_H

#include "base/memory/singleton.h"
#include "content/public/browser/media_capture_devices.h"

namespace content {

class MediaCaptureDevicesImpl : public MediaCaptureDevices {
 public:
  static MediaCaptureDevicesImpl* GetInstance();

  // Overriden from MediaCaptureDevices
  virtual const MediaStreamDevices& GetAudioCaptureDevices() OVERRIDE;
  virtual const MediaStreamDevices& GetVideoCaptureDevices() OVERRIDE;

  // Called by MediaStreamManager to notify the change of media capture
  // devices, these 2 methods are called in IO thread.
  void OnAudioCaptureDevicesChanged(const MediaStreamDevices& devices);
  void OnVideoCaptureDevicesChanged(const MediaStreamDevices& devices);

 private:
  friend struct DefaultSingletonTraits<MediaCaptureDevicesImpl>;
  MediaCaptureDevicesImpl();
  virtual ~MediaCaptureDevicesImpl();

  void UpdateAudioDevicesOnUIThread(const content::MediaStreamDevices& devices);
  void UpdateVideoDevicesOnUIThread(const content::MediaStreamDevices& devices);

  // Flag to indicate if device enumeration has been done/doing.
  // Only accessed on UI thread.
  bool devices_enumerated_;

  // A list of cached audio capture devices.
  MediaStreamDevices audio_devices_;

  // A list of cached video capture devices.
  MediaStreamDevices video_devices_;

  DISALLOW_COPY_AND_ASSIGN(MediaCaptureDevicesImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_CAPTURE_DEVICES_H
