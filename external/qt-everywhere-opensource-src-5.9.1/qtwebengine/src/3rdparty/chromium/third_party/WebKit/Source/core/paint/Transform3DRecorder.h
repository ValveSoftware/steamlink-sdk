// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Transform3DRecorder_h
#define Transform3DRecorder_h

#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/Allocator.h"

namespace blink {

class FloatPoint3D;
class GraphicsContext;
class TransformationMatrix;

class Transform3DRecorder {
  STACK_ALLOCATED();

 public:
  Transform3DRecorder(GraphicsContext&,
                      const DisplayItemClient&,
                      DisplayItem::Type,
                      const TransformationMatrix&,
                      const FloatPoint3D& transformOrigin);
  ~Transform3DRecorder();

 private:
  GraphicsContext& m_context;
  const DisplayItemClient& m_client;
  DisplayItem::Type m_type;
  bool m_skipRecordingForIdentityTransform;
};

}  // namespace blink

#endif  // Transform3DRecorder_h
