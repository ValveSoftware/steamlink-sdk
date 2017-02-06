/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef PaintInfo_h
#define PaintInfo_h

#include "core/CoreExport.h"
// TODO(jchaffraix): Once we unify PaintBehavior and PaintLayerFlags, we should move
// PaintLayerFlags to PaintPhase and rename it. Thus removing the need for this #include.#include "core/paint/PaintLayerPaintingInfo.h"
#include "core/paint/PaintLayerPaintingInfo.h"
#include "core/paint/PaintPhase.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/CullRect.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/transforms/AffineTransform.h"
#include "wtf/Allocator.h"
#include "wtf/HashMap.h"
#include "wtf/ListHashSet.h"

#include <limits>

namespace blink {

class LayoutInline;
class LayoutBoxModelObject;
class LayoutObject;
class PaintInvalidationState;

struct CORE_EXPORT PaintInfo {
    PaintInfo(GraphicsContext& newContext, const IntRect& cullRect, PaintPhase newPhase, GlobalPaintFlags globalPaintFlags,
        PaintLayerFlags paintFlags, const LayoutBoxModelObject* newPaintContainer = nullptr)
        : context(newContext)
        , phase(newPhase)
        , m_cullRect(cullRect)
        , m_paintContainer(newPaintContainer)
        , m_paintFlags(paintFlags)
        , m_globalPaintFlags(globalPaintFlags)
    {
    }

    PaintInfo(GraphicsContext& newContext, const PaintInfo& copyOtherFieldsFrom)
        : context(newContext)
        , phase(copyOtherFieldsFrom.phase)
        , m_cullRect(copyOtherFieldsFrom.m_cullRect)
        , m_paintContainer(copyOtherFieldsFrom.m_paintContainer)
        , m_paintFlags(copyOtherFieldsFrom.m_paintFlags)
        , m_globalPaintFlags(copyOtherFieldsFrom.m_globalPaintFlags)
    { }

    // Creates a PaintInfo for painting descendants. See comments about the paint phases
    // in PaintPhase.h for details.
    PaintInfo forDescendants() const
    {
        PaintInfo result(*this);
        if (phase == PaintPhaseDescendantOutlinesOnly)
            result.phase = PaintPhaseOutline;
        else if (phase == PaintPhaseDescendantBlockBackgroundsOnly)
            result.phase = PaintPhaseBlockBackground;
        return result;
    }

    bool isRenderingClipPathAsMaskImage() const { return m_paintFlags & PaintLayerPaintingRenderingClipPathAsMask; }

    bool skipRootBackground() const { return m_paintFlags & PaintLayerPaintingSkipRootBackground; }
    bool paintRootBackgroundOnly() const { return m_paintFlags & PaintLayerPaintingRootBackgroundOnly; }

    bool isPrinting() const { return m_globalPaintFlags & GlobalPaintPrinting; }

    DisplayItem::Type displayItemTypeForClipping() const { return DisplayItem::paintPhaseToClipBoxType(phase); }

    const LayoutBoxModelObject* paintContainer() const { return m_paintContainer; }

    GlobalPaintFlags getGlobalPaintFlags() const { return m_globalPaintFlags; }

    PaintLayerFlags paintFlags() const { return m_paintFlags; }

    const CullRect& cullRect() const { return m_cullRect; }

    void updateCullRect(const AffineTransform& localToParentTransform);

    // FIXME: Introduce setters/getters at some point. Requires a lot of changes throughout layout/.
    GraphicsContext& context;
    PaintPhase phase;

private:
    CullRect m_cullRect;
    const LayoutBoxModelObject* m_paintContainer; // the box model object that originates the current painting

    const PaintLayerFlags m_paintFlags;
    const GlobalPaintFlags m_globalPaintFlags;

    // TODO(chrishtr): temporary while we implement CullRect everywhere.
    friend class SVGPaintContext;
    friend class SVGShapePainter;
};

} // namespace blink

#endif // PaintInfo_h
