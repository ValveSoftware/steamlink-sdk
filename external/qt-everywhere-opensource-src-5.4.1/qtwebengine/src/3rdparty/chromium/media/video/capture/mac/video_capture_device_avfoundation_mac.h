// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_MAC_H_
#define MEDIA_VIDEO_CAPTURE_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_MAC_H_

#import <Foundation/Foundation.h>

#import "base/mac/scoped_nsobject.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "media/video/capture/video_capture_device.h"
#include "media/video/capture/video_capture_types.h"
#import "media/video/capture/mac/avfoundation_glue.h"
#import "media/video/capture/mac/platform_video_capturing_mac.h"

namespace media {
class VideoCaptureDeviceMac;
}

@class CrAVCaptureDevice;
@class CrAVCaptureSession;
@class CrAVCaptureVideoDataOutput;

// Class used by VideoCaptureDeviceMac (VCDM) for video capture using
// AVFoundation API. This class lives inside the thread created by its owner
// VCDM.
//
//  * Clients (VCDM) should call +deviceNames to fetch the list of devices
//    available in the system; this method returns the list of device names that
//    have to be used with -setCaptureDevice:.
//  * Previous to any use, clients (VCDM) must call -initWithFrameReceiver: to
//    initialise an object of this class and register a |frameReceiver_|.
//  * Frame receiver registration or removal can also happen via explicit call
//    to -setFrameReceiver:. Re-registrations are safe and allowed, even during
//    capture using this method.
//  * Method -setCaptureDevice: must be called at least once with a device
//    identifier from +deviceNames. Creates all the necessary AVFoundation
//    objects on first call; it connects them ready for capture every time.
//    This method should not be called during capture (i.e. between
//    -startCapture and -stopCapture).
//  * -setCaptureWidth:height:frameRate: is called if a resolution or frame rate
//    different than the by default one set by -setCaptureDevice: is needed.
//    This method should not be called during capture. This method must be
//    called after -setCaptureDevice:.
//  * -startCapture registers the notification listeners and starts the
//    capture. The capture can be stop using -stopCapture. The capture can be
//    restarted and restoped multiple times, reconfiguring or not the device in
//    between.
//  * -setCaptureDevice can be called with a |nil| value, case in which it stops
//    the capture and disconnects the library objects. This step is not
//    necessary.
//  * Deallocation of the library objects happens gracefully on destruction of
//    the VideoCaptureDeviceAVFoundation object.
//
//
@interface VideoCaptureDeviceAVFoundation
    : NSObject<CrAVCaptureVideoDataOutputSampleBufferDelegate,
               PlatformVideoCapturingMac> {
 @private
  // The following attributes are set via -setCaptureHeight:width:frameRate:.
  int frameWidth_;
  int frameHeight_;
  int frameRate_;

  base::Lock lock_;  // Protects concurrent setting and using of frameReceiver_.
  media::VideoCaptureDeviceMac* frameReceiver_;  // weak.

  base::scoped_nsobject<CrAVCaptureSession> captureSession_;

  // |captureDevice_| is an object coming from AVFoundation, used only to be
  // plugged in |captureDeviceInput_| and to query for session preset support.
  CrAVCaptureDevice* captureDevice_;
  // |captureDeviceInput_| is owned by |captureSession_|.
  CrAVCaptureDeviceInput* captureDeviceInput_;
  base::scoped_nsobject<CrAVCaptureVideoDataOutput> captureVideoDataOutput_;

  base::ThreadChecker main_thread_checker_;
  base::ThreadChecker callback_thread_checker_;
}

// Returns a dictionary of capture devices with friendly name and unique id.
+ (NSDictionary*)deviceNames;

// Retrieve the capture supported formats for a given device |name|.
+ (void)getDevice:(const media::VideoCaptureDevice::Name&)name
    supportedFormats:(media::VideoCaptureFormats*)formats;

// Initializes the instance and the underlying capture session and registers the
// frame receiver.
- (id)initWithFrameReceiver:(media::VideoCaptureDeviceMac*)frameReceiver;

// Sets the frame receiver.
- (void)setFrameReceiver:(media::VideoCaptureDeviceMac*)frameReceiver;

// Sets which capture device to use by name, retrieved via |deviceNames|. Once
// the deviceId is known, the library objects are created if needed and
// connected for the capture, and a by default resolution is set. If deviceId is
// nil, then the eventual capture is stopped and library objects are
// disconnected. Returns YES on sucess, NO otherwise. This method should not be
// called during capture.
- (BOOL)setCaptureDevice:(NSString*)deviceId;

// Configures the capture properties for the capture session and the video data
// output; this means it MUST be called after setCaptureDevice:. Return YES on
// success, else NO.
- (BOOL)setCaptureHeight:(int)height width:(int)width frameRate:(int)frameRate;

// Starts video capturing and register the notification listeners. Must be
// called after setCaptureDevice:, and, eventually, also after
// setCaptureHeight:width:frameRate:. Returns YES on sucess, NO otherwise.
- (BOOL)startCapture;

// Stops video capturing and stops listening to notifications.
- (void)stopCapture;

@end

#endif  // MEDIA_VIDEO_CAPTURE_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_MAC_H_
