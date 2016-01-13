/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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

#ifndef GraphicsLayer_h
#define GraphicsLayer_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatPoint3D.h"
#include "platform/geometry/FloatSize.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/GraphicsLayerClient.h"
#include "platform/graphics/GraphicsLayerDebugInfo.h"
#include "platform/graphics/OpaqueRectTrackingContentLayerDelegate.h"
#include "platform/graphics/filters/FilterOperations.h"
#include "platform/transforms/TransformationMatrix.h"
#include "public/platform/WebAnimationDelegate.h"
#include "public/platform/WebContentLayer.h"
#include "public/platform/WebImageLayer.h"
#include "public/platform/WebLayerClient.h"
#include "public/platform/WebLayerScrollClient.h"
#include "public/platform/WebNinePatchLayer.h"
#include "public/platform/WebSolidColorLayer.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"

namespace blink {
class GraphicsLayerFactoryChromium;
class WebAnimation;
class WebLayer;
}

namespace WebCore {

class FloatRect;
class GraphicsContext;
class GraphicsLayer;
class GraphicsLayerFactory;
class Image;
class ScrollableArea;
class TextStream;

// FIXME: find a better home for this declaration.
class PLATFORM_EXPORT LinkHighlightClient {
public:
    virtual void invalidate() = 0;
    virtual void clearCurrentGraphicsLayer() = 0;
    virtual blink::WebLayer* layer() = 0;

protected:
    virtual ~LinkHighlightClient() { }
};

typedef Vector<GraphicsLayer*, 64> GraphicsLayerVector;

// GraphicsLayer is an abstraction for a rendering surface with backing store,
// which may have associated transformation and animations.

class PLATFORM_EXPORT GraphicsLayer : public GraphicsContextPainter, public blink::WebAnimationDelegate, public blink::WebLayerScrollClient, public blink::WebLayerClient {
    WTF_MAKE_NONCOPYABLE(GraphicsLayer); WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<GraphicsLayer> create(GraphicsLayerFactory*, GraphicsLayerClient*);

    virtual ~GraphicsLayer();

    GraphicsLayerClient* client() const { return m_client; }

    // blink::WebLayerClient implementation.
    virtual blink::WebGraphicsLayerDebugInfo* takeDebugInfoFor(blink::WebLayer*) OVERRIDE;

    GraphicsLayerDebugInfo& debugInfo();

    void setCompositingReasons(CompositingReasons);
    CompositingReasons compositingReasons() const { return m_debugInfo.compositingReasons(); }
    void setOwnerNodeId(int);

    GraphicsLayer* parent() const { return m_parent; };
    void setParent(GraphicsLayer*); // Internal use only.

    const Vector<GraphicsLayer*>& children() const { return m_children; }
    // Returns true if the child list changed.
    bool setChildren(const GraphicsLayerVector&);

    // Add child layers. If the child is already parented, it will be removed from its old parent.
    void addChild(GraphicsLayer*);
    void addChildAtIndex(GraphicsLayer*, int index);
    void addChildAbove(GraphicsLayer*, GraphicsLayer* sibling);
    void addChildBelow(GraphicsLayer*, GraphicsLayer* sibling);
    bool replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild);

    void removeAllChildren();
    void removeFromParent();

    GraphicsLayer* maskLayer() const { return m_maskLayer; }
    void setMaskLayer(GraphicsLayer*);

    GraphicsLayer* contentsClippingMaskLayer() const { return m_contentsClippingMaskLayer; }
    void setContentsClippingMaskLayer(GraphicsLayer*);

    // The given layer will replicate this layer and its children; the replica renders behind this layer.
    void setReplicatedByLayer(GraphicsLayer*);
    // The layer that replicates this layer (if any).
    GraphicsLayer* replicaLayer() const { return m_replicaLayer; }
    // The layer being replicated.
    GraphicsLayer* replicatedLayer() const { return m_replicatedLayer; }

    enum ShouldSetNeedsDisplay {
        DontSetNeedsDisplay,
        SetNeedsDisplay
    };

    // Offset is origin of the renderer minus origin of the graphics layer (so either zero or negative).
    IntSize offsetFromRenderer() const { return m_offsetFromRenderer; }
    void setOffsetFromRenderer(const IntSize&, ShouldSetNeedsDisplay = SetNeedsDisplay);

    // The position of the layer (the location of its top-left corner in its parent)
    const FloatPoint& position() const { return m_position; }
    void setPosition(const FloatPoint&);

    const FloatPoint3D& transformOrigin() const { return m_transformOrigin; }
    void setTransformOrigin(const FloatPoint3D&);

