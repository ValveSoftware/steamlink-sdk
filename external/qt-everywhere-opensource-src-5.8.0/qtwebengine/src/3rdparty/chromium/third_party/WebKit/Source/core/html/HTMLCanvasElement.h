/*
 * Copyright (C) 2004, 2006, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HTMLCanvasElement_h
#define HTMLCanvasElement_h

#include "bindings/core/v8/ScriptValue.h"
#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/DOMTypedArray.h"
#include "core/dom/Document.h"
#include "core/fileapi/BlobCallback.h"
#include "core/html/HTMLElement.h"
#include "core/html/canvas/CanvasDrawListener.h"
#include "core/html/canvas/CanvasImageSource.h"
#include "core/imagebitmap/ImageBitmapSource.h"
#include "core/page/PageLifecycleObserver.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/CanvasSurfaceLayerBridge.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/GraphicsTypes3D.h"
#include "platform/graphics/ImageBufferClient.h"
#include "platform/heap/Handle.h"
#include <memory>

#define CanvasDefaultInterpolationQuality InterpolationLow

namespace blink {

class AffineTransform;
class CanvasContextCreationAttributes;
class CanvasRenderingContext;
class CanvasRenderingContextFactory;
class GraphicsContext;
class HTMLCanvasElement;
class Image;
class ImageBitmapOptions;
class ImageBuffer;
class ImageBufferSurface;
class ImageData;
class IntSize;

class CanvasRenderingContext2DOrWebGLRenderingContextOrWebGL2RenderingContextOrImageBitmapRenderingContext;
typedef CanvasRenderingContext2DOrWebGLRenderingContextOrWebGL2RenderingContextOrImageBitmapRenderingContext RenderingContext;

class CORE_EXPORT HTMLCanvasElement final : public HTMLElement, public ContextLifecycleObserver, public PageLifecycleObserver, public CanvasImageSource, public ImageBufferClient, public ImageBitmapSource {
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(HTMLCanvasElement);
    USING_PRE_FINALIZER(HTMLCanvasElement, dispose);
public:
    using Node::getExecutionContext;

    DECLARE_NODE_FACTORY(HTMLCanvasElement);
    ~HTMLCanvasElement() override;

    // Attributes and functions exposed to script
    int width() const { return size().width(); }
    int height() const { return size().height(); }

    const IntSize& size() const { return m_size; }

    void setWidth(int);
    void setHeight(int);

    void setSize(const IntSize& newSize)
    {
        if (newSize == size())
            return;
        m_ignoreReset = true;
        setWidth(newSize.width());
        setHeight(newSize.height());
        m_ignoreReset = false;
        reset();
    }

    // Called by Document::getCSSCanvasContext as well as above getContext().
    CanvasRenderingContext* getCanvasRenderingContext(const String&, const CanvasContextCreationAttributes&);

    bool isPaintable() const;

    enum EncodeReason {
        EncodeReasonToDataURL = 0,
        EncodeReasonToBlobCallback = 1,
        NumberOfEncodeReasons
    };
    static String toEncodingMimeType(const String& mimeType, const EncodeReason);
    String toDataURL(const String& mimeType, const ScriptValue& qualityArgument, ExceptionState&) const;
    String toDataURL(const String& mimeType, ExceptionState& exceptionState) const { return toDataURL(mimeType, ScriptValue(), exceptionState); }

    void toBlob(BlobCallback*, const String& mimeType, const ScriptValue& qualityArgument, ExceptionState&);
    void toBlob(BlobCallback* callback, const String& mimeType, ExceptionState& exceptionState) { return toBlob(callback, mimeType, ScriptValue(), exceptionState); }

    // Used for canvas capture.
    void addListener(CanvasDrawListener*);
    void removeListener(CanvasDrawListener*);

    // Used for rendering
    void didDraw(const FloatRect&);

    void paint(GraphicsContext&, const LayoutRect&);

    SkCanvas* drawingCanvas() const;
    void disableDeferral(DisableDeferralReason) const;
    SkCanvas* existingDrawingCanvas() const;

    CanvasRenderingContext* renderingContext() const { return m_context.get(); }

    void ensureUnacceleratedImageBuffer();
    ImageBuffer* buffer() const;
    PassRefPtr<Image> copiedImage(SourceDrawingBuffer, AccelerationHint) const;
    void clearCopiedImage();

    SecurityOrigin* getSecurityOrigin() const;
    bool originClean() const;
    void setOriginTainted() { m_originClean = false; }

    AffineTransform baseTransform() const;

    bool is3D() const;
    bool isAnimated2D() const;

    bool hasImageBuffer() const { return m_imageBuffer.get(); }
    void discardImageBuffer();

    bool shouldAccelerate(const IntSize&) const;

    bool shouldBeDirectComposited() const;

    void prepareSurfaceForPaintingIfNeeded() const;

    const AtomicString imageSourceURL() const override;

    InsertionNotificationRequest insertedInto(ContainerNode*) override;

    // ContextLifecycleObserver (and PageLifecycleObserver!!!) implementation
    void contextDestroyed() override;

    // PageLifecycleObserver implementation
    void pageVisibilityChanged() override;

    // CanvasImageSource implementation
    PassRefPtr<Image> getSourceImageForCanvas(SourceImageStatus*, AccelerationHint, SnapshotReason, const FloatSize&) const override;
    bool wouldTaintOrigin(SecurityOrigin*) const override;
    FloatSize elementSize(const FloatSize&) const override;
    bool isCanvasElement() const override { return true; }
    bool isOpaque() const override;
    int sourceWidth() override { return m_size.width(); }
    int sourceHeight() override { return m_size.height(); }

    // ImageBufferClient implementation
    void notifySurfaceInvalid() override;
    bool isDirty() override { return !m_dirtyRect.isEmpty(); }
    void didFinalizeFrame() override;
    void restoreCanvasMatrixClipStack(SkCanvas*) const override;

    void doDeferredPaintInvalidation();

    // ImageBitmapSource implementation
    IntSize bitmapSourceSize() const override;
    ScriptPromise createImageBitmap(ScriptState*, EventTarget&, int sx, int sy, int sw, int sh, const ImageBitmapOptions&, ExceptionState&) override;

    DECLARE_VIRTUAL_TRACE();

    DECLARE_VIRTUAL_TRACE_WRAPPERS();

    void createImageBufferUsingSurfaceForTesting(std::unique_ptr<ImageBufferSurface>);

    static void registerRenderingContextFactory(std::unique_ptr<CanvasRenderingContextFactory>);
    void updateExternallyAllocatedMemory() const;

    void styleDidChange(const ComputedStyle* oldStyle, const ComputedStyle& newStyle);

    void notifyListenersCanvasChanged();

    // For Canvas HitRegions
    bool isSupportedInteractiveCanvasFallback(const Element&);
    std::pair<Element*, String> getControlAndIdIfHitRegionExists(const LayoutPoint&);
    String getIdFromControl(const Element*);

    // For OffscreenCanvas that controls this html canvas element
    CanvasSurfaceLayerBridge* surfaceLayerBridge() const { return m_surfaceLayerBridge.get(); }
    bool createSurfaceLayer();

    void detachContext() { m_context = nullptr; }

protected:
    void didMoveToNewDocument(Document& oldDocument) override;

private:
    explicit HTMLCanvasElement(Document&);
    void dispose();

    using ContextFactoryVector = Vector<std::unique_ptr<CanvasRenderingContextFactory>>;
    static ContextFactoryVector& renderingContextFactories();
    static CanvasRenderingContextFactory* getRenderingContextFactory(int);

    void parseAttribute(const QualifiedName&, const AtomicString&, const AtomicString&) override;
    LayoutObject* createLayoutObject(const ComputedStyle&) override;
    bool areAuthorShadowsAllowed() const override { return false; }

    void reset();

    std::unique_ptr<ImageBufferSurface> createImageBufferSurface(const IntSize& deviceSize, int* msaaSampleCount);
    void createImageBuffer();
    void createImageBufferInternal(std::unique_ptr<ImageBufferSurface> externalSurface);
    bool shouldUseDisplayList(const IntSize& deviceSize);

    void setSurfaceSize(const IntSize&);

    bool paintsIntoCanvasBuffer() const;

    ImageData* toImageData(SourceDrawingBuffer, SnapshotReason) const;

    String toDataURLInternal(const String& mimeType, const double& quality, SourceDrawingBuffer) const;

    HeapHashSet<WeakMember<CanvasDrawListener>> m_listeners;

    IntSize m_size;

    Member<CanvasRenderingContext> m_context;

    bool m_ignoreReset;
    FloatRect m_dirtyRect;

    mutable intptr_t m_externallyAllocatedMemory;

    bool m_originClean;

    // It prevents HTMLCanvasElement::buffer() from continuously re-attempting to allocate an imageBuffer
    // after the first attempt failed.
    mutable bool m_didFailToCreateImageBuffer;
    bool m_imageBufferIsClear;
    std::unique_ptr<ImageBuffer> m_imageBuffer;

    mutable RefPtr<Image> m_copiedImage; // FIXME: This is temporary for platforms that have to copy the image buffer to render (and for CSSCanvasValue).

    // Used for OffscreenCanvas that controls this HTML canvas element
    std::unique_ptr<CanvasSurfaceLayerBridge> m_surfaceLayerBridge;
};

} // namespace blink

#endif // HTMLCanvasElement_h
