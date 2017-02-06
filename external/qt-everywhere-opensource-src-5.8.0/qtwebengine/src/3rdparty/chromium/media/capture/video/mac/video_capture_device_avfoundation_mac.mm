// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "media/capture/video/mac/video_capture_device_avfoundation_mac.h"

#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#include <stddef.h>
#include <stdint.h>

#include "base/location.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/metrics/histogram_macros.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_capture_types.h"
#include "media/capture/video/mac/video_capture_device_mac.h"
#include "ui/gfx/geometry/size.h"

// Prefer MJPEG if frame width or height is larger than this.
static const int kMjpegWidthThreshold = 640;
static const int kMjpegHeightThreshold = 480;

namespace {

enum MacBookVersions {
  OTHER = 0,
  MACBOOK_5,  // MacBook5.X
  MACBOOK_6,
  MACBOOK_7,
  MACBOOK_8,
  MACBOOK_PRO_11,  // MacBookPro11.X
  MACBOOK_PRO_12,
  MACBOOK_PRO_13,
  MACBOOK_AIR_5,  // MacBookAir5.X
  MACBOOK_AIR_6,
  MACBOOK_AIR_7,
  MACBOOK_AIR_8,
  MACBOOK_AIR_3,
  MACBOOK_AIR_4,
  MACBOOK_4,
  MACBOOK_9,
  MACBOOK_10,
  MACBOOK_PRO_10,
  MACBOOK_PRO_9,
  MACBOOK_PRO_8,
  MACBOOK_PRO_7,
  MACBOOK_PRO_6,
  MACBOOK_PRO_5,
  MAX_MACBOOK_VERSION = MACBOOK_PRO_5
};

MacBookVersions GetMacBookModel(const std::string& model) {
  struct {
    const char* name;
    MacBookVersions version;
  } static const kModelToVersion[] = {
      {"MacBook4,", MACBOOK_4},          {"MacBook5,", MACBOOK_5},
      {"MacBook6,", MACBOOK_6},          {"MacBook7,", MACBOOK_7},
      {"MacBook8,", MACBOOK_8},          {"MacBook9,", MACBOOK_9},
      {"MacBook10,", MACBOOK_10},        {"MacBookPro5,", MACBOOK_PRO_5},
      {"MacBookPro6,", MACBOOK_PRO_6},   {"MacBookPro7,", MACBOOK_PRO_7},
      {"MacBookPro8,", MACBOOK_PRO_8},   {"MacBookPro9,", MACBOOK_PRO_9},
      {"MacBookPro10,", MACBOOK_PRO_10}, {"MacBookPro11,", MACBOOK_PRO_11},
      {"MacBookPro12,", MACBOOK_PRO_12}, {"MacBookPro13,", MACBOOK_PRO_13},
      {"MacBookAir3,", MACBOOK_AIR_3},   {"MacBookAir4,", MACBOOK_AIR_4},
      {"MacBookAir5,", MACBOOK_AIR_5},   {"MacBookAir6,", MACBOOK_AIR_6},
      {"MacBookAir7,", MACBOOK_AIR_7},   {"MacBookAir8,", MACBOOK_AIR_8},
  };

  for (const auto& entry : kModelToVersion) {
    if (base::StartsWith(model, entry.name,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      return entry.version;
    }
  }
  return OTHER;
}

// Add Uma stats for number of detected devices on MacBooks. These are used for
// investigating crbug/582931.
void MaybeWriteUma(int number_of_devices, int number_of_suspended_devices) {
  std::string model = base::mac::GetModelIdentifier();
  if (base::StartsWith(model, "MacBook",
                       base::CompareCase::INSENSITIVE_ASCII)) {
    UMA_HISTOGRAM_COUNTS("Media.VideoCapture.MacBook.NumberOfDevices",
                         number_of_devices + number_of_suspended_devices);
    if (number_of_devices + number_of_suspended_devices == 0) {
      UMA_HISTOGRAM_ENUMERATION(
          "Media.VideoCapture.MacBook.HardwareVersionWhenNoCamera",
          GetMacBookModel(model), MAX_MACBOOK_VERSION + 1);
    }
  }
}

}  // anonymous namespace

// This function translates Mac Core Video pixel formats to Chromium pixel
// formats.
media::VideoPixelFormat FourCCToChromiumPixelFormat(FourCharCode code) {
  switch (code) {
    case kCVPixelFormatType_422YpCbCr8:
      return media::PIXEL_FORMAT_UYVY;
    case CoreMediaGlue::kCMPixelFormat_422YpCbCr8_yuvs:
      return media::PIXEL_FORMAT_YUY2;
    case CoreMediaGlue::kCMVideoCodecType_JPEG_OpenDML:
      return media::PIXEL_FORMAT_MJPEG;
    default:
      return media::PIXEL_FORMAT_UNKNOWN;
  }
}

@implementation VideoCaptureDeviceAVFoundation

#pragma mark Class methods

+ (void)getDeviceNames:(NSMutableDictionary*)deviceNames {
  // At this stage we already know that AVFoundation is supported and the whole
  // library is loaded and initialised, by the device monitoring.
  NSArray* devices = [AVCaptureDeviceGlue devices];
  int number_of_suspended_devices = 0;
  for (CrAVCaptureDevice* device in devices) {
    if ([device hasMediaType:AVFoundationGlue::AVMediaTypeVideo()] ||
        [device hasMediaType:AVFoundationGlue::AVMediaTypeMuxed()]) {
      if ([device isSuspended]) {
        ++number_of_suspended_devices;
        continue;
      }
      DeviceNameAndTransportType* nameAndTransportType =
          [[[DeviceNameAndTransportType alloc]
               initWithName:[device localizedName]
              transportType:[device transportType]] autorelease];
      [deviceNames setObject:nameAndTransportType forKey:[device uniqueID]];
    }
  }
  MaybeWriteUma([deviceNames count], number_of_suspended_devices);
}

+ (NSDictionary*)deviceNames {
  NSMutableDictionary* deviceNames =
      [[[NSMutableDictionary alloc] init] autorelease];
  // The device name retrieval is not going to happen in the main thread, and
  // this might cause instabilities (it did in QTKit), so keep an eye here.
  [self getDeviceNames:deviceNames];
  return deviceNames;
}

+ (void)getDevice:(const media::VideoCaptureDevice::Name&)name
 supportedFormats:(media::VideoCaptureFormats*)formats {
  NSArray* devices = [AVCaptureDeviceGlue devices];
  CrAVCaptureDevice* device = nil;
  for (device in devices) {
    if ([[device uniqueID] UTF8String] == name.id())
      break;
  }
  if (device == nil)
    return;
  for (CrAVCaptureDeviceFormat* format in device.formats) {
    // MediaSubType is a CMPixelFormatType but can be used as CVPixelFormatType
    // as well according to CMFormatDescription.h
    const media::VideoPixelFormat pixelFormat = FourCCToChromiumPixelFormat(
        CoreMediaGlue::CMFormatDescriptionGetMediaSubType(
            [format formatDescription]));

    CoreMediaGlue::CMVideoDimensions dimensions =
        CoreMediaGlue::CMVideoFormatDescriptionGetDimensions(
            [format formatDescription]);

    for (CrAVFrameRateRange* frameRate in
         [format videoSupportedFrameRateRanges]) {
      media::VideoCaptureFormat format(
          gfx::Size(dimensions.width, dimensions.height),
          frameRate.maxFrameRate, pixelFormat);
      formats->push_back(format);
      DVLOG(2) << name.name() << " "
               << media::VideoCaptureFormat::ToString(format);
    }
  }
}

#pragma mark Public methods

- (id)initWithFrameReceiver:(media::VideoCaptureDeviceMac*)frameReceiver {
  if ((self = [super init])) {
    DCHECK(main_thread_checker_.CalledOnValidThread());
    DCHECK(frameReceiver);
    [self setFrameReceiver:frameReceiver];
    captureSession_.reset(
        [[AVFoundationGlue::AVCaptureSessionClass() alloc] init]);
  }
  return self;
}

- (void)dealloc {
  [self stopCapture];
  [super dealloc];
}

- (void)setFrameReceiver:(media::VideoCaptureDeviceMac*)frameReceiver {
  base::AutoLock lock(lock_);
  frameReceiver_ = frameReceiver;
}

- (BOOL)setCaptureDevice:(NSString*)deviceId {
  DCHECK(captureSession_);
  DCHECK(main_thread_checker_.CalledOnValidThread());

  if (!deviceId) {
    // First stop the capture session, if it's running.
    [self stopCapture];
    // Now remove the input and output from the capture session.
    [captureSession_ removeOutput:captureVideoDataOutput_];
    if (captureDeviceInput_) {
      [captureSession_ removeInput:captureDeviceInput_];
      // No need to release |captureDeviceInput_|, is owned by the session.
      captureDeviceInput_ = nil;
    }
    return YES;
  }

  // Look for input device with requested name.
  captureDevice_ = [AVCaptureDeviceGlue deviceWithUniqueID:deviceId];
  if (!captureDevice_) {
    [self
        sendErrorString:[NSString stringWithUTF8String:
                                      "Could not open video capture device."]];
    return NO;
  }

  // Create the capture input associated with the device. Easy peasy.
  NSError* error = nil;
  captureDeviceInput_ =
      [AVCaptureDeviceInputGlue deviceInputWithDevice:captureDevice_
                                                error:&error];
  if (!captureDeviceInput_) {
    captureDevice_ = nil;
    [self sendErrorString:
              [NSString stringWithFormat:
                            @"Could not create video capture input (%@): %@",
                            [error localizedDescription],
                            [error localizedFailureReason]]];
    return NO;
  }
  [captureSession_ addInput:captureDeviceInput_];

  // Create a new data output for video. The data output is configured to
  // discard late frames by default.
  captureVideoDataOutput_.reset(
      [[AVFoundationGlue::AVCaptureVideoDataOutputClass() alloc] init]);
  if (!captureVideoDataOutput_) {
    [captureSession_ removeInput:captureDeviceInput_];
    [self sendErrorString:[NSString stringWithUTF8String:
                                        "Could not create video data output."]];
    return NO;
  }
  [captureVideoDataOutput_ setAlwaysDiscardsLateVideoFrames:true];
  [captureVideoDataOutput_
      setSampleBufferDelegate:self
                        queue:dispatch_get_global_queue(
                                  DISPATCH_QUEUE_PRIORITY_DEFAULT, 0)];
  [captureSession_ addOutput:captureVideoDataOutput_];
  return YES;
}

- (BOOL)setCaptureHeight:(int)height
                   width:(int)width
               frameRate:(float)frameRate {
  DCHECK(![captureSession_ isRunning] &&
         main_thread_checker_.CalledOnValidThread());

  frameWidth_ = width;
  frameHeight_ = height;
  frameRate_ = frameRate;

  FourCharCode best_fourcc = kCVPixelFormatType_422YpCbCr8;
  const bool prefer_mjpeg =
      width > kMjpegWidthThreshold || height > kMjpegHeightThreshold;
  for (CrAVCaptureDeviceFormat* format in captureDevice_.formats) {
    const FourCharCode fourcc =
        CoreMediaGlue::CMFormatDescriptionGetMediaSubType(
            [format formatDescription]);
    if (prefer_mjpeg &&
        fourcc == CoreMediaGlue::kCMVideoCodecType_JPEG_OpenDML) {
      best_fourcc = fourcc;
      break;
    }

    // Compare according to Chromium preference.
    if (media::VideoCaptureFormat::ComparePixelFormatPreference(
            FourCCToChromiumPixelFormat(fourcc),
            FourCCToChromiumPixelFormat(best_fourcc))) {
      best_fourcc = fourcc;
    }
  }

  // The capture output has to be configured, despite Mac documentation
  // detailing that setting the sessionPreset would be enough. The reason for
  // this mismatch is probably because most of the AVFoundation docs are written
  // for iOS and not for MacOsX. AVVideoScalingModeKey() refers to letterboxing
  // yes/no and preserve aspect ratio yes/no when scaling. Currently we set
  // cropping and preservation.
  NSDictionary* videoSettingsDictionary = @{
    (id) kCVPixelBufferWidthKey : @(width), (id)
    kCVPixelBufferHeightKey : @(height), (id)
    kCVPixelBufferPixelFormatTypeKey : @(best_fourcc),
    AVFoundationGlue::AVVideoScalingModeKey() :
        AVFoundationGlue::AVVideoScalingModeResizeAspectFill()
  };
  [captureVideoDataOutput_ setVideoSettings:videoSettingsDictionary];

  CrAVCaptureConnection* captureConnection = [captureVideoDataOutput_
      connectionWithMediaType:AVFoundationGlue::AVMediaTypeVideo()];
  // Check selector existence, related to bugs http://crbug.com/327532 and
  // http://crbug.com/328096.
  // CMTimeMake accepts integer argumenst but |frameRate| is float, round it.
  if ([captureConnection
          respondsToSelector:@selector(isVideoMinFrameDurationSupported)] &&
      [captureConnection isVideoMinFrameDurationSupported]) {
    [captureConnection
        setVideoMinFrameDuration:CoreMediaGlue::CMTimeMake(
                                     media::kFrameRatePrecision,
                                     (int)(frameRate *
                                           media::kFrameRatePrecision))];
  }
  if ([captureConnection
          respondsToSelector:@selector(isVideoMaxFrameDurationSupported)] &&
      [captureConnection isVideoMaxFrameDurationSupported]) {
    [captureConnection
        setVideoMaxFrameDuration:CoreMediaGlue::CMTimeMake(
                                     media::kFrameRatePrecision,
                                     (int)(frameRate *
                                           media::kFrameRatePrecision))];
  }
  return YES;
}

- (BOOL)startCapture {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (!captureSession_) {
    DLOG(ERROR) << "Video capture session not initialized.";
    return NO;
  }
  // Connect the notifications.
  NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
  [nc addObserver:self
         selector:@selector(onVideoError:)
             name:AVFoundationGlue::AVCaptureSessionRuntimeErrorNotification()
           object:captureSession_];
  [captureSession_ startRunning];
  return YES;
}

- (void)stopCapture {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if ([captureSession_ isRunning])
    [captureSession_ stopRunning];  // Synchronous.
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

#pragma mark Private methods

// |captureOutput| is called by the capture device to deliver a new frame.
// AVFoundation calls from a number of threads, depending on, at least, if
// Chrome is on foreground or background.
- (void)captureOutput:(CrAVCaptureOutput*)captureOutput
didOutputSampleBuffer:(CoreMediaGlue::CMSampleBufferRef)sampleBuffer
       fromConnection:(CrAVCaptureConnection*)connection {
  const CoreMediaGlue::CMFormatDescriptionRef formatDescription =
      CoreMediaGlue::CMSampleBufferGetFormatDescription(sampleBuffer);
  const FourCharCode fourcc =
      CoreMediaGlue::CMFormatDescriptionGetMediaSubType(formatDescription);
  const CoreMediaGlue::CMVideoDimensions dimensions =
      CoreMediaGlue::CMVideoFormatDescriptionGetDimensions(formatDescription);
  const media::VideoCaptureFormat captureFormat(
      gfx::Size(dimensions.width, dimensions.height), frameRate_,
      FourCCToChromiumPixelFormat(fourcc));

  char* baseAddress = 0;
  size_t frameSize = 0;
  CVImageBufferRef videoFrame = nil;
  if (fourcc == CoreMediaGlue::kCMVideoCodecType_JPEG_OpenDML) {
    // If MJPEG, use block buffer instead of pixel buffer.
    CoreMediaGlue::CMBlockBufferRef blockBuffer =
        CoreMediaGlue::CMSampleBufferGetDataBuffer(sampleBuffer);
    if (blockBuffer) {
      size_t lengthAtOffset;
      CoreMediaGlue::CMBlockBufferGetDataPointer(
          blockBuffer, 0, &lengthAtOffset, &frameSize, &baseAddress);
      // Expect the MJPEG data to be available as a contiguous reference, i.e.
      // not covered by multiple memory blocks.
      CHECK_EQ(lengthAtOffset, frameSize);
    }
  } else {
    videoFrame = CoreMediaGlue::CMSampleBufferGetImageBuffer(sampleBuffer);
    // Lock the frame and calculate frame size.
    if (CVPixelBufferLockBaseAddress(videoFrame, kCVPixelBufferLock_ReadOnly) ==
        kCVReturnSuccess) {
      baseAddress = static_cast<char*>(CVPixelBufferGetBaseAddress(videoFrame));
      frameSize = CVPixelBufferGetHeight(videoFrame) *
                  CVPixelBufferGetBytesPerRow(videoFrame);
    } else {
      videoFrame = nil;
    }
  }

  {
    base::AutoLock lock(lock_);
    const CoreMediaGlue::CMTime cm_timestamp =
        CoreMediaGlue::CMSampleBufferGetPresentationTimeStamp(sampleBuffer);
    const base::TimeDelta timestamp =
        CMTIME_IS_VALID(cm_timestamp)
            ? base::TimeDelta::FromMicroseconds(
                  cm_timestamp.value * base::TimeTicks::kMicrosecondsPerSecond /
                  cm_timestamp.timescale)
            : media::kNoTimestamp();

    if (frameReceiver_ && baseAddress) {
      frameReceiver_->ReceiveFrame(reinterpret_cast<uint8_t*>(baseAddress),
                                   frameSize, captureFormat, 0, 0, timestamp);
    }
  }

  if (videoFrame)
    CVPixelBufferUnlockBaseAddress(videoFrame, kCVPixelBufferLock_ReadOnly);
}

- (void)onVideoError:(NSNotification*)errorNotification {
  NSError* error = base::mac::ObjCCast<NSError>([[errorNotification userInfo]
      objectForKey:AVFoundationGlue::AVCaptureSessionErrorKey()]);
  [self sendErrorString:[NSString
                            stringWithFormat:@"%@: %@",
                                             [error localizedDescription],
                                             [error localizedFailureReason]]];
}

- (void)sendErrorString:(NSString*)error {
  DLOG(ERROR) << [error UTF8String];
  base::AutoLock lock(lock_);
  if (frameReceiver_)
    frameReceiver_->ReceiveError(FROM_HERE, [error UTF8String]);
}

@end
