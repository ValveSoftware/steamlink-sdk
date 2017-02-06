// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "media/base/mac/avfoundation_glue.h"

#import <AVFoundation/AVFoundation.h>
#include <dlfcn.h>
#include <stddef.h>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/mac/mac_util.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/trace_event/trace_event.h"
#include "media/base/media_switches.h"

namespace {
// This class is used to retrieve AVFoundation NSBundle and library handle. It
// must be used as a LazyInstance so that it is initialised once and in a
// thread-safe way. Normally no work is done in constructors: LazyInstance is
// an exception.
class AVFoundationInternal {
 public:
  AVFoundationInternal() {
    bundle_ = [NSBundle
        bundleWithPath:@"/System/Library/Frameworks/AVFoundation.framework"];
  }

  NSBundle* bundle() const { return bundle_; }

  NSString* AVCaptureDeviceWasConnectedNotification() const {
    return AVCaptureDeviceWasConnectedNotification_;
  }
  NSString* AVCaptureDeviceWasDisconnectedNotification() const {
    return AVCaptureDeviceWasDisconnectedNotification_;
  }
  NSString* AVMediaTypeVideo() const { return AVMediaTypeVideo_; }
  NSString* AVMediaTypeAudio() const { return AVMediaTypeAudio_; }
  NSString* AVMediaTypeMuxed() const { return AVMediaTypeMuxed_; }
  NSString* AVCaptureSessionRuntimeErrorNotification() const {
    return AVCaptureSessionRuntimeErrorNotification_;
  }
  NSString* AVCaptureSessionDidStopRunningNotification() const {
    return AVCaptureSessionDidStopRunningNotification_;
  }
  NSString* AVCaptureSessionErrorKey() const {
    return AVCaptureSessionErrorKey_;
  }
  NSString* AVVideoScalingModeKey() const { return AVVideoScalingModeKey_; }
  NSString* AVVideoScalingModeResizeAspectFill() const {
    return AVVideoScalingModeResizeAspectFill_;
  }

 private:
  NSBundle* bundle_;
  // The following members are replicas of the respectives in AVFoundation.
  NSString* AVCaptureDeviceWasConnectedNotification_ =
      ::AVCaptureDeviceWasConnectedNotification;
  NSString* AVCaptureDeviceWasDisconnectedNotification_ =
      ::AVCaptureDeviceWasDisconnectedNotification;
  NSString* AVMediaTypeVideo_ = ::AVMediaTypeVideo;
  NSString* AVMediaTypeAudio_ = ::AVMediaTypeAudio;
  NSString* AVMediaTypeMuxed_ = ::AVMediaTypeMuxed;
  NSString* AVCaptureSessionRuntimeErrorNotification_ =
      ::AVCaptureSessionRuntimeErrorNotification;
  NSString* AVCaptureSessionDidStopRunningNotification_ =
      ::AVCaptureSessionDidStopRunningNotification;
  NSString* AVCaptureSessionErrorKey_ = ::AVCaptureSessionErrorKey;
  NSString* AVVideoScalingModeKey_ = ::AVVideoScalingModeKey;
  NSString* AVVideoScalingModeResizeAspectFill_ =
      ::AVVideoScalingModeResizeAspectFill;

  DISALLOW_COPY_AND_ASSIGN(AVFoundationInternal);
};

static base::ThreadLocalStorage::StaticSlot g_avfoundation_handle =
    TLS_INITIALIZER;

void TlsCleanup(void* value) {
  delete static_cast<AVFoundationInternal*>(value);
}

AVFoundationInternal* GetAVFoundationInternal() {
  return static_cast<AVFoundationInternal*>(g_avfoundation_handle.Get());
}

// This contains the logic of checking whether AVFoundation is supported.
// It's called only once and the results are cached in a static bool.
bool LoadAVFoundationInternal() {
  g_avfoundation_handle.Initialize(TlsCleanup);
  g_avfoundation_handle.Set(new AVFoundationInternal());
  const bool ret = [AVFoundationGlue::AVFoundationBundle() load];
  CHECK(ret);
  return ret;
}

enum {
  INITIALIZE_NOT_CALLED = 0,
  AVFOUNDATION_IS_SUPPORTED,
  AVFOUNDATION_NOT_SUPPORTED
} static g_avfoundation_initialization = INITIALIZE_NOT_CALLED;

}  // namespace

