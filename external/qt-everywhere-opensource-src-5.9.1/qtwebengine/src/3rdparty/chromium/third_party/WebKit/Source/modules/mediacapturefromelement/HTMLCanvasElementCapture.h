// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLCanvasElementCapture_h
#define HTMLCanvasElementCapture_h

#include "core/html/HTMLCanvasElement.h"
#include "platform/heap/Handle.h"
#include "wtf/Allocator.h"

namespace blink {

class MediaStream;
class ExceptionState;

class HTMLCanvasElementCapture {
  STATIC_ONLY(HTMLCanvasElementCapture);

 public:
  static MediaStream* captureStream(HTMLCanvasElement&, ExceptionState&);
  static MediaStream* captureStream(HTMLCanvasElement&,
                                    double frameRate,
                                    ExceptionState&);

 private:
  static MediaStream* captureStream(HTMLCanvasElement&,
                                    bool givenFrameRate,
                                    double frameRate,
                                    ExceptionState&);
};

}  // namespace blink

#endif
