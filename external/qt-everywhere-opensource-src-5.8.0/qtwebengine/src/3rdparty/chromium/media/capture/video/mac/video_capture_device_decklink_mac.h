// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of VideoCaptureDevice class for Blackmagic video capture
// devices by using the DeckLink SDK.

#ifndef MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_DECKLINK_MAC_H_
#define MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_DECKLINK_MAC_H_

#include "media/capture/video/video_capture_device.h"

#import <Foundation/Foundation.h>
#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"

namespace {
class DeckLinkCaptureDelegate;
}  // namespace

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

namespace media {

// Extension of VideoCaptureDevice to create and manipulate Blackmagic devices.
// Creates a reference counted |decklink_capture_delegate_| that does all the
// DeckLink SDK configuration and capture work while holding a weak reference to
// us for sending back frames, logs and error messages.
class CAPTURE_EXPORT VideoCaptureDeviceDeckLinkMac : public VideoCaptureDevice {
 public:
  // Gets the names of all DeckLink video capture devices connected to this
  // computer, as enumerated by the DeckLink SDK. To allow the user to choose
  // exactly which capture format she wants, we enumerate as many cameras as
  // capture formats.
  static void EnumerateDevices(VideoCaptureDevice::Names* device_names);

  // Gets the supported formats of a particular device attached to the system,
  // identified by |device|. Formats are retrieved from the DeckLink SDK.
  // Following the enumeration, each camera will have only one capability.
  static void EnumerateDeviceCapabilities(
      const VideoCaptureDevice::Name& device,
      VideoCaptureFormats* supported_formats);

  explicit VideoCaptureDeviceDeckLinkMac(const Name& device_name);
  ~VideoCaptureDeviceDeckLinkMac() override;

  // Copy of VideoCaptureDevice::Client::OnIncomingCapturedData(). Used by
  // |decklink_capture_delegate_| to forward captured frames.
  void OnIncomingCapturedData(const uint8_t* data,
                              size_t length,
                              const VideoCaptureFormat& frame_format,
                              int rotation,  // Clockwise.
                              base::TimeTicks reference_time,
                              base::TimeDelta timestamp);

  // Forwarder to VideoCaptureDevice::Client::OnError().
  void SendErrorString(const tracked_objects::Location& from_here,
                       const std::string& reason);

  // Forwarder to VideoCaptureDevice::Client::OnLog().
  void SendLogString(const std::string& message);

 private:
  // VideoCaptureDevice implementation.
  void AllocateAndStart(
      const VideoCaptureParams& params,
      std::unique_ptr<VideoCaptureDevice::Client> client) override;
  void StopAndDeAllocate() override;

  // Protects concurrent setting and using of |client_|.
  base::Lock lock_;
  std::unique_ptr<VideoCaptureDevice::Client> client_;

  // Reference counted handle to the DeckLink capture delegate, ref counted by
  // the DeckLink SDK as well.
  scoped_refptr<DeckLinkCaptureDelegate> decklink_capture_delegate_;

  // Checks for Device (a.k.a. Audio) thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureDeviceDeckLinkMac);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_DECKLINK_MAC_H_
