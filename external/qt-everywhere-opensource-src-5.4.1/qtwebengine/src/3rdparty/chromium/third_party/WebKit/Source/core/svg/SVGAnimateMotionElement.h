/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef SVGAnimateMotionElement_h
#define SVGAnimateMotionElement_h

#include "core/svg/SVGAnimationElement.h"
#include "platform/graphics/Path.h"

namespace WebCore {

class SVGAnimateMotionElement FINAL : public SVGAnimationElement {
public:
    virtual ~SVGAnimateMotionElement();

    DECLARE_NODE_FACTORY(SVGAnimateMotionElement);
    void updateAnimationPath();

private:
    explicit SVGAnimateMotionElement(Document&);

    virtual bool hasValidAttributeType() OVERRIDE;
    virtual bool hasValidAttributeName() OVERRIDE;

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;

    virtual void resetAnimatedType() OVERRIDE;
    virtual void clearAnimatedType(SVGElement* targetElement) OVERRIDE;
    virtual bool calculateToAtEndOfDurationValue(const String& toAtEndOfDurationString) OVERRIDE;
    virtual bool calculateFromAndToValues(const String& fromString, const String& toString) OVERRIDE;
    virtual bool calculateFromAndByValues(const String& fromString, const String& byString) OVERRIDE;
    virtual void calculateAnimatedValue(float percentage, unsigned repeatCount, SVGSMILElement* resultElement) OVERRIDE;
    virtual void applyResultsToTarget() OVERRIDE;
    virtual float calculateDistance(const String& fromString, const String& toString) OVERRIDE;

    enum RotateMode {
        RotateAngle,
        RotateAuto,
        RotateAutoReverse
    };
    RotateMode rotateMode() const;

    bool m_hasToPointAtEndOfDuration;

    virtual void updateAnimationMode() OVERRIDE;

    // Note: we do not support percentage values for to/from coords as the spec implies we should (opera doesn't either)
    FloatPoint m_fromPoint;
    FloatPoint m_toPoint;
    FloatPoint m_toPointAtEndOfDuration;

    Path m_path;
    Path m_animationPath;
};

} // namespace WebCore

#endif // SVGAnimateMotionElement_h
