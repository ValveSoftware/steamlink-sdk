// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EmbeddedObjectPaintInvalidator_h
#define EmbeddedObjectPaintInvalidator_h

#include "platform/graphics/PaintInvalidationReason.h"
#include "wtf/Allocator.h"

namespace blink {

class LayoutEmbeddedObject;
struct PaintInvalidatorContext;

class EmbeddedObjectPaintInvalidator {
  STACK_ALLOCATED();

 public:
  EmbeddedObjectPaintInvalidator(const LayoutEmbeddedObject& embeddedObject,
                                 const PaintInvalidatorContext& context)
      : m_embeddedObject(embeddedObject), m_context(context) {}

  PaintInvalidationReason invalidatePaintIfNeeded();

 private:
  const LayoutEmbeddedObject& m_embeddedObject;
  const PaintInvalidatorContext& m_context;
};

}  // namespace blink

#endif
