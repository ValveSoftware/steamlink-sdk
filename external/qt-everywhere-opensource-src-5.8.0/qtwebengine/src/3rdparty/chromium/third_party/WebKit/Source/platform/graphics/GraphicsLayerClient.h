/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef GraphicsLayerClient_h
#define GraphicsLayerClient_h

#include "platform/PlatformExport.h"
#include "platform/geometry/LayoutSize.h"
#include "wtf/text/WTFString.h"

namespace blink {

class GraphicsContext;
class GraphicsLayer;
class IntRect;

enum GraphicsLayerPaintingPhaseFlags {
    GraphicsLayerPaintBackground = (1 << 0),
    GraphicsLayerPaintForeground = (1 << 1),
    GraphicsLayerPaintMask = (1 << 2),
    GraphicsLayerPaintOverflowContents = (1 << 3),
    GraphicsLayerPaintCompositedScroll = (1 << 4),
    GraphicsLayerPaintChildClippingMask = (1 << 5),
    GraphicsLayerPaintAllWithOverflowClip = (GraphicsLayerPaintBackground | GraphicsLayerPaintForeground | GraphicsLayerPaintMask)
};
typedef unsigned GraphicsLayerPaintingPhase;

enum {
    LayerTreeNormal = 0,
    LayerTreeIncludesDebugInfo = 1 << 0, // Dump extra debugging info like layer addresses.
    LayerTreeIncludesPaintInvalidations = 1 << 1,
    LayerTreeIncludesPaintingPhases = 1 << 2,
    LayerTreeIncludesRootLayer = 1 << 3,
    LayerTreeIncludesClipAndScrollParents = 1 << 4,
    LayerTreeIncludesCompositingReasons = 1 << 5,
};
typedef unsigned LayerTreeFlags;

class PLATFORM_EXPORT GraphicsLayerClient {
public:
    virtual ~GraphicsLayerClient() {}

    virtual void notifyFirstPaint() { }
    virtual void notifyFirstTextPaint() { }
    virtual void notifyFirstImagePaint() { }

    virtual IntRect computeInterestRect(const GraphicsLayer*, const IntRect& previousInterestRect) const = 0;
    virtual LayoutSize subpixelAccumulation() const { return LayoutSize(); }
    // Returns whether the client needs to be repainted with respect to the given graphics layer.
    virtual bool needsRepaint(const GraphicsLayer&) const = 0;
    virtual void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const IntRect& interestRect) const = 0;

    virtual bool isTrackingPaintInvalidations() const { return false; }

    virtual String debugName(const GraphicsLayer*) const = 0;

#if ENABLE(ASSERT)
    // CompositedLayerMapping overrides this to verify that it is not
    // currently painting contents. An ASSERT fails, if it is.
    // This is executed in GraphicsLayer construction and destruction
    // to verify that we don't create or destroy GraphicsLayers
    // while painting.
    virtual void verifyNotPainting() { }
#endif
};

} // namespace blink

#endif // GraphicsLayerClient_h
