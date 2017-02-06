// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintRenderingContext2D_h
#define PaintRenderingContext2D_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"
#include "modules/canvas2d/BaseRenderingContext2D.h"
#include "platform/graphics/ImageBuffer.h"
#include <memory>

class SkCanvas;

namespace blink {

class CanvasImageSource;
class Color;

class MODULES_EXPORT PaintRenderingContext2D : public BaseRenderingContext2D, public GarbageCollectedFinalized<PaintRenderingContext2D>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(PaintRenderingContext2D);
    WTF_MAKE_NONCOPYABLE(PaintRenderingContext2D);
public:
    static PaintRenderingContext2D* create(std::unique_ptr<ImageBuffer> imageBuffer)
    {
        return new PaintRenderingContext2D(std::move(imageBuffer));
    }

    // BaseRenderingContext2D

    // PaintRenderingContext2D doesn't have any pixel readback so the origin
    // is always clean, and unable to taint it.
    bool originClean() const final { return true; }
    void setOriginTainted() final { }
    bool wouldTaintOrigin(CanvasImageSource*, ExecutionContext*) final { return false; }

    int width() const final;
    int height() const final;

    bool hasImageBuffer() const final { return m_imageBuffer.get(); }
    ImageBuffer* imageBuffer() const final { return m_imageBuffer.get(); }

    bool parseColorOrCurrentColor(Color&, const String& colorString) const final;

    SkCanvas* drawingCanvas() const final;
    SkCanvas* existingDrawingCanvas() const final;
    void disableDeferral(DisableDeferralReason) final { }

    AffineTransform baseTransform() const final;

    void didDraw(const SkIRect& dirtyRect) final;

    // TODO(ikilpatrick): We'll need to either only accept resolved filters
    // from a typed-om <filter> object, or use the appropriate style resolution
    // host to determine 'em' units etc in filters. At the moment just pretend
    // that we don't have a filter set.
    bool stateHasFilter() final { return false; }
    SkImageFilter* stateGetFilter() final { return nullptr; }
    void snapshotStateForFilter() final { }

    void validateStateStack() final;

    bool hasAlpha() const final { return true; }

    // PaintRenderingContext2D cannot lose it's context.
    bool isContextLost() const final { return false; }

private:
    explicit PaintRenderingContext2D(std::unique_ptr<ImageBuffer>);

    std::unique_ptr<ImageBuffer> m_imageBuffer;
};

} // namespace blink

#endif // PaintRenderingContext2D_h
