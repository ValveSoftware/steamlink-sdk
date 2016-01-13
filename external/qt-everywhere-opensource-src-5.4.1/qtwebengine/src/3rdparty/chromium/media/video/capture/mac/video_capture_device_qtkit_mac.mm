// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "media/video/capture/mac/video_capture_device_qtkit_mac.h"

#import <QTKit/QTKit.h>

#include "base/debug/crash_logging.h"
#include "base/logging.h"
#include "base/mac/scoped_nsexception_enabler.h"
#include "media/video/capture/mac/video_capture_device_mac.h"
#include "media/video/capture/video_capture_device.h"
#include "media/video/capture/video_capture_types.h"
#include "ui/gfx/size.h"

@implementation VideoCaptureDeviceQTKit

#pragma mark Class methods

+ (void)getDeviceNames:(NSMutableDictionary*)deviceNames {
  // Third-party drivers often throw exceptions, which are fatal in
  // Chromium (see comments in scoped_nsexception_enabler.h).  The
  // following catches any exceptions and continues in an orderly
  // fashion with no devices detected.
  NSArray* captureDevices =
      base::mac::RunBlockIgnoringExceptions(^{
          return [QTCaptureDevice inputDevicesWithMediaType:QTMediaTypeVideo];
      });

  for (QTCaptureDevice* device in captureDevices) {
    if (![[device attributeForKey:QTCaptureDeviceSuspendedAttribute] boolValue])
      [deviceNames setObject:[device localizedDisplayName]
                      forKey:[device uniqueID]];
  }
}

+ (NSDictionary*)deviceNames {
  NSMutableDictionary* deviceNames =
      [[[NSMutableDictionary alloc] init] autorelease];

  // TODO(shess): Post to the main thread to see if that helps
  // http://crbug.com/139164
  [self performSelectorOnMainThread:@selector(getDeviceNames:)
                         withObject:deviceNames
                      waitUntilDone:YES];
  return deviceNames;
}

#pragma mark Public methods

- (id)initWithFrameReceiver:(media::VideoCaptureDeviceMac*)frameReceiver {
  self = [super init];
  if (self) {
    frameReceiver_ = frameReceiver;
    lock_ = [[NSLock alloc] init];
  }
  return self;
}

- (void)dealloc {
  [captureSession_ release];
  [captureDeviceInput_ release];
  [super dealloc];
}

- (void)setFrameReceiver:(media::VideoCaptureDeviceMac*)frameReceiver {
  [lock_ lock];
  frameReceiver_ = frameReceiver;
  [lock_ unlock];
}

