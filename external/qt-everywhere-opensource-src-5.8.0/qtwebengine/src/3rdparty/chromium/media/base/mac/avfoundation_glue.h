// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// AVFoundation API is only introduced in Mac OS X > 10.6, and there is only one
// build of Chromium, so the (potential) linking with AVFoundation has to happen
// in runtime. For this to be clean, an AVFoundationGlue class is defined to try
// and load these AVFoundation system libraries. If it succeeds, subsequent
// clients can use AVFoundation via the rest of the classes declared in this
// file.

#ifndef MEDIA_BASE_MAC_AVFOUNDATION_GLUE_H_
#define MEDIA_BASE_MAC_AVFOUNDATION_GLUE_H_

#if defined(__OBJC__)
#import <Foundation/Foundation.h>
#endif  // defined(__OBJC__)

#include <stdint.h>

#include "base/macros.h"
#include "media/base/mac/coremedia_glue.h"
#include "media/base/media_export.h"

class MEDIA_EXPORT AVFoundationGlue {
 public:
  // Must be called on the UI thread prior to attempting to use any other
  // AVFoundation methods.
  static void InitializeAVFoundation();

#if defined(__OBJC__)
  static NSBundle const* AVFoundationBundle();

  // Originally coming from AVCaptureDevice.h but in global namespace.
  static NSString* AVCaptureDeviceWasConnectedNotification();
  static NSString* AVCaptureDeviceWasDisconnectedNotification();

  // Originally coming from AVMediaFormat.h but in global namespace.
  static NSString* AVMediaTypeVideo();
  static NSString* AVMediaTypeAudio();
  static NSString* AVMediaTypeMuxed();

  // Originally from AVCaptureSession.h but in global namespace.
  static NSString* AVCaptureSessionRuntimeErrorNotification();
  static NSString* AVCaptureSessionDidStopRunningNotification();
  static NSString* AVCaptureSessionErrorKey();

  // Originally from AVVideoSettings.h but in global namespace.
  static NSString* AVVideoScalingModeKey();
  static NSString* AVVideoScalingModeResizeAspectFill();

  static Class AVCaptureSessionClass();
  static Class AVCaptureVideoDataOutputClass();
#endif  // defined(__OBJC__)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AVFoundationGlue);
};

#if defined(__OBJC__)

// Originally AVCaptureDevice and coming from AVCaptureDevice.h
MEDIA_EXPORT
@interface CrAVCaptureDevice : NSObject

- (BOOL)hasMediaType:(NSString*)mediaType;
- (NSString*)uniqueID;
- (NSString*)localizedName;
- (BOOL)isSuspended;
- (NSArray*)formats;
- (int32_t)transportType;

@end

// Originally AVCaptureDeviceFormat and coming from AVCaptureDevice.h.
MEDIA_EXPORT
@interface CrAVCaptureDeviceFormat : NSObject

- (CoreMediaGlue::CMFormatDescriptionRef)formatDescription;
- (NSArray*)videoSupportedFrameRateRanges;

@end

// Originally AVFrameRateRange and coming from AVCaptureDevice.h.
MEDIA_EXPORT
@interface CrAVFrameRateRange : NSObject

- (Float64)maxFrameRate;

@end

MEDIA_EXPORT
@interface CrAVCaptureInput : NSObject  // Originally from AVCaptureInput.h.
@end

MEDIA_EXPORT
@interface CrAVCaptureOutput : NSObject  // Originally from AVCaptureOutput.h.
@end

// Originally AVCaptureSession and coming from AVCaptureSession.h.
MEDIA_EXPORT
@interface CrAVCaptureSession : NSObject

- (void)release;
- (void)addInput:(CrAVCaptureInput*)input;
- (void)removeInput:(CrAVCaptureInput*)input;
- (void)addOutput:(CrAVCaptureOutput*)output;
- (void)removeOutput:(CrAVCaptureOutput*)output;
- (BOOL)isRunning;
- (void)startRunning;
- (void)stopRunning;

@end

// Originally AVCaptureConnection and coming from AVCaptureSession.h.
MEDIA_EXPORT
@interface CrAVCaptureConnection : NSObject

- (BOOL)isVideoMinFrameDurationSupported;
- (void)setVideoMinFrameDuration:(CoreMediaGlue::CMTime)minFrameRate;
- (BOOL)isVideoMaxFrameDurationSupported;
- (void)setVideoMaxFrameDuration:(CoreMediaGlue::CMTime)maxFrameRate;

@end

// Originally AVCaptureDeviceInput and coming from AVCaptureInput.h.
MEDIA_EXPORT
@interface CrAVCaptureDeviceInput : CrAVCaptureInput

@end

// Originally AVCaptureVideoDataOutputSampleBufferDelegate from
// AVCaptureOutput.h.
@protocol CrAVCaptureVideoDataOutputSampleBufferDelegate <NSObject>

@optional

- (void)captureOutput:(CrAVCaptureOutput*)captureOutput
didOutputSampleBuffer:(CoreMediaGlue::CMSampleBufferRef)sampleBuffer
       fromConnection:(CrAVCaptureConnection*)connection;

@end

// Originally AVCaptureVideoDataOutput and coming from AVCaptureOutput.h.
MEDIA_EXPORT
@interface CrAVCaptureVideoDataOutput : CrAVCaptureOutput

- (oneway void)release;
- (void)setSampleBufferDelegate:(id)sampleBufferDelegate
                          queue:(dispatch_queue_t)sampleBufferCallbackQueue;

- (void)setAlwaysDiscardsLateVideoFrames:(BOOL)flag;
- (void)setVideoSettings:(NSDictionary*)videoSettings;
- (NSDictionary*)videoSettings;
- (CrAVCaptureConnection*)connectionWithMediaType:(NSString*)mediaType;

@end

// Class to provide access to class methods of AVCaptureDevice.
MEDIA_EXPORT
@interface AVCaptureDeviceGlue : NSObject

+ (NSArray*)devices;

+ (CrAVCaptureDevice*)deviceWithUniqueID:(NSString*)deviceUniqueID;

@end

// Class to provide access to class methods of AVCaptureDeviceInput.
MEDIA_EXPORT
@interface AVCaptureDeviceInputGlue : NSObject

+ (CrAVCaptureDeviceInput*)deviceInputWithDevice:(CrAVCaptureDevice*)device
                                           error:(NSError**)outError;

@end

#endif  // defined(__OBJC__)

#endif  // MEDIA_BASE_MAC_AVFOUNDATION_GLUE_H_
