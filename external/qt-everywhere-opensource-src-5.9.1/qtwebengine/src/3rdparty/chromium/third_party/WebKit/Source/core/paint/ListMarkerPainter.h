// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ListMarkerPainter_h
#define ListMarkerPainter_h

#include "wtf/Allocator.h"

namespace blink {

struct PaintInfo;
class LayoutListMarker;
class LayoutPoint;

class ListMarkerPainter {
  STACK_ALLOCATED();

 public:
  ListMarkerPainter(const LayoutListMarker& layoutListMarker)
      : m_layoutListMarker(layoutListMarker) {}

  void paint(const PaintInfo&, const LayoutPoint& paintOffset);

 private:
  const LayoutListMarker& m_layoutListMarker;
};

}  // namespace blink

#endif  // ListMarkerPainter_h
