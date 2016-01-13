// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_MAC_COREMEDIA_GLUE_H_
#define MEDIA_VIDEO_CAPTURE_MAC_COREMEDIA_GLUE_H_

#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>

#include "base/basictypes.h"
#include "media/base/media_export.h"

// CoreMedia API is only introduced in Mac OS X > 10.6, the (potential) linking
// with it has to happen in runtime. If it succeeds, subsequent clients can use
// CoreMedia via the class declared in this file, where the original naming has
// been kept as much as possible.
class MEDIA_EXPORT CoreMediaGlue {
 public:
  // Originally from CMTime.h
  typedef int64_t CMTimeValue;
  typedef int32_t CMTimeScale;
  typedef int64_t CMTimeEpoch;
  typedef uint32_t CMTimeFlags;
  typedef struct {
    CMTimeValue value;
    CMTimeScale timescale;
    CMTimeFlags flags;
    CMTimeEpoch epoch;
  } CMTime;

  // Originally from CMFormatDescription.h.
  typedef const struct opaqueCMFormatDescription* CMFormatDescriptionRef;
  typedef CMFormatDescriptionRef CMVideoFormatDescriptionRef;
  typedef struct {
    int32_t width;
    int32_t height;
  } CMVideoDimensions;
  enum {
    kCMPixelFormat_422YpCbCr8_yuvs = 'yuvs',
  };
  enum {
    kCMVideoCodecType_JPEG_OpenDML = 'dmb1',
  };

  // Originally from CMSampleBuffer.h.
  typedef struct OpaqueCMSampleBuffer* CMSampleBufferRef;

  // Originally from CMTime.h.
  static CMTime CMTimeMake(int64_t value, int32_t timescale);

  // Originally from CMSampleBuffer.h.
  static CVImageBufferRef CMSampleBufferGetImageBuffer(
      CMSampleBufferRef buffer);

  // Originally from CMFormatDescription.h.
  static FourCharCode CMFormatDescriptionGetMediaSubType(
      CMFormatDescriptionRef desc);
  static CMVideoDimensions CMVideoFormatDescriptionGetDimensions(
      CMVideoFormatDescriptionRef videoDesc);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CoreMediaGlue);
};

#endif  // MEDIA_VIDEO_CAPTURE_MAC_COREMEDIA_GLUE_H_
