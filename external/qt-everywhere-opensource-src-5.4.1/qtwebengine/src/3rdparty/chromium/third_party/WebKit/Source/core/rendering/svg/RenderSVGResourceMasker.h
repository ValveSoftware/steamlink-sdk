/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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
 */

#ifndef RenderSVGResourceMasker_h
#define RenderSVGResourceMasker_h

#include "core/rendering/svg/RenderSVGResourceContainer.h"
#include "core/svg/SVGMaskElement.h"
#include "core/svg/SVGUnitTypes.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/ImageBuffer.h"

#include "wtf/HashMap.h"
#include "wtf/OwnPtr.h"

namespace WebCore {

class DisplayList;
class GraphicsContext;

class RenderSVGResourceMasker FINAL : public RenderSVGResourceContainer {
public:
    explicit RenderSVGResourceMasker(SVGMaskElement*);
    virtual ~RenderSVGResourceMasker();

    virtual const char* renderName() const OVERRIDE { return "RenderSVGResourceMasker"; }

    virtual void removeAllClientsFromCache(bool markForInvalidation = true) OVERRIDE;
    virtual void removeClientFromCache(RenderObject*, bool markForInvalidation = true) OVERRIDE;
    virtual bool applyResource(RenderObject*, RenderStyle*, GraphicsContext*&, unsigned short resourceMode) OVERRIDE;
    virtual void postApplyResource(RenderObject*, GraphicsContext*&, unsigned short, const Path*, const RenderSVGShape*) OVERRIDE;
    FloatRect resourceBoundingBox(const RenderObject*);

    SVGUnitTypes::SVGUnitType maskUnits() const { return toSVGMaskElement(element())->maskUnits()->currentValue()->enumValue(); }
    SVGUnitTypes::SVGUnitType maskContentUnits() const { return toSVGMaskElement(element())->maskContentUnits()->currentValue()->enumValue(); }

    virtual RenderSVGResourceType resourceType() const OVERRIDE { return s_resourceType; }
    static const RenderSVGResourceType s_resourceType;

private:
    void calculateMaskContentRepaintRect();
    void drawMaskForRenderer(GraphicsContext*, const FloatRect& targetBoundingBox);
    PassRefPtr<DisplayList> asDisplayList(GraphicsContext*, const AffineTransform&);

    RefPtr<DisplayList> m_maskContentDisplayList;
    FloatRect m_maskContentBoundaries;
};

DEFINE_RENDER_SVG_RESOURCE_TYPE_CASTS(RenderSVGResourceMasker, MaskerResourceType);

}

#endif
