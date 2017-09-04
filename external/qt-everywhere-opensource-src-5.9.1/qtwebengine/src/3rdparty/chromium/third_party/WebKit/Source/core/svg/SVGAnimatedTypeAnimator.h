/*
 * Copyright (C) Research In Motion Limited 2011-2012. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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

#ifndef SVGAnimatedTypeAnimator_h
#define SVGAnimatedTypeAnimator_h

#include "core/CSSPropertyNames.h"
#include "core/svg/properties/SVGPropertyInfo.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace blink {

class SVGAnimatedPropertyBase;
class SVGPropertyBase;
class SVGElement;
class SVGAnimationElement;

class SVGAnimatedTypeAnimator final {
  DISALLOW_NEW();

 public:
  SVGAnimatedTypeAnimator(SVGAnimationElement*);

  void clear();
  void reset(const SVGElement&);

  SVGPropertyBase* createAnimatedValue() const;
  SVGPropertyBase* createPropertyForAnimation(const String&) const;

  AnimatedPropertyType type() const { return m_type; }
  CSSPropertyID cssProperty() const { return m_cssProperty; }

  bool isAnimatingSVGDom() const { return m_animatedProperty; }
  bool isAnimatingCSSProperty() const {
    return m_cssProperty != CSSPropertyInvalid;
  }

  DECLARE_TRACE();

 private:
  SVGPropertyBase* createPropertyForAttributeAnimation(const String&) const;
  SVGPropertyBase* createPropertyForCSSAnimation(const String&) const;

  Member<SVGAnimationElement> m_animationElement;
  Member<SVGAnimatedPropertyBase> m_animatedProperty;
  AnimatedPropertyType m_type;
  CSSPropertyID m_cssProperty;
};

}  // namespace blink

#endif  // SVGAnimatedTypeAnimator_h
