// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollRecorder_h
#define ScrollRecorder_h

#include "core/CoreExport.h"
#include "core/paint/PaintPhase.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/Allocator.h"

namespace blink {

class GraphicsContext;

// Emits display items which represent a region which is scrollable, so that it
// can be translated by the scroll offset.
class CORE_EXPORT ScrollRecorder {
  USING_FAST_MALLOC(ScrollRecorder);

 public:
  ScrollRecorder(GraphicsContext&,
                 const DisplayItemClient&,
                 DisplayItem::Type,
                 const IntSize& currentOffset);
  ScrollRecorder(GraphicsContext&,
                 const DisplayItemClient&,
                 PaintPhase,
                 const IntSize& currentOffset);
  ~ScrollRecorder();

 private:
  const DisplayItemClient& m_client;
  DisplayItem::Type m_beginItemType;
  GraphicsContext& m_context;
};

}  // namespace blink

#endif  // ScrollRecorder_h
