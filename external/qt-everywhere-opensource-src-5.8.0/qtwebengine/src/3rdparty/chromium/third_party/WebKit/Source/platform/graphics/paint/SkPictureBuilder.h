// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SkPictureBuilder_h
#define SkPictureBuilder_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/DisplayItemClient.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include <memory>

class SkMetaData;
class SkPicture;

namespace blink {

class GraphicsContext;
class PaintController;

// When slimming paint ships we can remove this SkPicture abstraction and
// rely on PaintController here.
class PLATFORM_EXPORT SkPictureBuilder final : public DisplayItemClient {
    WTF_MAKE_NONCOPYABLE(SkPictureBuilder);
public:
    // Constructs a new builder with the given bounds for the resulting recorded picture. If
    // |metadata| is specified, that metadata is propagated to the builder's internal canvas. If
    // |containingContext| is specified, the device scale factor, printing, and disabled state are
    // propagated to the builder's internal context.
    SkPictureBuilder(const FloatRect& bounds, SkMetaData* = nullptr, GraphicsContext* containingContext = nullptr);
    ~SkPictureBuilder();

    GraphicsContext& context() { return *m_context; }

    // Returns a picture capturing all drawing performed on the builder's context since
    // construction.
    PassRefPtr<SkPicture> endRecording();

    // DisplayItemClient methods
    String debugName() const final { return "SkPictureBuilder"; }
    LayoutRect visualRect() const final { return LayoutRect(); }

private:
    std::unique_ptr<PaintController> m_paintController;
    std::unique_ptr<GraphicsContext> m_context;
    FloatRect m_bounds;
};

} // namespace blink

#endif // SkPictureBuilder_h
