// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MAC_COREVIDEO_GLUE_H_
#define MEDIA_BASE_MAC_COREVIDEO_GLUE_H_

#include "base/macros.h"
#include "media/base/media_export.h"

// Although CoreVideo exists in 10.6, not all of its types and functions were
// introduced in that release. Clients can use this class to access types,
// constants and functions not available in 10.6.
class MEDIA_EXPORT CoreVideoGlue {
 public:
  // Originally from CVPixelBuffer.h
  enum {
    kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange = '420v',
  };
  typedef struct CVPlanarPixelBufferInfo_YCbCrPlanar
      CVPlanarPixelBufferInfo_YCbCrPlanar;
  struct CVPlanarPixelBufferInfo_YCbCrBiPlanar {
    CVPlanarComponentInfo componentInfoY;
    CVPlanarComponentInfo componentInfoCbCr;
  };
  typedef struct CVPlanarPixelBufferInfo_YCbCrBiPlanar
      CVPlanarPixelBufferInfo_YCbCrBiPlanar;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CoreVideoGlue);
};

#endif  // MEDIA_BASE_MAC_COREVIDEO_GLUE_H_
