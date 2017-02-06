// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebImageCaptureFrameGrabber_h
#define WebImageCaptureFrameGrabber_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebCommon.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkImage;

namespace blink {

class WebMediaStreamTrack;

using WebImageCaptureGrabFrameCallbacks = WebCallbacks<sk_sp<SkImage>, void>;

// Platform interface of an ImageCapture class for grabFrame() calls.
class WebImageCaptureFrameGrabber {
public:
    virtual ~WebImageCaptureFrameGrabber() { }

    virtual void grabFrame(WebMediaStreamTrack*, WebImageCaptureGrabFrameCallbacks*) = 0;
};

} // namespace blink

#endif // WebImageCaptureFrameGrabber_h