- (BOOL)setCaptureDevice:(NSString*)deviceId {
  if (deviceId) {
    // Set the capture device.
    if (captureDeviceInput_) {
      DLOG(ERROR) << "Video capture device already set.";
      return NO;
    }

    // TODO(mcasas): Consider using [QTCaptureDevice deviceWithUniqueID] instead
    // of explicitly forcing reenumeration of devices.
    NSArray *captureDevices =
        [QTCaptureDevice inputDevicesWithMediaType:QTMediaTypeVideo];
    NSArray *captureDevicesNames =
        [captureDevices valueForKey:@"uniqueID"];
    NSUInteger index = [captureDevicesNames indexOfObject:deviceId];
    if (index == NSNotFound) {
      [self sendErrorString:[NSString
        stringWithUTF8String:"Video capture device not found."]];
      return NO;
    }
    QTCaptureDevice *device = [captureDevices objectAtIndex:index];
    if ([[device attributeForKey:QTCaptureDeviceSuspendedAttribute]
            boolValue]) {
      [self sendErrorString:[NSString
        stringWithUTF8String:"Cannot open suspended video capture device."]];
      return NO;
    }
    NSError *error;
    if (![device open:&error]) {
      [self sendErrorString:[NSString
          stringWithFormat:@"Could not open video capture device (%@): %@",
                           [error localizedDescription],
                           [error localizedFailureReason]]];
      return NO;
    }
    captureDeviceInput_ = [[QTCaptureDeviceInput alloc] initWithDevice:device];
    captureSession_ = [[QTCaptureSession alloc] init];

    QTCaptureDecompressedVideoOutput *captureDecompressedOutput =
        [[[QTCaptureDecompressedVideoOutput alloc] init] autorelease];
    [captureDecompressedOutput setDelegate:self];
    if (![captureSession_ addOutput:captureDecompressedOutput error:&error]) {
      [self sendErrorString:[NSString
          stringWithFormat:@"Could not connect video capture output (%@): %@",
                           [error localizedDescription],
                           [error localizedFailureReason]]];
      return NO;
    }

    // This key can be used to check if video capture code was related to a
    // particular crash.
    base::debug::SetCrashKeyValue("VideoCaptureDeviceQTKit", "OpenedDevice");

    // Set the video pixel format to 2VUY (a.k.a UYVY, packed 4:2:2).
    NSDictionary *captureDictionary = [NSDictionary
        dictionaryWithObject:
            [NSNumber numberWithUnsignedInt:kCVPixelFormatType_422YpCbCr8]
                      forKey:(id)kCVPixelBufferPixelFormatTypeKey];
    [captureDecompressedOutput setPixelBufferAttributes:captureDictionary];

    return YES;
  } else {
    // Remove the previously set capture device.
    if (!captureDeviceInput_) {
      [self sendErrorString:[NSString
          stringWithUTF8String:"No video capture device set, on removal."]];
      return YES;
    }
    if ([[captureSession_ inputs] count] > 0) {
      // The device is still running.
      [self stopCapture];
    }
    if ([[captureSession_ outputs] count] > 0) {
      // Only one output is set for |captureSession_|.
      DCHECK_EQ([[captureSession_ outputs] count], 1u);
      id output = [[captureSession_ outputs] objectAtIndex:0];
      [output setDelegate:nil];

      // TODO(shess): QTKit achieves thread safety by posting messages to the
      // main thread.  As part of -addOutput:, it posts a message to the main
      // thread which in turn posts a notification which will run in a future
      // spin after the original method returns.  -removeOutput: can post a
      // main-thread message in between while holding a lock which the
      // notification handler  will need.  Posting either -addOutput: or
      // -removeOutput: to the main thread should fix it, remove is likely
      // safer. http://crbug.com/152757
      [captureSession_ performSelectorOnMainThread:@selector(removeOutput:)
                                        withObject:output
                                     waitUntilDone:YES];
    }
    [captureSession_ release];
    captureSession_ = nil;
    [captureDeviceInput_ release];
    captureDeviceInput_ = nil;
    return YES;
  }
}

- (BOOL)setCaptureHeight:(int)height width:(int)width frameRate:(int)frameRate {
  if (!captureDeviceInput_) {
    [self sendErrorString:[NSString
        stringWithUTF8String:"No video capture device set."]];
    return NO;
  }
  if ([[captureSession_ outputs] count] != 1) {
    [self sendErrorString:[NSString
        stringWithUTF8String:"Video capture capabilities already set."]];
    return NO;
  }
  if (frameRate <= 0) {
    [self sendErrorString:[NSString stringWithUTF8String: "Wrong frame rate."]];
    return NO;
  }

  frameRate_ = frameRate;

  QTCaptureDecompressedVideoOutput *output =
      [[captureSession_ outputs] objectAtIndex:0];

  // Set up desired output properties. The old capture dictionary is used to
  // retrieve the initial pixel format, which must be maintained.
  NSDictionary* videoSettingsDictionary = @{
    (id)kCVPixelBufferWidthKey : @(width),
    (id)kCVPixelBufferHeightKey : @(height),
    (id)kCVPixelBufferPixelFormatTypeKey : [[output pixelBufferAttributes]
        valueForKey:(id)kCVPixelBufferPixelFormatTypeKey]
  };
  [output setPixelBufferAttributes:videoSettingsDictionary];

  [output setMinimumVideoFrameInterval:(NSTimeInterval)1/(float)frameRate];
  return YES;
}

- (BOOL)startCapture {
  if ([[captureSession_ outputs] count] == 0) {
    // Capture properties not set.
    [self sendErrorString:[NSString
        stringWithUTF8String:"Video capture device not initialized."]];
    return NO;
  }
  if ([[captureSession_ inputs] count] == 0) {
    NSError *error;
    if (![captureSession_ addInput:captureDeviceInput_ error:&error]) {
      [self sendErrorString:[NSString
          stringWithFormat:@"Could not connect video capture device (%@): %@",
                           [error localizedDescription],
                           [error localizedFailureReason]]];

      return NO;
    }
    NSNotificationCenter * notificationCenter =
        [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self
                           selector:@selector(handleNotification:)
                               name:QTCaptureSessionRuntimeErrorNotification
                             object:captureSession_];
    [captureSession_ startRunning];
  }
  return YES;
}

