// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OffscreenCanvas_h
#define OffscreenCanvas_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/html/HTMLCanvasElement.h"
#include "platform/geometry/IntSize.h"
#include "platform/heap/Handle.h"
#include <memory>

namespace blink {

class CanvasContextCreationAttributes;
class ImageBitmap;
class OffscreenCanvasRenderingContext2DOrWebGLRenderingContextOrWebGL2RenderingContext;
typedef OffscreenCanvasRenderingContext2DOrWebGLRenderingContextOrWebGL2RenderingContext OffscreenRenderingContext;

class CORE_EXPORT OffscreenCanvas final : public GarbageCollected<OffscreenCanvas>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static OffscreenCanvas* create(unsigned width, unsigned height);

    // IDL attributes
    unsigned width() const { return m_size.width(); }
    unsigned height() const { return m_size.height(); }
    void setWidth(unsigned);
    void setHeight(unsigned);

    // API Methods
    ImageBitmap* transferToImageBitmap(ExceptionState&);

    IntSize size() const { return m_size; }
    void setAssociatedCanvasId(int canvasId) { m_canvasId = canvasId; }
    int getAssociatedCanvasId() const { return m_canvasId; }
    bool isNeutered() const { return m_isNeutered; }
    void setNeutered();
    CanvasRenderingContext* getCanvasRenderingContext(ScriptState*, const String&, const CanvasContextCreationAttributes&);
    CanvasRenderingContext* renderingContext() { return m_context; }

    static void registerRenderingContextFactory(std::unique_ptr<CanvasRenderingContextFactory>);

    bool originClean() const;
    void setOriginTainted() { m_originClean = false; }

    DECLARE_VIRTUAL_TRACE();

private:
    explicit OffscreenCanvas(const IntSize&);

    using ContextFactoryVector = Vector<std::unique_ptr<CanvasRenderingContextFactory>>;
    static ContextFactoryVector& renderingContextFactories();
    static CanvasRenderingContextFactory* getRenderingContextFactory(int);

    Member<CanvasRenderingContext> m_context;
    int m_canvasId = -1; // DOMNodeIds starts from 0, using -1 to indicate no associated canvas element.
    IntSize m_size;
    bool m_isNeutered = false;

    bool m_originClean;
};

} // namespace blink

#endif // OffscreenCanvas_h
