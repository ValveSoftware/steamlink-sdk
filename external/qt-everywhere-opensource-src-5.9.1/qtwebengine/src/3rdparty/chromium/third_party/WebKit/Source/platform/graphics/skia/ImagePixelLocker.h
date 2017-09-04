// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImagePixelLocker_h
#define ImagePixelLocker_h

#include "platform/heap/Heap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

class SkImage;

namespace blink {

class ImagePixelLocker final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  WTF_MAKE_NONCOPYABLE(ImagePixelLocker);

 public:
  ImagePixelLocker(sk_sp<const SkImage>, SkAlphaType, SkColorType);

  const void* pixels() const { return m_pixels; }

 private:
  const sk_sp<const SkImage> m_image;
  const void* m_pixels;
  Vector<char> m_pixelStorage;
};

}  // namespace blink

#endif