- (void)stopCapture {
  if ([[captureSession_ inputs] count] == 1) {
    [captureSession_ removeInput:captureDeviceInput_];
    [captureSession_ stopRunning];
  }

  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

// |captureOutput| is called by the capture device to deliver a new frame.
- (void)captureOutput:(QTCaptureOutput*)captureOutput
  didOutputVideoFrame:(CVImageBufferRef)videoFrame
     withSampleBuffer:(QTSampleBuffer*)sampleBuffer
       fromConnection:(QTCaptureConnection*)connection {
  [lock_ lock];
  if(!frameReceiver_) {
    [lock_ unlock];
    return;
  }

  // Lock the frame and calculate frame size.
  const int kLockFlags = 0;
  if (CVPixelBufferLockBaseAddress(videoFrame, kLockFlags)
      == kCVReturnSuccess) {
    void *baseAddress = CVPixelBufferGetBaseAddress(videoFrame);
    size_t bytesPerRow = CVPixelBufferGetBytesPerRow(videoFrame);
    size_t frameWidth = CVPixelBufferGetWidth(videoFrame);
    size_t frameHeight = CVPixelBufferGetHeight(videoFrame);
    size_t frameSize = bytesPerRow * frameHeight;

    // TODO(shess): bytesPerRow may not correspond to frameWidth_*2,
    // but VideoCaptureController::OnIncomingCapturedData() requires
    // it to do so.  Plumbing things through is intrusive, for now
    // just deliver an adjusted buffer.
    // TODO(nick): This workaround could probably be eliminated by using
    // VideoCaptureController::OnIncomingCapturedVideoFrame, which supports
    // pitches.
    UInt8* addressToPass = static_cast<UInt8*>(baseAddress);
    // UYVY is 2 bytes per pixel.
    size_t expectedBytesPerRow = frameWidth * 2;
    if (bytesPerRow > expectedBytesPerRow) {
      // TODO(shess): frameHeight and frameHeight_ are not the same,
      // try to do what the surrounding code seems to assume.
      // Ironically, captureCapability and frameSize are ignored
      // anyhow.
      adjustedFrame_.resize(expectedBytesPerRow * frameHeight);
      // std::vector is contiguous according to standard.
      UInt8* adjustedAddress = &adjustedFrame_[0];

      for (size_t y = 0; y < frameHeight; ++y) {
        memcpy(adjustedAddress + y * expectedBytesPerRow,
               addressToPass + y * bytesPerRow,
               expectedBytesPerRow);
      }

      addressToPass = adjustedAddress;
      frameSize = frameHeight * expectedBytesPerRow;
    }

    media::VideoCaptureFormat captureFormat(gfx::Size(frameWidth, frameHeight),
                                            frameRate_,
                                            media::PIXEL_FORMAT_UYVY);

    // The aspect ratio dictionary is often missing, in which case we report
    // a pixel aspect ratio of 0:0.
    int aspectNumerator = 0, aspectDenominator = 0;
    CFDictionaryRef aspectRatioDict = (CFDictionaryRef)CVBufferGetAttachment(
        videoFrame, kCVImageBufferPixelAspectRatioKey, NULL);
    if (aspectRatioDict) {
      CFNumberRef aspectNumeratorRef = (CFNumberRef)CFDictionaryGetValue(
          aspectRatioDict, kCVImageBufferPixelAspectRatioHorizontalSpacingKey);
      CFNumberRef aspectDenominatorRef = (CFNumberRef)CFDictionaryGetValue(
          aspectRatioDict, kCVImageBufferPixelAspectRatioVerticalSpacingKey);
      DCHECK(aspectNumeratorRef && aspectDenominatorRef) <<
          "Aspect Ratio dictionary missing its entries.";
      CFNumberGetValue(aspectNumeratorRef, kCFNumberIntType, &aspectNumerator);
      CFNumberGetValue(
          aspectDenominatorRef, kCFNumberIntType, &aspectDenominator);
    }

    // Deliver the captured video frame.
    frameReceiver_->ReceiveFrame(addressToPass, frameSize, captureFormat,
        aspectNumerator, aspectDenominator);

    CVPixelBufferUnlockBaseAddress(videoFrame, kLockFlags);
  }
  [lock_ unlock];
}

- (void)handleNotification:(NSNotification*)errorNotification {
  NSError * error = (NSError*)[[errorNotification userInfo]
      objectForKey:QTCaptureSessionErrorKey];
  [self sendErrorString:[NSString
      stringWithFormat:@"%@: %@",
                       [error localizedDescription],
                       [error localizedFailureReason]]];
}

- (void)sendErrorString:(NSString*)error {
  DLOG(ERROR) << [error UTF8String];
  [lock_ lock];
  if (frameReceiver_)
    frameReceiver_->ReceiveError([error UTF8String]);
  [lock_ unlock];
}

@end
