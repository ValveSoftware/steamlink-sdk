// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageEncoderUtils_h
#define ImageEncoderUtils_h

#include "platform/PlatformExport.h"
#include "wtf/text/WTFString.h"

namespace blink {

class PLATFORM_EXPORT ImageEncoderUtils {
 public:
  enum EncodeReason {
    EncodeReasonToDataURL = 0,
    EncodeReasonToBlobCallback = 1,
    EncodeReasonConvertToBlobPromise = 2,
    NumberOfEncodeReasons
  };
  static String toEncodingMimeType(const String& mimeType, const EncodeReason);

  // Default image mime type for toDataURL and toBlob functions
  static const char DefaultMimeType[];
};

}  // namespace blink

#endif  // ImageEncoderUtils_h