void AVFoundationGlue::InitializeAVFoundation() {
  TRACE_EVENT0("video", "AVFoundationGlue::InitializeAVFoundation");
  CHECK([NSThread isMainThread]);
  if (g_avfoundation_initialization != INITIALIZE_NOT_CALLED)
    return;
  g_avfoundation_initialization = LoadAVFoundationInternal() ?
      AVFOUNDATION_IS_SUPPORTED : AVFOUNDATION_NOT_SUPPORTED;
}

NSBundle const* AVFoundationGlue::AVFoundationBundle() {
  return GetAVFoundationInternal()->bundle();
}

NSString* AVFoundationGlue::AVCaptureDeviceWasConnectedNotification() {
  return GetAVFoundationInternal()->AVCaptureDeviceWasConnectedNotification();
}

NSString* AVFoundationGlue::AVCaptureDeviceWasDisconnectedNotification() {
  return GetAVFoundationInternal()
      ->AVCaptureDeviceWasDisconnectedNotification();
}

NSString* AVFoundationGlue::AVMediaTypeVideo() {
  return GetAVFoundationInternal()->AVMediaTypeVideo();
}

NSString* AVFoundationGlue::AVMediaTypeAudio() {
  return GetAVFoundationInternal()->AVMediaTypeAudio();
}

NSString* AVFoundationGlue::AVMediaTypeMuxed() {
  return GetAVFoundationInternal()->AVMediaTypeMuxed();
}

NSString* AVFoundationGlue::AVCaptureSessionRuntimeErrorNotification() {
  return GetAVFoundationInternal()->AVCaptureSessionRuntimeErrorNotification();
}

NSString* AVFoundationGlue::AVCaptureSessionDidStopRunningNotification() {
  return GetAVFoundationInternal()
      ->AVCaptureSessionDidStopRunningNotification();
}

NSString* AVFoundationGlue::AVCaptureSessionErrorKey() {
  return GetAVFoundationInternal()->AVCaptureSessionErrorKey();
}

NSString* AVFoundationGlue::AVVideoScalingModeKey() {
  return GetAVFoundationInternal()->AVVideoScalingModeKey();
}

NSString* AVFoundationGlue::AVVideoScalingModeResizeAspectFill() {
  return GetAVFoundationInternal()->AVVideoScalingModeResizeAspectFill();
}

Class AVFoundationGlue::AVCaptureSessionClass() {
  return [AVFoundationBundle() classNamed:@"AVCaptureSession"];
}

Class AVFoundationGlue::AVCaptureVideoDataOutputClass() {
  return [AVFoundationBundle() classNamed:@"AVCaptureVideoDataOutput"];
}

@implementation AVCaptureDeviceGlue

+ (NSArray*)devices {
  Class avcClass =
      [AVFoundationGlue::AVFoundationBundle() classNamed:@"AVCaptureDevice"];
  if ([avcClass respondsToSelector:@selector(devices)]) {
    return [avcClass performSelector:@selector(devices)];
  }
  return nil;
}

+ (CrAVCaptureDevice*)deviceWithUniqueID:(NSString*)deviceUniqueID {
  Class avcClass =
      [AVFoundationGlue::AVFoundationBundle() classNamed:@"AVCaptureDevice"];
  return [avcClass performSelector:@selector(deviceWithUniqueID:)
                        withObject:deviceUniqueID];
}

@end  // @implementation AVCaptureDeviceGlue

@implementation AVCaptureDeviceInputGlue

+ (CrAVCaptureDeviceInput*)deviceInputWithDevice:(CrAVCaptureDevice*)device
                                           error:(NSError**)outError {
  return [[AVFoundationGlue::AVFoundationBundle()
      classNamed:@"AVCaptureDeviceInput"] deviceInputWithDevice:device
                                                          error:outError];
}

@end  // @implementation AVCaptureDeviceInputGlue
