/*
 * Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Rob Buis <buis@kde.org>
 * Copyright (C) 2014 Google, Inc.
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

#ifndef SVGSVGElement_h
#define SVGSVGElement_h

#include "core/svg/SVGAnimatedBoolean.h"
#include "core/svg/SVGAnimatedLength.h"
#include "core/svg/SVGFitToViewBox.h"
#include "core/svg/SVGGraphicsElement.h"
#include "core/svg/SVGLengthTearOff.h"
#include "core/svg/SVGPointTearOff.h"
#include "core/svg/SVGZoomAndPan.h"

namespace WebCore {

class SVGMatrixTearOff;
class SVGAngleTearOff;
class SVGNumberTearOff;
class SVGTransformTearOff;
class SVGViewSpec;
class SVGViewElement;
class SMILTimeContainer;

class SVGSVGElement FINAL : public SVGGraphicsElement,
                            public SVGFitToViewBox,
                            public SVGZoomAndPan {
public:
    DECLARE_NODE_FACTORY(SVGSVGElement);

#if !ENABLE(OILPAN)
    using SVGGraphicsElement::ref;
    using SVGGraphicsElement::deref;
#endif

    // 'SVGSVGElement' functions
    PassRefPtr<SVGRectTearOff> viewport() const;

    float pixelUnitToMillimeterX() const;
    float pixelUnitToMillimeterY() const;
    float screenPixelToMillimeterX() const;
    float screenPixelToMillimeterY() const;

    bool useCurrentView() const { return m_useCurrentView; }
    SVGViewSpec* currentView();

    Length intrinsicWidth() const;
    Length intrinsicHeight() const;
    FloatSize currentViewportSize() const;
    FloatRect currentViewBoxRect() const;

    float currentScale() const;
    void setCurrentScale(float scale);

    FloatPoint currentTranslate() { return m_translation->value(); }
    void setCurrentTranslate(const FloatPoint&);
    PassRefPtr<SVGPointTearOff> currentTranslateFromJavascript();

    SMILTimeContainer* timeContainer() const { return m_timeContainer.get(); }

    void pauseAnimations();
    void unpauseAnimations();
    bool animationsPaused() const;

    float getCurrentTime() const;
    void setCurrentTime(float seconds);

    // Stubs for the deprecated 'redraw' interface.
    unsigned suspendRedraw(unsigned) { return 1; }
    void unsuspendRedraw(unsigned) { }
    void unsuspendRedrawAll() { }
    void forceRedraw() { }

    PassRefPtrWillBeRawPtr<StaticNodeList> getIntersectionList(PassRefPtr<SVGRectTearOff>, SVGElement* referenceElement) const;
    PassRefPtrWillBeRawPtr<StaticNodeList> getEnclosureList(PassRefPtr<SVGRectTearOff>, SVGElement* referenceElement) const;
    bool checkIntersection(SVGElement*, PassRefPtr<SVGRectTearOff>) const;
    bool checkEnclosure(SVGElement*, PassRefPtr<SVGRectTearOff>) const;
    void deselectAll();

    static PassRefPtr<SVGNumberTearOff> createSVGNumber();
    static PassRefPtr<SVGLengthTearOff> createSVGLength();
    static PassRefPtr<SVGAngleTearOff> createSVGAngle();
    static PassRefPtr<SVGPointTearOff> createSVGPoint();
    static PassRefPtr<SVGMatrixTearOff> createSVGMatrix();
    static PassRefPtr<SVGRectTearOff> createSVGRect();
    static PassRefPtr<SVGTransformTearOff> createSVGTransform();
    static PassRefPtr<SVGTransformTearOff> createSVGTransformFromMatrix(PassRefPtr<SVGMatrixTearOff>);

    AffineTransform viewBoxToViewTransform(float viewWidth, float viewHeight) const;

    void setupInitialView(const String& fragmentIdentifier, Element* anchorNode);

    bool hasIntrinsicWidth() const;
    bool hasIntrinsicHeight() const;

    SVGAnimatedLength* x() const { return m_x.get(); }
    SVGAnimatedLength* y() const { return m_y.get(); }
    SVGAnimatedLength* width() const { return m_width.get(); }
    SVGAnimatedLength* height() const { return m_height.get(); }

    virtual void trace(Visitor*) OVERRIDE;

private:
    explicit SVGSVGElement(Document&);
    virtual ~SVGSVGElement();

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual bool isPresentationAttribute(const QualifiedName&) const OVERRIDE;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStylePropertySet*) OVERRIDE;

    virtual bool rendererIsNeeded(const RenderStyle&) OVERRIDE;
    virtual RenderObject* createRenderer(RenderStyle*) OVERRIDE;

    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void removedFrom(ContainerNode*) OVERRIDE;

    virtual void svgAttributeChanged(const QualifiedName&) OVERRIDE;

    virtual bool selfHasRelativeLengths() const OVERRIDE;

    void inheritViewAttributes(SVGViewElement*);

    void updateCurrentTranslate();

    virtual void finishParsingChildren() OVERRIDE;

    enum CheckIntersectionOrEnclosure {
        CheckIntersection,
        CheckEnclosure
    };

    bool checkIntersectionOrEnclosure(const SVGElement&, const FloatRect&, CheckIntersectionOrEnclosure) const;
    PassRefPtrWillBeRawPtr<StaticNodeList> collectIntersectionOrEnclosureList(const FloatRect&, SVGElement*, CheckIntersectionOrEnclosure) const;

    RefPtr<SVGAnimatedLength> m_x;
    RefPtr<SVGAnimatedLength> m_y;
    RefPtr<SVGAnimatedLength> m_width;
    RefPtr<SVGAnimatedLength> m_height;

    virtual AffineTransform localCoordinateSpaceTransform(SVGElement::CTMScope) const OVERRIDE;

    bool m_useCurrentView;
    RefPtrWillBeMember<SMILTimeContainer> m_timeContainer;
    RefPtr<SVGPoint> m_translation;
    RefPtrWillBeMember<SVGViewSpec> m_viewSpec;

    friend class SVGCurrentTranslateTearOff;
};

} // namespace WebCore

#endif
