// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EmbeddedObjectPainter_h
#define EmbeddedObjectPainter_h

#include "wtf/Allocator.h"

namespace blink {

struct PaintInfo;
class LayoutEmbeddedObject;
class LayoutPoint;

class EmbeddedObjectPainter {
  STACK_ALLOCATED();

 public:
  EmbeddedObjectPainter(const LayoutEmbeddedObject& layoutEmbeddedObject)
      : m_layoutEmbeddedObject(layoutEmbeddedObject) {}

  void paintReplaced(const PaintInfo&, const LayoutPoint& paintOffset);

 private:
  const LayoutEmbeddedObject& m_layoutEmbeddedObject;
};

}  // namespace blink

#endif  // EmbeddedObjectPainter_h
