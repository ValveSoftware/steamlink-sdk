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

#ifndef LayoutSVGResourceClipper_h
#define LayoutSVGResourceClipper_h

#include "core/layout/svg/LayoutSVGResourceContainer.h"
#include "core/svg/SVGClipPathElement.h"

class SkPicture;

namespace blink {

class LayoutSVGResourceClipper final : public LayoutSVGResourceContainer {
public:
    explicit LayoutSVGResourceClipper(SVGClipPathElement*);
    ~LayoutSVGResourceClipper() override;

    const char* name() const override { return "LayoutSVGResourceClipper"; }

    void removeAllClientsFromCache(bool markForInvalidation = true) override;
    void removeClientFromCache(LayoutObject*, bool markForInvalidation = true) override;

    FloatRect resourceBoundingBox(const LayoutObject*);

    static const LayoutSVGResourceType s_resourceType = ClipperResourceType;
    LayoutSVGResourceType resourceType() const override { return s_resourceType; }

    bool hitTestClipContent(const FloatRect&, const FloatPoint&);

    SVGUnitTypes::SVGUnitType clipPathUnits() const { return toSVGClipPathElement(element())->clipPathUnits()->currentValue()->enumValue(); }

    bool asPath(const AffineTransform&, const FloatRect& referenceBox, Path&);
    PassRefPtr<const SkPicture> createContentPicture();

    bool hasCycle() { return m_inClipExpansion; }
    void beginClipExpansion() { ASSERT(!m_inClipExpansion); m_inClipExpansion = true; }
    void endClipExpansion() { ASSERT(m_inClipExpansion); m_inClipExpansion = false; }
private:
    void calculateLocalClipBounds();

    // Return true if the clip path was calculated or a cached value is available.
    bool calculateClipContentPathIfNeeded();

    // Cache of the clip path when using path clipping.
    Path m_clipContentPath;

    // Cache of the clip path picture when falling back to masking for clipping.
    RefPtr<const SkPicture> m_clipContentPicture;

    FloatRect m_localClipBounds;

    // Reference cycle detection.
    bool m_inClipExpansion;
};

DEFINE_LAYOUT_SVG_RESOURCE_TYPE_CASTS(LayoutSVGResourceClipper, ClipperResourceType);

} // namespace blink

#endif
