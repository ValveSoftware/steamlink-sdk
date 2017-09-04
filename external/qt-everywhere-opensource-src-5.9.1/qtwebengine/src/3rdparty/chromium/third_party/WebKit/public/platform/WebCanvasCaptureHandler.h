// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebCanvasCaptureHandler_h
#define WebCanvasCaptureHandler_h

#include "WebCommon.h"

class SkImage;

namespace blink {

// Platform interface of a CanvasCaptureHandler.
class BLINK_PLATFORM_EXPORT WebCanvasCaptureHandler {
 public:
  virtual ~WebCanvasCaptureHandler() = default;
  virtual void sendNewFrame(const SkImage*) {}
  virtual bool needsNewFrame() const { return false; }
};

}  // namespace blink

#endif  // WebMediaRecorderHandler_h
