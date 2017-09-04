// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DisplayItemCacheSkipper_h
#define DisplayItemCacheSkipper_h

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintController.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"

namespace blink {

class DisplayItemCacheSkipper final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  WTF_MAKE_NONCOPYABLE(DisplayItemCacheSkipper);

 public:
  DisplayItemCacheSkipper(GraphicsContext& context) : m_context(context) {
    context.getPaintController().beginSkippingCache();
  }
  ~DisplayItemCacheSkipper() {
    m_context.getPaintController().endSkippingCache();
  }

 private:
  GraphicsContext& m_context;
};

}  // namespace blink

#endif  // DisplayItemCacheSkipper_h
