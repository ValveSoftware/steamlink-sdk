/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#ifndef SVGMarkerElement_h
#define SVGMarkerElement_h

#include "bindings/v8/ExceptionState.h"
#include "core/svg/SVGAnimatedAngle.h"
#include "core/svg/SVGAnimatedBoolean.h"
#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGAnimatedLength.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGFitToViewBox.h"

namespace WebCore {

enum SVGMarkerUnitsType {
    SVGMarkerUnitsUnknown = 0,
    SVGMarkerUnitsUserSpaceOnUse,
    SVGMarkerUnitsStrokeWidth
};
template<> const SVGEnumerationStringEntries& getStaticStringEntries<SVGMarkerUnitsType>();

class SVGMarkerElement FINAL : public SVGElement,
                               public SVGFitToViewBox {
public:
    // Forward declare enumerations in the W3C naming scheme, for IDL generation.
    enum {
        SVG_MARKERUNITS_UNKNOWN = SVGMarkerUnitsUnknown,
        SVG_MARKERUNITS_USERSPACEONUSE = SVGMarkerUnitsUserSpaceOnUse,
        SVG_MARKERUNITS_STROKEWIDTH = SVGMarkerUnitsStrokeWidth
    };

    enum {
        SVG_MARKER_ORIENT_UNKNOWN = SVGMarkerOrientUnknown,
        SVG_MARKER_ORIENT_AUTO = SVGMarkerOrientAuto,
        SVG_MARKER_ORIENT_ANGLE = SVGMarkerOrientAngle
    };

    DECLARE_NODE_FACTORY(SVGMarkerElement);

    AffineTransform viewBoxToViewTransform(float viewWidth, float viewHeight) const;

    void setOrientToAuto();
    void setOrientToAngle(PassRefPtr<SVGAngleTearOff>);

    SVGAnimatedLength* refX() const { return m_refX.get(); }
    SVGAnimatedLength* refY() const { return m_refY.get(); }
    SVGAnimatedLength* markerWidth() const { return m_markerWidth.get(); }
    SVGAnimatedLength* markerHeight() const { return m_markerHeight.get(); }
    SVGAnimatedEnumeration<SVGMarkerUnitsType>* markerUnits() { return m_markerUnits.get(); }
    SVGAnimatedAngle* orientAngle() { return m_orientAngle.get(); }
    SVGAnimatedEnumeration<SVGMarkerOrientType>* orientType() { return m_orientAngle->orientType(); }

private:
    explicit SVGMarkerElement(Document&);

    virtual bool needsPendingResourceHandling() const OVERRIDE { return false; }

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&) OVERRIDE;
    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0) OVERRIDE;

    virtual RenderObject* createRenderer(RenderStyle*) OVERRIDE;
    virtual bool rendererIsNeeded(const RenderStyle&) OVERRIDE { return true; }

    virtual bool selfHasRelativeLengths() const OVERRIDE;

    RefPtr<SVGAnimatedLength> m_refX;
    RefPtr<SVGAnimatedLength> m_refY;
    RefPtr<SVGAnimatedLength> m_markerWidth;
    RefPtr<SVGAnimatedLength> m_markerHeight;
    RefPtr<SVGAnimatedAngle> m_orientAngle;
    RefPtr<SVGAnimatedEnumeration<SVGMarkerUnitsType> > m_markerUnits;
};

}

#endif
