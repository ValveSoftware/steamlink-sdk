/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#ifndef SVGAnimateElement_h
#define SVGAnimateElement_h

#include "core/SVGNames.h"
#include "core/svg/SVGAnimatedTypeAnimator.h"
#include "core/svg/SVGAnimationElement.h"
#include "wtf/OwnPtr.h"

namespace WebCore {

class SVGAnimatedTypeAnimator;

class SVGAnimateElement : public SVGAnimationElement {
public:
    static PassRefPtrWillBeRawPtr<SVGAnimateElement> create(Document&);
    virtual ~SVGAnimateElement();

    virtual void trace(Visitor*) OVERRIDE;

    AnimatedPropertyType animatedPropertyType();
    bool animatedPropertyTypeSupportsAddition();

protected:
    SVGAnimateElement(const QualifiedName&, Document&);

    virtual void resetAnimatedType() OVERRIDE FINAL;
    virtual void clearAnimatedType(SVGElement* targetElement) OVERRIDE FINAL;

    virtual bool calculateToAtEndOfDurationValue(const String& toAtEndOfDurationString) OVERRIDE FINAL;
    virtual bool calculateFromAndToValues(const String& fromString, const String& toString) OVERRIDE FINAL;
    virtual bool calculateFromAndByValues(const String& fromString, const String& byString) OVERRIDE FINAL;
    virtual void calculateAnimatedValue(float percentage, unsigned repeatCount, SVGSMILElement* resultElement) OVERRIDE FINAL;
    virtual void applyResultsToTarget() OVERRIDE FINAL;
    virtual float calculateDistance(const String& fromString, const String& toString) OVERRIDE FINAL;
    virtual bool isAdditive() OVERRIDE FINAL;

    virtual void setTargetElement(SVGElement*) OVERRIDE FINAL;
    virtual void setAttributeName(const QualifiedName&) OVERRIDE FINAL;

private:
    void resetAnimatedPropertyType();
    SVGAnimatedTypeAnimator* ensureAnimator();

    virtual bool hasValidAttributeType() OVERRIDE;

    RefPtr<SVGPropertyBase> m_fromProperty;
    RefPtr<SVGPropertyBase> m_toProperty;
    RefPtr<SVGPropertyBase> m_toAtEndOfDurationProperty;
    RefPtr<SVGPropertyBase> m_animatedProperty;

    OwnPtrWillBeMember<SVGAnimatedTypeAnimator> m_animator;
};

inline bool isSVGAnimateElement(const Node& node)
{
    return node.hasTagName(SVGNames::animateTag)
        || node.hasTagName(SVGNames::animateTransformTag)
        || node.hasTagName(SVGNames::setTag);
}

DEFINE_ELEMENT_TYPE_CASTS_WITH_FUNCTION(SVGAnimateElement);

} // namespace WebCore

#endif // SVGAnimateElement_h