    // The size of the layer.
    const FloatSize& size() const { return m_size; }
    void setSize(const FloatSize&);

    // The boundOrigin affects the offset at which content is rendered, and sublayers are positioned.
    const FloatPoint& boundsOrigin() const { return m_boundsOrigin; }
    void setBoundsOrigin(const FloatPoint& origin) { m_boundsOrigin = origin; }

    const TransformationMatrix& transform() const { return m_transform; }
    void setTransform(const TransformationMatrix&);
    void setShouldFlattenTransform(bool);
    void setRenderingContext(int id);
    void setMasksToBounds(bool);

    bool drawsContent() const { return m_drawsContent; }
    void setDrawsContent(bool);

    bool contentsAreVisible() const { return m_contentsVisible; }
    void setContentsVisible(bool);

    void setScrollParent(blink::WebLayer*);
    void setClipParent(blink::WebLayer*);

    // For special cases, e.g. drawing missing tiles on Android.
    // The compositor should never paint this color in normal cases because the RenderLayer
    // will paint background by itself.
    void setBackgroundColor(const Color&);

    // opaque means that we know the layer contents have no alpha
    bool contentsOpaque() const { return m_contentsOpaque; }
    void setContentsOpaque(bool);

    bool backfaceVisibility() const { return m_backfaceVisibility; }
    void setBackfaceVisibility(bool visible);

    float opacity() const { return m_opacity; }
    void setOpacity(float);

    void setBlendMode(blink::WebBlendMode);
    void setIsRootForIsolatedGroup(bool);

    // Returns true if filter can be rendered by the compositor
    bool setFilters(const FilterOperations&);
    void setBackgroundFilters(const FilterOperations&);

    // Some GraphicsLayers paint only the foreground or the background content
    void setPaintingPhase(GraphicsLayerPaintingPhase);

    void setNeedsDisplay();
    // mark the given rect (in layer coords) as needing dispay. Never goes deep.
    void setNeedsDisplayInRect(const FloatRect&);

    void setContentsNeedsDisplay();

    // Set that the position/size of the contents (image or video).
    void setContentsRect(const IntRect&);

    // Return true if the animation is handled by the compositing system. If this returns
    // false, the animation will be run by AnimationController.
    // These methods handle both transitions and keyframe animations.
    bool addAnimation(PassOwnPtr<blink::WebAnimation>);
    void pauseAnimation(int animationId, double /*timeOffset*/);
    void removeAnimation(int animationId);

    // Layer contents
    void setContentsToImage(Image*);
    void setContentsToNinePatch(Image*, const IntRect& aperture);
    void setContentsToPlatformLayer(blink::WebLayer* layer) { setContentsTo(layer); }
    bool hasContentsLayer() const { return m_contentsLayer; }

    // Callback from the underlying graphics system to draw layer contents.
    void paintGraphicsLayerContents(GraphicsContext&, const IntRect& clip);

    // For hosting this GraphicsLayer in a native layer hierarchy.
    blink::WebLayer* platformLayer() const;

    typedef HashMap<int, int> RenderingContextMap;
    void dumpLayer(TextStream&, int indent, LayerTreeFlags, RenderingContextMap&) const;

    int paintCount() const { return m_paintCount; }

    // Return a string with a human readable form of the layer tree, If debug is true
    // pointers for the layers and timing data will be included in the returned string.
    String layerTreeAsText(LayerTreeFlags = LayerTreeNormal) const;
    String debugName(blink::WebLayer*) const;

    void resetTrackedRepaints();
    void addRepaintRect(const FloatRect&);

    void collectTrackedRepaintRects(Vector<FloatRect>&) const;

    void addLinkHighlight(LinkHighlightClient*);
    void removeLinkHighlight(LinkHighlightClient*);
    // Exposed for tests
    unsigned numLinkHighlights() { return m_linkHighlights.size(); }
    LinkHighlightClient* linkHighlight(int i) { return m_linkHighlights[i]; }

    void setScrollableArea(ScrollableArea*, bool isMainFrame);
    ScrollableArea* scrollableArea() const { return m_scrollableArea; }

    blink::WebContentLayer* contentLayer() const { return m_layer.get(); }

    static void registerContentsLayer(blink::WebLayer*);
    static void unregisterContentsLayer(blink::WebLayer*);

    // GraphicsContextPainter implementation.
    virtual void paint(GraphicsContext&, const IntRect& clip) OVERRIDE;

