// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RoundedInnerRectClipper_h
#define RoundedInnerRectClipper_h

#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/Allocator.h"

namespace blink {

class FloatRoundedRect;
class LayoutRect;
class LayoutObject;
struct PaintInfo;

enum RoundedInnerRectClipperBehavior { ApplyToDisplayList, ApplyToContext };

class RoundedInnerRectClipper {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  RoundedInnerRectClipper(const LayoutObject&,
                          const PaintInfo&,
                          const LayoutRect&,
                          const FloatRoundedRect& clipRect,
                          RoundedInnerRectClipperBehavior);
  ~RoundedInnerRectClipper();

 private:
  const LayoutObject& m_layoutObject;
  const PaintInfo& m_paintInfo;
  bool m_usePaintController;
  DisplayItem::Type m_clipType;
};

}  // namespace blink

#endif  // RoundedInnerRectClipper_h
