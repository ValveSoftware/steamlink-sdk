// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mac/videotoolbox_glue.h"

#include <dlfcn.h>
#import <Foundation/Foundation.h>

#include "base/lazy_instance.h"
#include "base/macros.h"

// This class stores VideoToolbox library symbol pointers.
struct VideoToolboxGlue::Library {
  typedef OSStatus (*VTCompressionSessionCreateMethod)(
      CFAllocatorRef,
      int32_t,
      int32_t,
      CoreMediaGlue::CMVideoCodecType,
      CFDictionaryRef,
      CFDictionaryRef,
      CFAllocatorRef,
      VTCompressionOutputCallback,
      void*,
      VTCompressionSessionRef*);
  typedef OSStatus (*VTCompressionSessionEncodeFrameMethod)(
      VTCompressionSessionRef,
      CVImageBufferRef,
      CoreMediaGlue::CMTime,
      CoreMediaGlue::CMTime,
      CFDictionaryRef,
      void*,
      VTEncodeInfoFlags*);
  typedef CVPixelBufferPoolRef (*VTCompressionSessionGetPixelBufferPoolMethod)(
      VTCompressionSessionRef);
  typedef void (*VTCompressionSessionInvalidateMethod)(VTCompressionSessionRef);
  typedef OSStatus (*VTCompressionSessionCompleteFramesMethod)(
      VTCompressionSessionRef,
      CoreMediaGlue::CMTime);
  typedef OSStatus (*VTSessionSetPropertyMethod)(VTSessionRef,
                                                 CFStringRef,
                                                 CFTypeRef);

  VTCompressionSessionCreateMethod VTCompressionSessionCreate;
  VTCompressionSessionEncodeFrameMethod VTCompressionSessionEncodeFrame;
  VTCompressionSessionGetPixelBufferPoolMethod
      VTCompressionSessionGetPixelBufferPool;
  VTCompressionSessionInvalidateMethod VTCompressionSessionInvalidate;
  VTCompressionSessionCompleteFramesMethod VTCompressionSessionCompleteFrames;
  VTSessionSetPropertyMethod VTSessionSetProperty;

  CFStringRef* kVTCompressionPropertyKey_AllowFrameReordering;
  CFStringRef* kVTCompressionPropertyKey_AverageBitRate;
  CFStringRef* kVTCompressionPropertyKey_ColorPrimaries;
  CFStringRef* kVTCompressionPropertyKey_DataRateLimits;
  CFStringRef* kVTCompressionPropertyKey_ExpectedFrameRate;
  CFStringRef* kVTCompressionPropertyKey_MaxFrameDelayCount;
  CFStringRef* kVTCompressionPropertyKey_MaxKeyFrameInterval;
  CFStringRef* kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration;
  CFStringRef* kVTCompressionPropertyKey_ProfileLevel;
  CFStringRef* kVTCompressionPropertyKey_RealTime;
  CFStringRef* kVTCompressionPropertyKey_TransferFunction;
  CFStringRef* kVTCompressionPropertyKey_YCbCrMatrix;
  CFStringRef* kVTEncodeFrameOptionKey_ForceKeyFrame;
  CFStringRef* kVTProfileLevel_H264_Baseline_AutoLevel;
  CFStringRef* kVTProfileLevel_H264_Main_AutoLevel;
  CFStringRef* kVTProfileLevel_H264_Extended_AutoLevel;
  CFStringRef* kVTProfileLevel_H264_High_AutoLevel;
  CFStringRef*
      kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder;
  CFStringRef*
      kVTVideoEncoderSpecification_RequireHardwareAcceleratedVideoEncoder;
};

