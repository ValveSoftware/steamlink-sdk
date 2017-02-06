/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef SVGResources_h
#define SVGResources_h

#include "wtf/Allocator.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class ComputedStyle;
class LayoutObject;
class LayoutSVGResourceClipper;
class LayoutSVGResourceContainer;
class LayoutSVGResourceFilter;
class LayoutSVGResourceMarker;
class LayoutSVGResourceMasker;
class LayoutSVGResourcePaintServer;
class SVGElement;

// Holds a set of resources associated with a LayoutObject
class SVGResources {
    WTF_MAKE_NONCOPYABLE(SVGResources); USING_FAST_MALLOC(SVGResources);
public:
    SVGResources();

    static std::unique_ptr<SVGResources> buildResources(const LayoutObject*, const ComputedStyle&);
    void layoutIfNeeded();

    static bool supportsMarkers(const SVGElement&);

    // Ordinary resources
    LayoutSVGResourceClipper* clipper() const { return m_clipperFilterMaskerData ? m_clipperFilterMaskerData->clipper : 0; }
    LayoutSVGResourceMarker* markerStart() const { return m_markerData ? m_markerData->markerStart : 0; }
    LayoutSVGResourceMarker* markerMid() const { return m_markerData ? m_markerData->markerMid : 0; }
    LayoutSVGResourceMarker* markerEnd() const { return m_markerData ? m_markerData->markerEnd : 0; }
    LayoutSVGResourceMasker* masker() const { return m_clipperFilterMaskerData ? m_clipperFilterMaskerData->masker : 0; }

    LayoutSVGResourceFilter* filter() const
    {
        if (m_clipperFilterMaskerData)
            return m_clipperFilterMaskerData->filter;
        return nullptr;
    }

    // Paint servers
    LayoutSVGResourcePaintServer* fill() const { return m_fillStrokeData ? m_fillStrokeData->fill : 0; }
    LayoutSVGResourcePaintServer* stroke() const { return m_fillStrokeData ? m_fillStrokeData->stroke : 0; }

    // Chainable resources - linked through xlink:href
    LayoutSVGResourceContainer* linkedResource() const { return m_linkedResource; }

    void buildSetOfResources(HashSet<LayoutSVGResourceContainer*>&);

    // Methods operating on all cached resources
    void removeClientFromCache(LayoutObject*, bool markForInvalidation = true) const;
    void resourceDestroyed(LayoutSVGResourceContainer*);

#ifndef NDEBUG
    void dump(const LayoutObject*);
#endif

private:
    friend class SVGResourcesCycleSolver;

    bool hasResourceData() const;

    // Only used by SVGResourcesCache cycle detection logic
    void resetClipper();
    void resetFilter();
    void resetMarkerStart();
    void resetMarkerMid();
    void resetMarkerEnd();
    void resetMasker();
    void resetFill();
    void resetStroke();
    void resetLinkedResource();

    bool setClipper(LayoutSVGResourceClipper*);
    bool setFilter(LayoutSVGResourceFilter*);
    bool setMarkerStart(LayoutSVGResourceMarker*);
    bool setMarkerMid(LayoutSVGResourceMarker*);
    bool setMarkerEnd(LayoutSVGResourceMarker*);
    bool setMasker(LayoutSVGResourceMasker*);
    bool setFill(LayoutSVGResourcePaintServer*);
    bool setStroke(LayoutSVGResourcePaintServer*);
    bool setLinkedResource(LayoutSVGResourceContainer*);

    // From SVG 1.1 2nd Edition
    // clipper: 'container elements' and 'graphics elements'
    // filter:  'container elements' and 'graphics elements'
    // masker:  'container elements' and 'graphics elements'
    // -> a, circle, defs, ellipse, glyph, g, image, line, marker, mask, missing-glyph, path, pattern, polygon, polyline, rect, svg, switch, symbol, text, use
    struct ClipperFilterMaskerData {
        USING_FAST_MALLOC(ClipperFilterMaskerData);
    public:
        ClipperFilterMaskerData()
            : clipper(nullptr)
            , filter(nullptr)
            , masker(nullptr)
        {
        }

        static std::unique_ptr<ClipperFilterMaskerData> create()
        {
            return wrapUnique(new ClipperFilterMaskerData);
        }

        LayoutSVGResourceClipper* clipper;
        LayoutSVGResourceFilter* filter;
        LayoutSVGResourceMasker* masker;
    };

    // From SVG 1.1 2nd Edition
    // marker: line, path, polygon, polyline
    struct MarkerData {
        USING_FAST_MALLOC(MarkerData);
    public:
        MarkerData()
            : markerStart(nullptr)
            , markerMid(nullptr)
            , markerEnd(nullptr)
        {
        }

        static std::unique_ptr<MarkerData> create()
        {
            return wrapUnique(new MarkerData);
        }

        LayoutSVGResourceMarker* markerStart;
        LayoutSVGResourceMarker* markerMid;
        LayoutSVGResourceMarker* markerEnd;
    };

    // From SVG 1.1 2nd Edition
    // fill:       'shapes' and 'text content elements'
    // stroke:     'shapes' and 'text content elements'
    // -> circle, ellipse, line, path, polygon, polyline, rect, text, textPath, tspan
    struct FillStrokeData {
        USING_FAST_MALLOC(FillStrokeData);
    public:
        FillStrokeData()
            : fill(nullptr)
            , stroke(nullptr)
        {
        }

        static std::unique_ptr<FillStrokeData> create()
        {
            return wrapUnique(new FillStrokeData);
        }

        LayoutSVGResourcePaintServer* fill;
        LayoutSVGResourcePaintServer* stroke;
    };

    std::unique_ptr<ClipperFilterMaskerData> m_clipperFilterMaskerData;
    std::unique_ptr<MarkerData> m_markerData;
    std::unique_ptr<FillStrokeData> m_fillStrokeData;
    LayoutSVGResourceContainer* m_linkedResource;
};

} // namespace blink

#endif
