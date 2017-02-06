// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MAC_VIDEOTOOLBOX_GLUE_H_
#define MEDIA_BASE_MAC_VIDEOTOOLBOX_GLUE_H_

#include <stdint.h>

#include "base/macros.h"
#include "media/base/mac/coremedia_glue.h"
#include "media/base/media_export.h"

// VideoToolbox API is available in and after OS X 10.9 and iOS 8 (10.8 has
// support for software encoding, but this class exposes the 10.9 API level).
// Chromium requires OS X 10.9 or iOS 9. This class is defined to try and load
// the VideoToolbox library at runtime. If it succeeds, clients can use
// VideoToolbox via this class.
// Note that this file is necessary because Chromium still targets OS X 10.6 for
// deployment. It should be deprecated soon, see crbug.com/579648.
class MEDIA_EXPORT VideoToolboxGlue {
 public:
  class Loader;

  // Returns a glue object if VideoToolbox is supported or null otherwise.
  // Using a glue object allows to avoid expensive atomic operations on every
  // function call. The object has process life duration and must not be
  // deleted.
  static const VideoToolboxGlue* Get();

  // Originally from VTErrors.h
  typedef UInt32 VTEncodeInfoFlags;
  enum {
    kVTEncodeInfo_Asynchronous = 1UL << 0,
    kVTEncodeInfo_FrameDropped = 1UL << 1,
  };

  // Originally from VTCompressionSession.h
  typedef struct OpaqueVTCompressionSession* VTCompressionSessionRef;
  typedef void (*VTCompressionOutputCallback)(
      void* outputCallbackRefCon,
      void* sourceFrameRefCon,
      OSStatus status,
      VTEncodeInfoFlags infoFlags,
      CoreMediaGlue::CMSampleBufferRef sampleBuffer);

  // Originally from VTSession.h
  typedef CFTypeRef VTSessionRef;

  // Originally from VTCompressionProperties.h
  CFStringRef kVTCompressionPropertyKey_AllowFrameReordering() const;
  CFStringRef kVTCompressionPropertyKey_AverageBitRate() const;
  CFStringRef kVTCompressionPropertyKey_ColorPrimaries() const;
  CFStringRef kVTCompressionPropertyKey_DataRateLimits() const;
  CFStringRef kVTCompressionPropertyKey_ExpectedFrameRate() const;
  CFStringRef kVTCompressionPropertyKey_MaxFrameDelayCount() const;
  CFStringRef kVTCompressionPropertyKey_MaxKeyFrameInterval() const;
  CFStringRef kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration() const;
  CFStringRef kVTCompressionPropertyKey_ProfileLevel() const;
  CFStringRef kVTCompressionPropertyKey_RealTime() const;
  CFStringRef kVTCompressionPropertyKey_TransferFunction() const;
  CFStringRef kVTCompressionPropertyKey_YCbCrMatrix() const;

  CFStringRef kVTEncodeFrameOptionKey_ForceKeyFrame() const;

  CFStringRef kVTProfileLevel_H264_Baseline_AutoLevel() const;
  CFStringRef kVTProfileLevel_H264_Main_AutoLevel() const;
  CFStringRef kVTProfileLevel_H264_Extended_AutoLevel() const;
  CFStringRef kVTProfileLevel_H264_High_AutoLevel() const;

  CFStringRef
      kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder()
      const;
  CFStringRef
  kVTVideoEncoderSpecification_RequireHardwareAcceleratedVideoEncoder() const;

  // Originally from VTCompressionSession.h
  OSStatus VTCompressionSessionCreate(
      CFAllocatorRef allocator,
      int32_t width,
      int32_t height,
      CoreMediaGlue::CMVideoCodecType codecType,
      CFDictionaryRef encoderSpecification,
      CFDictionaryRef sourceImageBufferAttributes,
      CFAllocatorRef compressedDataAllocator,
      VTCompressionOutputCallback outputCallback,
      void* outputCallbackRefCon,
      VTCompressionSessionRef* compressionSessionOut) const;
  OSStatus VTCompressionSessionEncodeFrame(
      VTCompressionSessionRef session,
      CVImageBufferRef imageBuffer,
      CoreMediaGlue::CMTime presentationTimeStamp,
      CoreMediaGlue::CMTime duration,
      CFDictionaryRef frameProperties,
      void* sourceFrameRefCon,
      VTEncodeInfoFlags* infoFlagsOut) const;
  CVPixelBufferPoolRef VTCompressionSessionGetPixelBufferPool(
      VTCompressionSessionRef session) const;
  void VTCompressionSessionInvalidate(VTCompressionSessionRef session) const;
  OSStatus VTCompressionSessionCompleteFrames(
      VTCompressionSessionRef session,
      CoreMediaGlue::CMTime completeUntilPresentationTimeStamp) const;

  // Originally from VTSession.h
  OSStatus VTSessionSetProperty(VTSessionRef session,
                                CFStringRef propertyKey,
                                CFTypeRef propertyValue) const;

 private:
  struct Library;
  VideoToolboxGlue();
  Library* library_;
  DISALLOW_COPY_AND_ASSIGN(VideoToolboxGlue);
};

#endif  // MEDIA_BASE_MAC_VIDEOTOOLBOX_GLUE_H_
