// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipPathClipper_h
#define ClipPathClipper_h

#include "platform/graphics/paint/ClipPathRecorder.h"
#include "wtf/Optional.h"

namespace blink {

class ClipPathOperation;
class FloatPoint;
class FloatRect;
class GraphicsContext;
class LayoutSVGResourceClipper;
class LayoutObject;

enum class ClipperState { NotApplied, AppliedPath, AppliedMask };

class ClipPathClipper {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  ClipPathClipper(GraphicsContext&,
                  ClipPathOperation&,
                  const LayoutObject&,
                  const FloatRect& referenceBox,
                  const FloatPoint& origin);
  ~ClipPathClipper();

  bool usingMask() const { return m_clipperState == ClipperState::AppliedMask; }

 private:
  LayoutSVGResourceClipper* m_resourceClipper;
  Optional<ClipPathRecorder> m_clipPathRecorder;
  ClipperState m_clipperState;
  const LayoutObject& m_layoutObject;
  GraphicsContext& m_context;
};

}  // namespace blink

#endif  // ClipPathClipper_h
