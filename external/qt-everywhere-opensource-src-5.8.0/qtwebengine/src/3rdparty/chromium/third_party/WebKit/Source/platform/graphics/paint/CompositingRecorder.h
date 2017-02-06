// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositingRecorder_h
#define CompositingRecorder_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "wtf/Allocator.h"

namespace blink {

class GraphicsContext;

class PLATFORM_EXPORT CompositingRecorder {
    USING_FAST_MALLOC(CompositingRecorder);
public:
    CompositingRecorder(GraphicsContext&, const DisplayItemClient&, const SkXfermode::Mode, const float opacity, const FloatRect* bounds = 0, ColorFilter = ColorFilterNone);

    ~CompositingRecorder();

    // FIXME: These helpers only exist to ease the transition to slimming paint
    //        and should be removed once slimming paint is enabled by default.
    static void beginCompositing(GraphicsContext&, const DisplayItemClient&, const SkXfermode::Mode, const float opacity, const FloatRect* bounds = 0, ColorFilter = ColorFilterNone);
    static void endCompositing(GraphicsContext&, const DisplayItemClient&);

private:
    const DisplayItemClient& m_client;
    GraphicsContext& m_graphicsContext;
};

} // namespace blink

#endif // CompositingRecorder_h
