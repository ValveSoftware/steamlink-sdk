// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TransformRecorder_h
#define TransformRecorder_h

#include "core/CoreExport.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/Allocator.h"

namespace blink {

class GraphicsContext;
class AffineTransform;

class CORE_EXPORT TransformRecorder {
  STACK_ALLOCATED();

 public:
  TransformRecorder(GraphicsContext&,
                    const DisplayItemClient&,
                    const AffineTransform&);
  ~TransformRecorder();

 private:
  GraphicsContext& m_context;
  const DisplayItemClient& m_client;
  bool m_skipRecordingForIdentityTransform;
};

}  // namespace blink

#endif  // TransformRecorder_h