    // WebAnimationDelegate implementation.
    virtual void notifyAnimationStarted(double monotonicTime, blink::WebAnimation::TargetProperty) OVERRIDE;
    virtual void notifyAnimationFinished(double monotonicTime, blink::WebAnimation::TargetProperty) OVERRIDE;

    // WebLayerScrollClient implementation.
    virtual void didScroll() OVERRIDE;

protected:
    explicit GraphicsLayer(GraphicsLayerClient*);
    // GraphicsLayerFactoryChromium that wants to create a GraphicsLayer need to be friends.
    friend class blink::GraphicsLayerFactoryChromium;

    // Exposed for tests.
    virtual blink::WebLayer* contentsLayer() const { return m_contentsLayer; }

private:
    // Adds a child without calling updateChildList(), so that adding children
    // can be batched before updating.
    void addChildInternal(GraphicsLayer*);

#if ASSERT_ENABLED
    bool hasAncestor(GraphicsLayer*) const;
#endif

    // This method is used by platform GraphicsLayer classes to clear the filters
    // when compositing is not done in hardware. It is not virtual, so the caller
    // needs to notifiy the change to the platform layer as needed.
    void clearFilters() { m_filters.clear(); }

    void setReplicatedLayer(GraphicsLayer* layer) { m_replicatedLayer = layer; }

    int incrementPaintCount() { return ++m_paintCount; }

    void dumpProperties(TextStream&, int indent, LayerTreeFlags, RenderingContextMap&) const;

    // Helper functions used by settors to keep layer's the state consistent.
    void updateChildList();
    void updateLayerIsDrawable();
    void updateContentsRect();

    void setContentsTo(blink::WebLayer*);
    void setupContentsLayer(blink::WebLayer*);
    void clearContentsLayerIfUnregistered();
    blink::WebLayer* contentsLayerIfRegistered();

    GraphicsLayerClient* m_client;

    // Offset from the owning renderer
    IntSize m_offsetFromRenderer;

    // Position is relative to the parent GraphicsLayer
    FloatPoint m_position;
    FloatSize m_size;
    FloatPoint m_boundsOrigin;

    TransformationMatrix m_transform;
    FloatPoint3D m_transformOrigin;

    Color m_backgroundColor;
    float m_opacity;

    blink::WebBlendMode m_blendMode;

    FilterOperations m_filters;

    bool m_hasTransformOrigin : 1;
    bool m_contentsOpaque : 1;
    bool m_shouldFlattenTransform: 1;
    bool m_backfaceVisibility : 1;
    bool m_masksToBounds : 1;
    bool m_drawsContent : 1;
    bool m_contentsVisible : 1;
    bool m_isRootForIsolatedGroup : 1;

    bool m_hasScrollParent : 1;
    bool m_hasClipParent : 1;

    GraphicsLayerPaintingPhase m_paintingPhase;

    Vector<GraphicsLayer*> m_children;
    GraphicsLayer* m_parent;

    GraphicsLayer* m_maskLayer; // Reference to mask layer. We don't own this.
    GraphicsLayer* m_contentsClippingMaskLayer; // Reference to clipping mask layer. We don't own this.

    GraphicsLayer* m_replicaLayer; // A layer that replicates this layer. We only allow one, for now.
                                   // The replica is not parented; this is the primary reference to it.
    GraphicsLayer* m_replicatedLayer; // For a replica layer, a reference to the original layer.
    FloatPoint m_replicatedLayerPosition; // For a replica layer, the position of the replica.

    IntRect m_contentsRect;

    int m_paintCount;

    OwnPtr<blink::WebContentLayer> m_layer;
    OwnPtr<blink::WebImageLayer> m_imageLayer;
    OwnPtr<blink::WebNinePatchLayer> m_ninePatchLayer;
    blink::WebLayer* m_contentsLayer;
    // We don't have ownership of m_contentsLayer, but we do want to know if a given layer is the
    // same as our current layer in setContentsTo(). Since m_contentsLayer may be deleted at this point,
    // we stash an ID away when we know m_contentsLayer is alive and use that for comparisons from that point
    // on.
    int m_contentsLayerId;

    Vector<LinkHighlightClient*> m_linkHighlights;

    OwnPtr<OpaqueRectTrackingContentLayerDelegate> m_opaqueRectTrackingContentLayerDelegate;

    ScrollableArea* m_scrollableArea;
    GraphicsLayerDebugInfo m_debugInfo;
    int m_3dRenderingContext;
};

} // namespace WebCore

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void PLATFORM_EXPORT showGraphicsLayerTree(const WebCore::GraphicsLayer*);
#endif

#endif // GraphicsLayer_h
