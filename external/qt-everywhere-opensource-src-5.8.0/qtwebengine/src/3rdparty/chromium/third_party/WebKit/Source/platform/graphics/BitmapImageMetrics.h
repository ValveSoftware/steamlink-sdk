// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BitmapImageMetrics_h
#define BitmapImageMetrics_h

#include "platform/PlatformExport.h"
#include "platform/graphics/ImageOrientation.h"
#include "wtf/Allocator.h"
#include "wtf/Forward.h"

namespace blink {

class PLATFORM_EXPORT BitmapImageMetrics {
    STATIC_ONLY(BitmapImageMetrics);
public:
    // Values synced with 'DecodedImageType' in src/tools/metrics/histograms/histograms.xml
    enum DecodedImageType {
        ImageUnknown = 0,
        ImageJPEG = 1,
        ImagePNG = 2,
        ImageGIF = 3,
        ImageWebP = 4,
        ImageICO = 5,
        ImageBMP = 6,
        DecodedImageTypeEnumEnd = ImageBMP + 1
    };

    static void countDecodedImageType(const String& type);
    static void countImageOrientation(const ImageOrientationEnum);
};

} // namespace blink

#endif