// Lazy-instance responsible for loading VideoToolbox.
class VideoToolboxGlue::Loader {
 public:
  Loader() {
    NSBundle* bundle = [NSBundle
        bundleWithPath:@"/System/Library/Frameworks/VideoToolbox.framework"];
    const char* path = [[bundle executablePath] fileSystemRepresentation];
    if (!path)
      return;

    handle_ = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
    if (!handle_)
      return;

#define LOAD_SYMBOL(SYMBOL)                                             \
  if (!LoadSymbol(#SYMBOL, reinterpret_cast<void**>(&library_.SYMBOL))) \
    return;

    LOAD_SYMBOL(VTCompressionSessionCreate)
    LOAD_SYMBOL(VTCompressionSessionEncodeFrame)
    LOAD_SYMBOL(VTCompressionSessionGetPixelBufferPool)
    LOAD_SYMBOL(VTCompressionSessionInvalidate)
    LOAD_SYMBOL(VTCompressionSessionCompleteFrames)
    LOAD_SYMBOL(VTSessionSetProperty)

    LOAD_SYMBOL(kVTCompressionPropertyKey_AllowFrameReordering)
    LOAD_SYMBOL(kVTCompressionPropertyKey_AverageBitRate)
    LOAD_SYMBOL(kVTCompressionPropertyKey_ColorPrimaries)
    LOAD_SYMBOL(kVTCompressionPropertyKey_DataRateLimits)
    LOAD_SYMBOL(kVTCompressionPropertyKey_ExpectedFrameRate)
    LOAD_SYMBOL(kVTCompressionPropertyKey_MaxFrameDelayCount)
    LOAD_SYMBOL(kVTCompressionPropertyKey_MaxKeyFrameInterval)
    LOAD_SYMBOL(kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration)
    LOAD_SYMBOL(kVTCompressionPropertyKey_ProfileLevel)
    LOAD_SYMBOL(kVTCompressionPropertyKey_RealTime)
    LOAD_SYMBOL(kVTCompressionPropertyKey_TransferFunction)
    LOAD_SYMBOL(kVTCompressionPropertyKey_YCbCrMatrix)
    LOAD_SYMBOL(kVTEncodeFrameOptionKey_ForceKeyFrame);
    LOAD_SYMBOL(kVTProfileLevel_H264_Baseline_AutoLevel)
    LOAD_SYMBOL(kVTProfileLevel_H264_Main_AutoLevel)
    LOAD_SYMBOL(kVTProfileLevel_H264_Extended_AutoLevel)
    LOAD_SYMBOL(kVTProfileLevel_H264_High_AutoLevel)
    LOAD_SYMBOL(
        kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder)
    LOAD_SYMBOL(
        kVTVideoEncoderSpecification_RequireHardwareAcceleratedVideoEncoder)

#undef LOAD_SYMBOL

    glue_.library_ = &library_;
  }

  const VideoToolboxGlue* glue() const {
    return (glue_.library_) ? &glue_ : NULL;
  }

 private:
  bool LoadSymbol(const char* name, void** symbol_out) {
    *symbol_out = dlsym(handle_, name);
    return *symbol_out != NULL;
  }

  Library library_;
  VideoToolboxGlue glue_;
  void* handle_;

  DISALLOW_COPY_AND_ASSIGN(Loader);
};

static base::LazyInstance<VideoToolboxGlue::Loader> g_videotoolbox_loader =
    LAZY_INSTANCE_INITIALIZER;

// static
const VideoToolboxGlue* VideoToolboxGlue::Get() {
  return g_videotoolbox_loader.Get().glue();
}

VideoToolboxGlue::VideoToolboxGlue() : library_(NULL) {
}

OSStatus VideoToolboxGlue::VTCompressionSessionCreate(
    CFAllocatorRef allocator,
    int32_t width,
    int32_t height,
    CoreMediaGlue::CMVideoCodecType codecType,
    CFDictionaryRef encoderSpecification,
    CFDictionaryRef sourceImageBufferAttributes,
    CFAllocatorRef compressedDataAllocator,
    VTCompressionOutputCallback outputCallback,
    void* outputCallbackRefCon,
    VTCompressionSessionRef* compressionSessionOut) const {
  return library_->VTCompressionSessionCreate(allocator,
                                              width,
                                              height,
                                              codecType,
                                              encoderSpecification,
                                              sourceImageBufferAttributes,
                                              compressedDataAllocator,
                                              outputCallback,
                                              outputCallbackRefCon,
                                              compressionSessionOut);
}

OSStatus VideoToolboxGlue::VTCompressionSessionEncodeFrame(
    VTCompressionSessionRef session,
    CVImageBufferRef imageBuffer,
    CoreMediaGlue::CMTime presentationTimeStamp,
    CoreMediaGlue::CMTime duration,
    CFDictionaryRef frameProperties,
    void* sourceFrameRefCon,
    VTEncodeInfoFlags* infoFlagsOut) const {
  return library_->VTCompressionSessionEncodeFrame(session,
                                                   imageBuffer,
                                                   presentationTimeStamp,
                                                   duration,
                                                   frameProperties,
                                                   sourceFrameRefCon,
                                                   infoFlagsOut);
}

CVPixelBufferPoolRef VideoToolboxGlue::VTCompressionSessionGetPixelBufferPool(
    VTCompressionSessionRef session) const {
  return library_->VTCompressionSessionGetPixelBufferPool(session);
}

void VideoToolboxGlue::VTCompressionSessionInvalidate(
    VTCompressionSessionRef session) const {
  library_->VTCompressionSessionInvalidate(session);
}

OSStatus VideoToolboxGlue::VTCompressionSessionCompleteFrames(
    VTCompressionSessionRef session,
    CoreMediaGlue::CMTime completeUntilPresentationTimeStamp) const {
  return library_->VTCompressionSessionCompleteFrames(
      session, completeUntilPresentationTimeStamp);
}

OSStatus VideoToolboxGlue::VTSessionSetProperty(VTSessionRef session,
                                                CFStringRef propertyKey,
                                                CFTypeRef propertyValue) const {
  return library_->VTSessionSetProperty(session, propertyKey, propertyValue);
}

#define KEY_ACCESSOR(KEY) \
  CFStringRef VideoToolboxGlue::KEY() const { return *library_->KEY; }

KEY_ACCESSOR(kVTCompressionPropertyKey_AllowFrameReordering)
KEY_ACCESSOR(kVTCompressionPropertyKey_AverageBitRate)
KEY_ACCESSOR(kVTCompressionPropertyKey_ColorPrimaries)
KEY_ACCESSOR(kVTCompressionPropertyKey_DataRateLimits)
KEY_ACCESSOR(kVTCompressionPropertyKey_ExpectedFrameRate)
KEY_ACCESSOR(kVTCompressionPropertyKey_MaxFrameDelayCount)
KEY_ACCESSOR(kVTCompressionPropertyKey_MaxKeyFrameInterval)
KEY_ACCESSOR(kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration)
KEY_ACCESSOR(kVTCompressionPropertyKey_ProfileLevel)
KEY_ACCESSOR(kVTCompressionPropertyKey_RealTime)
KEY_ACCESSOR(kVTCompressionPropertyKey_TransferFunction)
KEY_ACCESSOR(kVTCompressionPropertyKey_YCbCrMatrix)
KEY_ACCESSOR(kVTEncodeFrameOptionKey_ForceKeyFrame)
KEY_ACCESSOR(kVTProfileLevel_H264_Baseline_AutoLevel)
KEY_ACCESSOR(kVTProfileLevel_H264_Main_AutoLevel)
KEY_ACCESSOR(kVTProfileLevel_H264_Extended_AutoLevel)
KEY_ACCESSOR(kVTProfileLevel_H264_High_AutoLevel)
KEY_ACCESSOR(kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder)
KEY_ACCESSOR(
    kVTVideoEncoderSpecification_RequireHardwareAcceleratedVideoEncoder)

#undef KEY_ACCESSOR
