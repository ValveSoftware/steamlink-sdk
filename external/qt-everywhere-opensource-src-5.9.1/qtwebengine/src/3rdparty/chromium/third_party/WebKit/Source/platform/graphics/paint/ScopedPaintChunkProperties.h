// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScopedPaintChunkProperties_h
#define ScopedPaintChunkProperties_h

#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/PaintChunk.h"
#include "platform/graphics/paint/PaintChunkProperties.h"
#include "platform/graphics/paint/PaintController.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"

namespace blink {

class ScopedPaintChunkProperties {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  WTF_MAKE_NONCOPYABLE(ScopedPaintChunkProperties);

 public:
  ScopedPaintChunkProperties(PaintController& paintController,
                             const DisplayItemClient& client,
                             DisplayItem::Type type,
                             const PaintChunkProperties& properties)
      : m_paintController(paintController),
        m_previousProperties(paintController.currentPaintChunkProperties()) {
    PaintChunk::Id id(client, type);
    m_paintController.updateCurrentPaintChunkProperties(&id, properties);
  }

  // Omits the type parameter, in case that the client creates only one
  // PaintChunkProperties node during each painting.
  ScopedPaintChunkProperties(PaintController& paintController,
                             const DisplayItemClient& client,
                             const PaintChunkProperties& properties)
      : ScopedPaintChunkProperties(paintController,
                                   client,
                                   DisplayItem::kUninitializedType,
                                   properties) {}

  ~ScopedPaintChunkProperties() {
    // We should not return to the previous id, because that may cause a new
    // chunk to use the same id as that of the previous chunk before this
    // ScopedPaintChunkProperties. The painter should create another scope of
    // paint properties with new id, or the new chunk will have no id and will
    // not match any old chunk and will be treated as fully invalidated for
    // rasterization.
    m_paintController.updateCurrentPaintChunkProperties(nullptr,
                                                        m_previousProperties);
  }

 private:
  PaintController& m_paintController;
  PaintChunkProperties m_previousProperties;
};

}  // namespace blink

#endif  // ScopedPaintChunkProperties_h
