// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// VideoCaptureDeviceQTKit implements all QTKit related code for
// communicating with a QTKit capture device.

#ifndef MEDIA_VIDEO_CAPTURE_MAC_VIDEO_CAPTURE_DEVICE_QTKIT_MAC_H_
#define MEDIA_VIDEO_CAPTURE_MAC_VIDEO_CAPTURE_DEVICE_QTKIT_MAC_H_

#import <Foundation/Foundation.h>

#include <vector>

#import "media/video/capture/mac/platform_video_capturing_mac.h"

namespace media {
class VideoCaptureDeviceMac;
}

@class QTCaptureDeviceInput;
@class QTCaptureSession;

@interface VideoCaptureDeviceQTKit : NSObject<PlatformVideoCapturingMac> {
 @private
  // Settings.
  int frameRate_;

  NSLock *lock_;
  media::VideoCaptureDeviceMac *frameReceiver_;

  // QTKit variables.
  QTCaptureSession *captureSession_;
  QTCaptureDeviceInput *captureDeviceInput_;

  // Buffer for adjusting frames which do not fit receiver
  // assumptions.  scoped_array<> might make more sense, if the size
  // can be proven invariant.
  std::vector<UInt8> adjustedFrame_;
}

// Fills up the |deviceNames| dictionary of capture devices with friendly name
// and unique id. No thread assumptions, but this method should run in UI
// thread, see http://crbug.com/139164
+ (void)getDeviceNames:(NSMutableDictionary*)deviceNames;

// Returns a dictionary of capture devices with friendly name and unique id, via
// runing +getDeviceNames: on Main Thread.
+ (NSDictionary*)deviceNames;

// Initializes the instance and registers the frame receiver.
- (id)initWithFrameReceiver:(media::VideoCaptureDeviceMac*)frameReceiver;

// Set the frame receiver.
- (void)setFrameReceiver:(media::VideoCaptureDeviceMac*)frameReceiver;

// Sets which capture device to use. Returns YES on sucess, NO otherwise.
- (BOOL)setCaptureDevice:(NSString*)deviceId;

// Configures the capture properties.
- (BOOL)setCaptureHeight:(int)height width:(int)width frameRate:(int)frameRate;

// Start video capturing. Returns YES on sucess, NO otherwise.
- (BOOL)startCapture;

// Stops video capturing.
- (void)stopCapture;

// Handle any QTCaptureSessionRuntimeErrorNotifications.
- (void)handleNotification:(NSNotification*)errorNotification;

@end

#endif  // MEDIA_VIDEO_CAPTURE_MAC_VIDEO_CAPTURE_DEVICE_QTKIT_MAC_H_
