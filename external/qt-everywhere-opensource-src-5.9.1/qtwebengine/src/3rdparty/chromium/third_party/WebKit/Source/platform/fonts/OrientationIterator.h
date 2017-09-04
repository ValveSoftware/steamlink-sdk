// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OrientationIterator_h
#define OrientationIterator_h

#include "platform/fonts/FontOrientation.h"
#include "platform/fonts/ScriptRunIterator.h"
#include "platform/fonts/UTF16TextIterator.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include <memory>

namespace blink {

class PLATFORM_EXPORT OrientationIterator {
  USING_FAST_MALLOC(OrientationIterator);
  WTF_MAKE_NONCOPYABLE(OrientationIterator);

 public:
  enum RenderOrientation {
    OrientationKeep,
    OrientationRotateSideways,
    OrientationInvalid
  };

  OrientationIterator(const UChar* buffer,
                      unsigned bufferSize,
                      FontOrientation runOrientation);

  bool consume(unsigned* orientationLimit, RenderOrientation*);

 private:
  std::unique_ptr<UTF16TextIterator> m_utf16Iterator;
  unsigned m_bufferSize;
  bool m_atEnd;
};

}  // namespace blink

#endif
