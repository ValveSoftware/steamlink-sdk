// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScopedPaintChunkProperties_h
#define ScopedPaintChunkProperties_h

#include "platform/graphics/paint/PaintChunkProperties.h"
#include "platform/graphics/paint/PaintController.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"

namespace blink {

class ScopedPaintChunkProperties {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    WTF_MAKE_NONCOPYABLE(ScopedPaintChunkProperties);
public:
    ScopedPaintChunkProperties(PaintController& paintController, const PaintChunkProperties& properties)
        : m_paintController(paintController)
        , m_previousProperties(paintController.currentPaintChunkProperties())
    {
        m_paintController.updateCurrentPaintChunkProperties(properties);
    }

    ~ScopedPaintChunkProperties()
    {
        m_paintController.updateCurrentPaintChunkProperties(m_previousProperties);
    }

private:
    PaintController& m_paintController;
    PaintChunkProperties m_previousProperties;
};

} // namespace blink

#endif // ScopedPaintChunkProperties_h
